#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "pj_drone_core/ardupilot/parser.hpp"

using namespace pj_drone::ardupilot;

namespace {

// ---- synthetic BIN builder -------------------------------------------------------------------------------------

template <typename T>
void put(std::vector<uint8_t>& out, T v) {
  const auto* p = reinterpret_cast<const uint8_t*>(&v);
  out.insert(out.end(), p, p + sizeof(T));
}

void putStr(std::vector<uint8_t>& out, const std::string& s, size_t width) {
  for (size_t i = 0; i < width; i++) {
    out.push_back(i < s.size() ? static_cast<uint8_t>(s[i]) : uint8_t{0});
  }
}

void header(std::vector<uint8_t>& out, uint8_t msgid) {
  out.push_back(0xA3);
  out.push_back(0x95);
  out.push_back(msgid);
}

void fmt(std::vector<uint8_t>& out, uint8_t type, uint8_t len, const std::string& name, const std::string& format,
         const std::string& labels) {
  header(out, 128);
  out.push_back(type);
  out.push_back(len);
  putStr(out, name, 4);
  putStr(out, format, 16);
  putStr(out, labels, 64);
}

struct Group {
  std::string name;
  std::vector<FieldSpec> fields;
};
struct Row {
  size_t group;
  int64_t ts_ns;
  std::vector<SampleValue> values;
};

class RecordingSink : public SampleSink {
 public:
  size_t declareGroup(const std::string& name, const std::vector<FieldSpec>& fields) override {
    groups.push_back({name, fields});
    return groups.size() - 1;
  }
  bool append(size_t group, int64_t ts_ns, const SampleValue* values, size_t count) override {
    rows.push_back({group, ts_ns, std::vector<SampleValue>(values, values + count)});
    return !fail;
  }
  std::vector<Group> groups;
  std::vector<Row> rows;
  bool fail = false;
};

// ATT: type 130, "Qff" TimeUS,Roll,Pitch  (3 + 8 + 4 + 4 = 19 bytes)
void defineAtt(std::vector<uint8_t>& out) {
  fmt(out, 130, 19, "ATT", "Qff", "TimeUS,Roll,Pitch");
}
void att(std::vector<uint8_t>& out, uint64_t us, float roll, float pitch) {
  header(out, 130);
  put(out, us);
  put(out, roll);
  put(out, pitch);
}

}  // namespace

TEST(ArdupilotParser, DecodesNativeTypesAndNanosecondTimestamps) {
  std::vector<uint8_t> log;
  defineAtt(log);
  att(log, 1000000, 1.5F, -2.5F);
  att(log, 1020000, 2.5F, -3.5F);

  RecordingSink sink;
  Parser parser(log.data(), log.size(), {}, &sink);

  ASSERT_EQ(parser.result(), Parser::Result::kOk);
  ASSERT_EQ(sink.groups.size(), 1U);
  EXPECT_EQ(sink.groups[0].name, "ATT");
  ASSERT_EQ(sink.groups[0].fields.size(), 3U);
  EXPECT_EQ(sink.groups[0].fields[0].name, "TimeUS");
  EXPECT_EQ(sink.groups[0].fields[0].type, ValueType::kUint64);
  EXPECT_EQ(sink.groups[0].fields[1].type, ValueType::kFloat32);

  ASSERT_EQ(sink.rows.size(), 2U);
  EXPECT_EQ(sink.rows[0].ts_ns, 1000000000);
  EXPECT_EQ(sink.rows[1].ts_ns, 1020000000);
  EXPECT_EQ(std::get<uint64_t>(sink.rows[0].values[0]), 1000000U);
  EXPECT_FLOAT_EQ(std::get<float>(sink.rows[0].values[1]), 1.5F);
  EXPECT_FLOAT_EQ(std::get<float>(sink.rows[1].values[2]), -3.5F);
  EXPECT_EQ(parser.totalSamples(), 6U);
  EXPECT_EQ(parser.seriesCount(), 3U);
  ASSERT_EQ(parser.messageStats().size(), 1U);
  EXPECT_EQ(parser.messageStats()[0].count, 2U);
}

TEST(ArdupilotParser, ImplicitFormatScalingIsDeliveredAsDouble) {
  // "QcL": c -> x1e-2, L -> x1e-7 when no FMTU says otherwise.  3 + 8 + 2 + 4 = 17
  std::vector<uint8_t> log;
  fmt(log, 131, 17, "SCL", "QcL", "TimeUS,Cs,Lat");
  header(log, 131);
  put<uint64_t>(log, 5);
  put<int16_t>(log, 250);
  put<int32_t>(log, 473000000);

  RecordingSink sink;
  Parser parser(log.data(), log.size(), {}, &sink);

  ASSERT_EQ(sink.rows.size(), 1U);
  EXPECT_EQ(sink.groups[0].fields[1].type, ValueType::kFloat64);
  EXPECT_EQ(sink.groups[0].fields[2].type, ValueType::kFloat64);
  EXPECT_DOUBLE_EQ(std::get<double>(sink.rows[0].values[1]), 2.5);
  EXPECT_DOUBLE_EQ(std::get<double>(sink.rows[0].values[2]), 47.3);
}

TEST(ArdupilotParser, FmtuUnitsAndMultipliersApply) {
  std::vector<uint8_t> log;
  // UNIT: "QbZ" TimeUS,Id,Label (3+8+1+64 = 76); FMTU: "QBNN" TimeUS,FmtType,UnitIds,MultIds (3+8+1+16+16 = 44)
  fmt(log, 200, 76, "UNIT", "QbZ", "TimeUS,Id,Label");
  fmt(log, 201, 44, "FMTU", "QBNN", "TimeUS,FmtType,UnitIds,MultIds");
  header(log, 200);
  put<uint64_t>(log, 0);
  put<int8_t>(log, 'm');
  putStr(log, "m", 64);

  // XYZ: "QH" TimeUS,Val (3 + 8 + 2 = 13), unit ids "sm", mult ids "F2" (1e-6, 1e2)
  fmt(log, 140, 13, "XYZ", "QH", "TimeUS,Val");
  header(log, 201);
  put<uint64_t>(log, 0);
  put<uint8_t>(log, 140);
  putStr(log, "sm", 16);
  putStr(log, "F2", 16);

  header(log, 140);
  put<uint64_t>(log, 2000000);
  put<uint16_t>(log, 5);

  RecordingSink sink;
  Parser parser(log.data(), log.size(), {}, &sink);

  ASSERT_EQ(sink.groups.size(), 1U);
  EXPECT_EQ(sink.groups[0].fields[1].unit, "m");
  ASSERT_EQ(sink.rows.size(), 1U);
  EXPECT_DOUBLE_EQ(std::get<double>(sink.rows[0].values[0]), 2.0);  // TimeUS x 1e-6
  EXPECT_DOUBLE_EQ(std::get<double>(sink.rows[0].values[1]), 500.0);
  EXPECT_EQ(sink.rows[0].ts_ns, 2000000000);  // timestamp keeps full precision regardless of scaling
}

TEST(ArdupilotParser, InstancesGetOwnGroups) {
  // "QBf" TimeUS,I,V (3 + 8 + 1 + 4 = 16)
  std::vector<uint8_t> log;
  fmt(log, 150, 16, "GPS", "QBf", "TimeUS,I,V");
  const uint8_t instances[] = {0, 1, 0};
  for (const uint8_t inst : instances) {
    header(log, 150);
    put<uint64_t>(log, 10);
    put<uint8_t>(log, inst);
    put<float>(log, 1.0F);
  }

  {
    RecordingSink sink;
    Parser parser(log.data(), log.size(), {}, &sink);
    ASSERT_EQ(sink.groups.size(), 2U);
    EXPECT_EQ(sink.groups[0].name, "GPS/0");
    EXPECT_EQ(sink.groups[1].name, "GPS/1");
    EXPECT_EQ(sink.rows.size(), 3U);
    EXPECT_EQ(sink.rows[2].group, 0U);
  }
  {
    ParserOptions options;
    options.hash_instance = true;
    RecordingSink sink;
    Parser parser(log.data(), log.size(), options, &sink);
    EXPECT_EQ(sink.groups[0].name, "GPS/#0");
  }
}

TEST(ArdupilotParser, CollectsParametersMessagesAndFilesWithoutSink) {
  std::vector<uint8_t> log;
  // PARM "QNf" (3 + 8 + 16 + 4 = 31), MSG "QZ" (3 + 8 + 64 = 75),
  // FILE "NZBI"... use a compact layout: "NIBZ" FileName,Offset,Length,Data (3 + 16 + 4 + 1 + 64 = 88)
  fmt(log, 160, 31, "PARM", "QNf", "TimeUS,Name,Value");
  fmt(log, 161, 75, "MSG", "QZ", "TimeUS,Message");
  fmt(log, 162, 88, "FILE", "NIBZ", "FileName,Offset,Length,Data");

  header(log, 160);
  put<uint64_t>(log, 1);
  putStr(log, "ATC_RAT_P", 16);
  put<float>(log, 0.5F);
  header(log, 160);
  put<uint64_t>(log, 2);
  putStr(log, "ATC_RAT_P", 16);
  put<float>(log, 0.75F);

  header(log, 161);
  put<uint64_t>(log, 1500000);
  putStr(log, "Arming checks passed", 64);

  header(log, 162);
  putStr(log, "@SYS/x.txt", 16);
  put<uint32_t>(log, 0);
  put<uint8_t>(log, 5);
  putStr(log, "hello", 64);

  Parser parser(log.data(), log.size());

  ASSERT_EQ(parser.parameters().size(), 1U);
  EXPECT_EQ(parser.parameters()[0].name, "ATC_RAT_P");
  EXPECT_DOUBLE_EQ(parser.parameters()[0].value, 0.75);  // last value wins

  ASSERT_EQ(parser.logMessages().size(), 1U);
  EXPECT_DOUBLE_EQ(parser.logMessages()[0].timestamp, 1.5);
  EXPECT_EQ(parser.logMessages()[0].message, "Arming checks passed");

  ASSERT_EQ(parser.embeddedFiles().size(), 1U);
  EXPECT_EQ(parser.embeddedFiles()[0].name, "@SYS/x.txt");
  EXPECT_EQ(std::string(parser.embeddedFiles()[0].data.begin(), parser.embeddedFiles()[0].data.end()), "hello");
  EXPECT_EQ(parser.totalSamples(), 0U);
}

TEST(ArdupilotParser, FilesAreSkippedWhenDisabled) {
  std::vector<uint8_t> log;
  fmt(log, 162, 88, "FILE", "NIBZ", "FileName,Offset,Length,Data");
  header(log, 162);
  putStr(log, "a.bin", 16);
  put<uint32_t>(log, 0);
  put<uint8_t>(log, 1);
  putStr(log, "x", 64);

  ParserOptions options;
  options.load_files = false;
  Parser parser(log.data(), log.size(), options);
  EXPECT_TRUE(parser.embeddedFiles().empty());
}

TEST(ArdupilotParser, TruncatedLogKeepsCompletePackets) {
  std::vector<uint8_t> log;
  defineAtt(log);
  att(log, 1, 1.0F, 2.0F);
  att(log, 2, 1.0F, 2.0F);
  log.resize(log.size() - 5);  // cut the last packet

  RecordingSink sink;
  Parser parser(log.data(), log.size(), {}, &sink);
  EXPECT_EQ(parser.result(), Parser::Result::kOk);
  EXPECT_EQ(sink.rows.size(), 1U);
}

TEST(ArdupilotParser, DefinitionLongerThanItsLengthIsSkippedNotOverRead) {
  // Declares 3 + 4 = 7 bytes but the fields need 8 + 4.
  std::vector<uint8_t> log;
  fmt(log, 170, 7, "BAD", "Qf", "TimeUS,V");
  header(log, 170);
  put<uint32_t>(log, 1);
  defineAtt(log);
  att(log, 9, 1.0F, 2.0F);

  RecordingSink sink;
  Parser parser(log.data(), log.size(), {}, &sink);
  ASSERT_EQ(sink.rows.size(), 1U);
  EXPECT_EQ(sink.groups[sink.rows[0].group].name, "ATT");
}

TEST(ArdupilotParser, CancelAndSinkErrorStopParsingButKeepWhatWasRead) {
  std::vector<uint8_t> log;
  defineAtt(log);
  for (uint64_t i = 0; i < 300; i++) {
    att(log, i, 0.0F, 0.0F);
  }

  {
    ParserOptions options;
    int calls = 0;
    options.progress = [&](size_t, size_t) { return ++calls < 10; };
    RecordingSink sink;
    Parser parser(log.data(), log.size(), options, &sink);
    EXPECT_EQ(parser.result(), Parser::Result::kCancelled);
    EXPECT_GT(sink.rows.size(), 0U);
    EXPECT_LT(sink.rows.size(), 300U);
  }
  {
    RecordingSink sink;
    sink.fail = true;
    Parser parser(log.data(), log.size(), {}, &sink);
    EXPECT_EQ(parser.result(), Parser::Result::kSinkError);
    EXPECT_EQ(sink.rows.size(), 1U);
  }
}

TEST(ArdupilotParser, GarbageIsSkippedUntilNextHeader) {
  std::vector<uint8_t> log = {0x00, 0x11, 0xA3, 0x00, 0x42};
  defineAtt(log);
  att(log, 4, 1.0F, 2.0F);

  RecordingSink sink;
  Parser parser(log.data(), log.size(), {}, &sink);
  EXPECT_EQ(sink.rows.size(), 1U);
}
