#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "pj_drone_core/ardupilot/log_tools.hpp"

using namespace pj_drone::ardupilot;

TEST(LogTools, FormatsValuesLikeTheOriginalPlugin) {
  EXPECT_EQ(formatParamValue(0.5), "0.5");
  EXPECT_EQ(formatParamValue(1234567.891), "1234567.9");
  EXPECT_EQ(formatMessageTimestamp(1.5), "1.500000");
}

TEST(LogTools, ParamFilterIsCaseInsensitiveRegexWithSubstringFallback) {
  EXPECT_TRUE(ParamFilter("").matches("ATC_RAT_P"));
  EXPECT_TRUE(ParamFilter("^atc").matches("ATC_RAT_P"));
  EXPECT_FALSE(ParamFilter("^RAT").matches("ATC_RAT_P"));
  EXPECT_TRUE(ParamFilter("RAT_.*P").matches("ATC_RAT_P"));
  // "(" is not a valid regex: falls back to a plain substring match.
  EXPECT_TRUE(ParamFilter("(").matches("A(B"));
  EXPECT_FALSE(ParamFilter("(").matches("AB"));
}

TEST(LogTools, FilterSortsByNameAndBuildsParamFile) {
  const std::vector<Parameter> params = {{"B_PARAM", 2.0}, {"A_PARAM", 0.25}, {"C_OTHER", 3.0}};
  const auto filtered = filterParameters(params, ParamFilter("PARAM"));
  ASSERT_EQ(filtered.size(), 2U);
  EXPECT_EQ(filtered[0].name, "A_PARAM");
  EXPECT_EQ(buildParamFileText(filtered), "A_PARAM,0.25\nB_PARAM,2\n");
}

namespace {

namespace fs = std::filesystem;

void putChunk(std::vector<uint8_t>& out, const std::string& name, uint32_t offset, const std::string& data) {
  const auto put_str = [&out](const std::string& s, size_t width) {
    for (size_t i = 0; i < width; i++) {
      out.push_back(i < s.size() ? static_cast<uint8_t>(s[i]) : uint8_t{0});
    }
  };
  out.insert(out.end(), {0xA3, 0x95, 162});
  put_str(name, 16);
  for (int i = 0; i < 4; i++) {
    out.push_back(static_cast<uint8_t>(offset >> (8 * i)));
  }
  out.push_back(static_cast<uint8_t>(data.size()));
  put_str(data, 64);
}

// A log whose FILE format is defined, followed by the given (name, content) files.
std::vector<uint8_t> buildLog(const std::vector<std::pair<std::string, std::string>>& files) {
  std::vector<uint8_t> log = {0xA3, 0x95, 128, 162, 88};
  const auto put_str = [&log](const std::string& s, size_t width) {
    for (size_t i = 0; i < width; i++) {
      log.push_back(i < s.size() ? static_cast<uint8_t>(s[i]) : uint8_t{0});
    }
  };
  put_str("FILE", 4);
  put_str("NIBZ", 16);
  put_str("FileName,Offset,Length,Data", 64);
  for (const auto& [name, content] : files) {
    putChunk(log, name, 0, content);
  }
  return log;
}

void writeBytes(const fs::path& path, const std::vector<uint8_t>& bytes) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::string readAll(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream content;
  content << in.rdbuf();
  return content.str();
}

class ExportFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    root = fs::temp_directory_path() / "pj_drone_core_export_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "out");
    log_path = root / "flight.bin";
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(root, ec);
  }

  std::vector<EmbeddedFile> scan(const std::vector<std::pair<std::string, std::string>>& files) {
    const auto log = buildLog(files);
    writeBytes(log_path, log);
    Parser parser(log.data(), log.size());
    return parser.embeddedFiles();
  }

  fs::path root;
  fs::path log_path;
};

}  // namespace

TEST_F(ExportFixture, ExportKeepsSubdirectories) {
  const auto files = scan({{"@SYS/crash.bin", "abc"}});
  const auto result = exportEmbeddedFiles(log_path, {&files[0]}, root / "out", OverwritePolicy::kOverwrite);
  EXPECT_EQ(result.exported, 1U);
  EXPECT_TRUE(result.failed.empty());
  EXPECT_EQ(readAll(root / "out" / "@SYS" / "crash.bin"), "abc");
  EXPECT_FALSE(fs::exists(root / "out" / "@SYS" / "crash.bin.part"));
}

TEST(LogTools, ResolveExportPathRejectsUnsafeNames) {
  const auto dir = fs::temp_directory_path() / "pj_drone_core_resolve_test";
  EXPECT_TRUE(resolveExportPath(dir, "@SYS/a.txt").has_value());
  EXPECT_TRUE(resolveExportPath(dir, "plain.bin").has_value());

  for (const char* bad : {"", "../evil.bin", "a/../../evil.bin", "..", ".", "/abs.bin", "\\abs.bin", "a\\..\\..\\x",
                          "C:\\evil.bin", "C:evil.bin", "a:stream", "dir/", "a//b", "con", "NUL.txt", "lpt1", "x.",
                          "x ", "a<b", "a|b", "a?b", "a*b", "tab\there"}) {
    EXPECT_FALSE(resolveExportPath(dir, bad).has_value()) << bad;
  }
  EXPECT_FALSE(resolveExportPath(dir, std::filesystem::absolute("evil_abs.bin").string()).has_value());
  EXPECT_FALSE(resolveExportPath(dir, std::string("nul\0x", 5)).has_value());
}

TEST_F(ExportFixture, UnsafeNameIsReportedAndNothingEscapes) {
  const auto files = scan({{"../evil.bin", "x"}, {"ok.bin", "y"}});
  std::vector<const EmbeddedFile*> ptrs;
  for (const auto& f : files) {
    ptrs.push_back(&f);
  }
  const auto result = exportEmbeddedFiles(log_path, ptrs, root / "out", OverwritePolicy::kOverwrite);
  EXPECT_EQ(result.exported, 1U);
  ASSERT_EQ(result.failed.size(), 1U);
  EXPECT_EQ(result.failed[0].name, "../evil.bin");
  EXPECT_FALSE(fs::exists(root / "evil.bin"));
}

TEST_F(ExportFixture, ExistingFileIsOverwrittenOrSkippedAccordingToPolicy) {
  const auto files = scan({{"a.bin", "new"}});
  writeBytes(root / "out" / "a.bin", {'o', 'l', 'd'});

  EXPECT_EQ(findExportConflicts({&files[0]}, root / "out"), std::vector<std::string>{"a.bin"});

  auto skipped = exportEmbeddedFiles(log_path, {&files[0]}, root / "out", OverwritePolicy::kSkipExisting);
  EXPECT_EQ(skipped.exported, 0U);
  EXPECT_EQ(skipped.skipped, std::vector<std::string>{"a.bin"});
  EXPECT_EQ(readAll(root / "out" / "a.bin"), "old");

  auto overwritten = exportEmbeddedFiles(log_path, {&files[0]}, root / "out", OverwritePolicy::kOverwrite);
  EXPECT_EQ(overwritten.exported, 1U);
  EXPECT_EQ(readAll(root / "out" / "a.bin"), "new");
}

TEST_F(ExportFixture, ExistingPartFileOfTheUserIsLeftAlone) {
  const auto files = scan({{"a.bin", "data"}});
  writeBytes(root / "out" / "a.bin.part", {'m', 'i', 'n', 'e'});

  const auto result = exportEmbeddedFiles(log_path, {&files[0]}, root / "out", OverwritePolicy::kOverwrite);
  EXPECT_EQ(result.exported, 1U);
  EXPECT_EQ(readAll(root / "out" / "a.bin"), "data");
  EXPECT_EQ(readAll(root / "out" / "a.bin.part"), "mine");
  EXPECT_FALSE(fs::exists(root / "out" / "a.bin.1.part"));
}

TEST_F(ExportFixture, ChangedLogIsRefused) {
  const auto files = scan({{"a.bin", "first"}});
  writeBytes(log_path, buildLog({{"a.bin", "other"}}));  // same name, different content

  const auto result = exportEmbeddedFiles(log_path, {&files[0]}, root / "out", OverwritePolicy::kOverwrite);
  EXPECT_EQ(result.exported, 0U);
  ASSERT_EQ(result.failed.size(), 1U);
  EXPECT_FALSE(fs::exists(root / "out" / "a.bin"));
}

TEST_F(ExportFixture, FileWithMissingChunksIsNotExported) {
  auto log = buildLog({});
  putChunk(log, "gap.bin", 0, "aaaa");
  putChunk(log, "gap.bin", 100, "bbbb");  // bytes 4..100 were never logged
  putChunk(log, "ok.bin", 0, "fine");
  writeBytes(log_path, log);
  Parser parser(log.data(), log.size());
  const auto& files = parser.embeddedFiles();
  ASSERT_EQ(files.size(), 2U);

  const auto result = exportEmbeddedFiles(log_path, {&files[0], &files[1]}, root / "out", OverwritePolicy::kOverwrite);
  EXPECT_EQ(result.exported, 1U);
  ASSERT_EQ(result.failed.size(), 1U);
  EXPECT_EQ(result.failed[0].name, "gap.bin");
  EXPECT_FALSE(fs::exists(root / "out" / "gap.bin"));
  EXPECT_FALSE(fs::exists(root / "out" / "gap.bin.part"));
  EXPECT_EQ(readAll(root / "out" / "ok.bin"), "fine");
}

TEST(LogTools, CheckEmbeddedFileClassifiesFiles) {
  EmbeddedFile file;
  file.name = "@SYS/ok.txt";
  file.size = file.estimated_size = file.covered_bytes = 10;
  EXPECT_EQ(checkEmbeddedFile(file).status, FileStatus::kOk);

  auto overlapping = file;
  overlapping.overlap_bytes = 2;
  EXPECT_EQ(checkEmbeddedFile(overlapping).status, FileStatus::kWarning);

  auto mismatched = file;
  mismatched.estimated_size = 20;
  EXPECT_EQ(checkEmbeddedFile(mismatched).status, FileStatus::kWarning);

  auto gappy = file;
  gappy.gap_bytes = 4;
  gappy.overlap_bytes = 2;
  const auto gappy_check = checkEmbeddedFile(gappy);
  EXPECT_EQ(gappy_check.status, FileStatus::kCannotExport);
  EXPECT_EQ(gappy_check.details.size(), 2U);  // the blocking finding and the warning

  auto unsafe = file;
  unsafe.name = "../evil.txt";
  EXPECT_EQ(checkEmbeddedFile(unsafe).status, FileStatus::kCannotExport);
}

TEST_F(ExportFixture, SizeMismatchIsReportedAsWarning) {
  auto files = scan({{"a.bin", "abc"}});
  EXPECT_TRUE(embeddedFileWarnings(files[0]).empty());

  files[0].estimated_size = 10;  // as if the FILE offsets promised more than the chunks delivered
  EXPECT_EQ(embeddedFileWarnings(files[0]).size(), 1U);
}

TEST(LogTools, SanitizeFileNameEscapesNonPrintableBytesAndPercent) {
  EXPECT_EQ(sanitizeFileName("@SYS/crash.bin"), "@SYS/crash.bin");
  EXPECT_EQ(sanitizeFileName(std::string("a\xFF" "b", 3)), "a%FFb");
  EXPECT_EQ(sanitizeFileName("100%"), "100%25");
  EXPECT_EQ(sanitizeFileName("tab\there\x7F"), "tab%09here%7F");
  // '%' is escaped too, so an escape sequence in the log cannot be confused with a sanitized byte.
  EXPECT_NE(sanitizeFileName("a%FF"), sanitizeFileName("a\xFF"));
}

TEST_F(ExportFixture, ScannedNamesAreSanitizedAndExportedUnderTheSanitizedName) {
  const auto files = scan({{"bad\xFF.bin", "x"}, {"50%.bin", "y"}});
  ASSERT_EQ(files.size(), 2U);
  EXPECT_EQ(files[0].name, "50%25.bin");
  EXPECT_EQ(files[0].raw_name, "50%.bin");
  EXPECT_EQ(files[1].name, "bad%FF.bin");
  EXPECT_EQ(files[1].raw_name, "bad\xFF.bin");
  // Displayed names are pure printable ASCII, so they can always be serialized as JSON / UTF-8.
  for (const auto& f : files) {
    for (const char ch : f.name) {
      EXPECT_TRUE(ch >= 0x20 && ch <= 0x7E);
    }
  }

  const auto result = exportEmbeddedFiles(log_path, {&files[0], &files[1]}, root / "out", OverwritePolicy::kOverwrite);
  EXPECT_EQ(result.exported, 2U);
  EXPECT_TRUE(result.failed.empty());
  EXPECT_EQ(readAll(root / "out" / "50%25.bin"), "y");
  EXPECT_EQ(readAll(root / "out" / "bad%FF.bin"), "x");
}

TEST_F(ExportFixture, ChangedContentUnderAnUnprintableNameIsRefused) {
  const auto files = scan({{"a\xFF.bin", "first"}});
  writeBytes(log_path, buildLog({{"a\xFF.bin", "other"}}));
  const auto result = exportEmbeddedFiles(log_path, {&files[0]}, root / "out", OverwritePolicy::kOverwrite);
  EXPECT_EQ(result.exported, 0U);
  EXPECT_EQ(result.failed.size(), 1U);
}

TEST_F(ExportFixture, FilesWithTheSameTargetPathAreReportedNotOverwritten) {
  // Both separators split path components, so these two different names resolve to one target.
  const auto files = scan({{"dir/x.bin", "one"}, {"dir\\x.bin", "two"}, {"ok.bin", "fine"}});
  ASSERT_EQ(files.size(), 3U);
  std::vector<const EmbeddedFile*> ptrs;
  for (const auto& f : files) {
    ptrs.push_back(&f);
  }
  const auto result = exportEmbeddedFiles(log_path, ptrs, root / "out", OverwritePolicy::kOverwrite);
  EXPECT_EQ(result.exported, 1U);
  EXPECT_EQ(result.failed.size(), 2U);
  EXPECT_FALSE(fs::exists(root / "out" / "dir" / "x.bin"));
  EXPECT_EQ(readAll(root / "out" / "ok.bin"), "fine");
}
