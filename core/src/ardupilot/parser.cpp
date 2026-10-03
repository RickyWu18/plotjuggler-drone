/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "pj_drone_core/ardupilot/parser.hpp"

#include <algorithm>
#include <cstring>

namespace pj_drone::ardupilot {

namespace {

constexpr uint8_t kHeadByte1 = 0xA3;
constexpr uint8_t kHeadByte2 = 0x95;
constexpr uint8_t kFmtMsgId = 128;
constexpr size_t kFmtPayloadLen = 86;

// A corrupt FILE chunk offset must not make us allocate gigabytes.
constexpr size_t kMaxEmbeddedFileSize = size_t{256} * 1024 * 1024;

template <typename T>
T readLe(const uint8_t* p) {
  T v;
  std::memcpy(&v, p, sizeof(T));
  return v;
}

std::string readCString(const uint8_t* p, size_t max_len) {
  const char* s = reinterpret_cast<const char*>(p);
  return std::string(s, strnlen(s, max_len));
}

}  // namespace

Parser::Parser(const uint8_t* data, size_t length, ParserOptions options, SampleSink* sink)
    : data_(data), length_(length), options_(std::move(options)), sink_(sink) {
  // Seed built-in multiplier table so scaling works before MULT packets arrive
  mult_table_['?'] = 1.0;
  mult_table_['-'] = 1.0;  // "no units" — treat as identity rather than zero
  mult_table_['0'] = 1.0;
  mult_table_['A'] = 1e-1;
  mult_table_['B'] = 1e-2;
  mult_table_['C'] = 1e-3;
  mult_table_['D'] = 1e-4;
  mult_table_['E'] = 1e-5;
  mult_table_['F'] = 1e-6;
  mult_table_['G'] = 1e-7;
  mult_table_['I'] = 1e-9;
  mult_table_['1'] = 1e1;
  mult_table_['2'] = 1e2;
  mult_table_['!'] = 3.6;
  mult_table_['/'] = 3600.0;

  parse();
}

void Parser::parse() {
  if (!parseSinglePass()) {
    return;
  }
  if (options_.load_files) {
    assembleEmbeddedFiles();
  }
}

std::vector<FieldSpec> Parser::buildFieldSpecs(const MessageDef& def) const {
  std::vector<FieldSpec> specs;
  specs.reserve(def.numeric_idx.size());
  for (const size_t idx : def.numeric_idx) {
    const FieldDef& f = def.fields[idx];
    FieldSpec spec;
    spec.name = f.label;
    spec.type = f.type;
    if (f.unit_id != '?' && f.unit_id != '-') {
      const auto uit = unit_table_.find(f.unit_id);
      if (uit != unit_table_.end()) {
        spec.unit = uit->second;
      }
    }
    specs.push_back(std::move(spec));
  }
  return specs;
}

void Parser::finalizeDef(MessageDef& def) {
  def.finalized = true;

  const bool is_special = (def.msg_type == unit_msg_type_ || def.msg_type == mult_msg_type_ ||
                           def.msg_type == fmtu_msg_type_ || def.msg_type == file_msg_type_);

  // Cache mult_val on every field so the decode hot-path needs no map lookup
  for (auto& field : def.fields) {
    if (field.mult_id == '?') {
      switch (field.fmt_char) {
        case 'c':
        case 'C':
          field.mult_val = 1e-2;
          break;
        case 'e':
        case 'E':
          field.mult_val = 1e-4;
          break;
        case 'L':
          field.mult_val = 1e-7;
          break;
        default:
          field.mult_val = 1.0;
          break;
      }
    } else if (field.mult_id == '-') {
      field.mult_val = 1.0;
    } else {
      const auto it = mult_table_.find(field.mult_id);
      field.mult_val = (it != mult_table_.end() && it->second != 0.0) ? it->second : 1.0;
    }
    field.scaled = field.mult_val != 1.0;
    field.type = field.scaled ? ValueType::kFloat64 : nativeType(field.fmt_char);
  }

  if (is_special) {
    return;
  }

  if (def.timestamp_idx >= 0 && static_cast<size_t>(def.timestamp_idx) >= def.fields.size()) {
    def.timestamp_idx = -1;
  }

  def.numeric_idx.clear();
  for (size_t i = 0; i < def.fields.size(); i++) {
    if (!def.fields[i].is_string && !def.fields[i].is_array) {
      def.numeric_idx.push_back(i);
    }
  }

  // Init stats entry
  def.stats_idx = static_cast<int>(stats_.size());
  stats_.push_back({def.name, 0});

  // Non-instance messages get their group up front; instance messages on first encounter.
  if (sink_ && def.instance_idx < 0) {
    const auto specs = buildFieldSpecs(def);
    series_count_ += specs.size();
    def.group = sink_->declareGroup(def.name, specs);
  }
}

bool Parser::parseSinglePass() {
  size_t pos = 0;
  int last_percent = -1;

  while (pos + 3 <= length_) {
    if (data_[pos] != kHeadByte1 || data_[pos + 1] != kHeadByte2) {
      pos++;
      continue;
    }

    const uint8_t msgid = data_[pos + 2];
    pos += 3;

    if (msgid == kFmtMsgId) {
      if (pos + kFmtPayloadLen > length_) {
        break;
      }

      MessageDef def = buildMessageDef(data_ + pos);
      pos += kFmtPayloadLen;

      const uint8_t mtype = def.msg_type;
      fmt_table_[mtype] = std::move(def);
      fmt_valid_[mtype] = true;

      const auto& name = fmt_table_[mtype].name;
      if (name == "UNIT") {
        unit_msg_type_ = mtype;
      } else if (name == "MULT") {
        mult_msg_type_ = mtype;
      } else if (name == "FMTU") {
        fmtu_msg_type_ = mtype;
      } else if (name == "FILE") {
        file_msg_type_ = mtype;
      }

      applyPendingFmtu(mtype);
    } else {
      if (!fmt_valid_[msgid]) {
        break;
      }

      MessageDef& def = fmt_table_[msgid];
      if (def.msg_len < 3) {
        break;
      }

      const size_t payload_len = static_cast<size_t>(def.msg_len) - 3;
      if (pos + payload_len > length_) {
        break;
      }

      const uint8_t* payload = data_ + pos;
      pos += payload_len;

      // A definition whose fields do not fit its own declared length is corrupt: decoding it would read
      // past the packet.
      if (def.payload_size <= payload_len) {
        if (msgid == unit_msg_type_) {
          parseUnitPacket(payload, def);
        } else if (msgid == mult_msg_type_) {
          parseMultPacket(payload, def);
        } else if (msgid == fmtu_msg_type_) {
          parseFmtuPacket(payload, def);
        } else if (msgid == file_msg_type_) {
          if (options_.load_files) {
            parseFilePacket(payload, def);
          }
        } else {
          if (!def.finalized) {
            finalizeDef(def);
          }
          if (!parseDataPacket(payload, def)) {
            result_ = Result::kSinkError;
            return false;
          }
        }
      }
    }

    if (options_.progress) {
      const int pct = static_cast<int>(100.0 * static_cast<double>(pos) / static_cast<double>(length_));
      if (pct != last_percent) {
        last_percent = pct;
        if (!options_.progress(pos, length_)) {
          result_ = Result::kCancelled;
          return false;
        }
      }
    }
  }

  return true;
}

Parser::MessageDef Parser::buildMessageDef(const uint8_t* payload86) {
  MessageDef def;
  def.msg_type = payload86[0];
  def.msg_len = payload86[1];
  def.name = readCString(payload86 + 2, 4);

  const char* fmt_buf = reinterpret_cast<const char*>(payload86 + 6);
  const char* labels_buf = reinterpret_cast<const char*>(payload86 + 22);
  const auto labels = splitLabels(labels_buf, 64);

  size_t offset = 0;
  for (size_t i = 0; i < kMaxFields && fmt_buf[i] != '\0'; i++) {
    const char c = fmt_buf[i];
    FieldDef field;
    field.fmt_char = c;
    field.byte_size = fieldByteSize(c);
    field.offset = offset;
    field.is_string = isStringField(c);
    field.is_array = isArrayField(c);
    if (i < labels.size()) {
      field.label = labels[i];
    }

    if (c == 'Q' && field.label == "TimeUS") {
      def.timestamp_idx = static_cast<int>(i);
    }

    if ((c == 'B' || c == 'M') && (field.label == "I" || field.label == "Instance")) {
      def.instance_idx = static_cast<int>(i);
    }

    offset += field.byte_size;
    def.fields.push_back(std::move(field));
  }
  def.payload_size = offset;

  return def;
}

size_t Parser::fieldByteSize(char c) {
  switch (c) {
    case 'Q':
    case 'q':
    case 'd':
      return 8;
    case 'f':
    case 'I':
    case 'i':
    case 'L':
    case 'e':
    case 'E':
      return 4;
    case 'H':
    case 'h':
    case 'c':
    case 'C':
    case 'g':
      return 2;
    case 'B':
    case 'M':
    case 'b':
      return 1;
    case 'n':
      return 4;
    case 'N':
      return 16;
    case 'Z':
      return 64;
    case 'a':
      return 64;
    default:
      return 0;
  }
}

bool Parser::isStringField(char c) {
  return c == 'n' || c == 'N' || c == 'Z';
}

bool Parser::isArrayField(char c) {
  return c == 'a';
}

std::vector<std::string> Parser::splitLabels(const char* buf, size_t len) {
  std::vector<std::string> result;
  std::string token;
  for (size_t i = 0; i < len && buf[i] != '\0'; i++) {
    if (buf[i] == ',') {
      result.push_back(token);
      token.clear();
    } else {
      token += buf[i];
    }
  }
  if (!token.empty()) {
    result.push_back(token);
  }
  return result;
}

double Parser::float16ToDouble(uint16_t bits) {
  const uint32_t sign = (bits >> 15) & 0x1U;
  const uint32_t exponent = (bits >> 10) & 0x1FU;
  const uint32_t mantissa = bits & 0x3FFU;

  uint32_t f;
  if (exponent == 0) {
    if (mantissa == 0) {
      f = sign << 31;
    } else {
      int32_t e = 1;
      uint32_t m = mantissa;
      while (!(m & 0x400U)) {
        m <<= 1;
        e--;
      }
      m &= ~0x400U;
      f = (sign << 31) | (static_cast<uint32_t>(e + 127 - 15) << 23) | (m << 13);
    }
  } else if (exponent == 31) {
    f = (sign << 31) | (0xFFU << 23) | (mantissa << 13);
  } else {
    f = (sign << 31) | ((exponent + 127 - 15) << 23) | (mantissa << 13);
  }

  float result;
  std::memcpy(&result, &f, 4);
  return static_cast<double>(result);
}

ValueType Parser::nativeType(char fmt_char) {
  switch (fmt_char) {
    case 'Q':
      return ValueType::kUint64;
    case 'q':
      return ValueType::kInt64;
    case 'd':
      return ValueType::kFloat64;
    case 'f':
    case 'g':
      return ValueType::kFloat32;
    case 'I':
    case 'E':
      return ValueType::kUint32;
    case 'i':
    case 'L':
    case 'e':
      return ValueType::kInt32;
    case 'H':
    case 'C':
      return ValueType::kUint16;
    case 'h':
    case 'c':
      return ValueType::kInt16;
    case 'B':
    case 'M':
      return ValueType::kUint8;
    case 'b':
      return ValueType::kInt8;
    default:
      return ValueType::kFloat64;
  }
}

SampleValue Parser::decodeNative(const uint8_t* p, char fmt_char) {
  switch (fmt_char) {
    case 'Q':
      return readLe<uint64_t>(p);
    case 'q':
      return readLe<int64_t>(p);
    case 'd':
      return readLe<double>(p);
    case 'f':
      return readLe<float>(p);
    case 'g':
      return static_cast<float>(float16ToDouble(readLe<uint16_t>(p)));
    case 'I':
    case 'E':
      return readLe<uint32_t>(p);
    case 'i':
    case 'L':
    case 'e':
      return readLe<int32_t>(p);
    case 'H':
    case 'C':
      return readLe<uint16_t>(p);
    case 'h':
    case 'c':
      return readLe<int16_t>(p);
    case 'B':
    case 'M':
      return readLe<uint8_t>(p);
    case 'b':
      return readLe<int8_t>(p);
    default:
      return 0.0;
  }
}

double Parser::decodeDouble(const uint8_t* p, char fmt_char) {
  switch (fmt_char) {
    case 'Q':
      return static_cast<double>(readLe<uint64_t>(p));
    case 'q':
      return static_cast<double>(readLe<int64_t>(p));
    case 'd':
      return readLe<double>(p);
    case 'f':
      return static_cast<double>(readLe<float>(p));
    case 'I':
    case 'E':
      return static_cast<double>(readLe<uint32_t>(p));
    case 'i':
    case 'L':
    case 'e':
      return static_cast<double>(readLe<int32_t>(p));
    case 'H':
    case 'C':
      return static_cast<double>(readLe<uint16_t>(p));
    case 'h':
    case 'c':
      return static_cast<double>(readLe<int16_t>(p));
    case 'g':
      return float16ToDouble(readLe<uint16_t>(p));
    case 'B':
    case 'M':
      return static_cast<double>(readLe<uint8_t>(p));
    case 'b':
      return static_cast<double>(readLe<int8_t>(p));
    default:
      return 0.0;
  }
}

bool Parser::parseDataPacket(const uint8_t* payload, MessageDef& def) {
  // Extract PARM name/value before the generic numeric pass skips string fields
  if (def.name == "PARM") {
    std::string parm_name;
    double parm_value = 0.0;
    for (const auto& field : def.fields) {
      if (field.label == "Name" && field.is_string) {
        parm_name = readCString(payload + field.offset, field.byte_size);
      } else if (field.label == "Value" && field.fmt_char == 'f') {
        parm_value = static_cast<double>(readLe<float>(payload + field.offset));
      }
    }
    if (!parm_name.empty()) {
      const auto it = params_index_.find(parm_name);
      if (it != params_index_.end()) {
        params_[it->second].value = parm_value;
      } else {
        params_index_[parm_name] = params_.size();
        params_.push_back({parm_name, parm_value});
      }
    }
  }

  // Timestamp is read from the raw bytes to keep the full uint64 precision.
  if (def.timestamp_idx >= 0) {
    last_timestamp_us_ = readLe<uint64_t>(payload + def.fields[static_cast<size_t>(def.timestamp_idx)].offset);
  }
  const auto timestamp_ns = static_cast<int64_t>(last_timestamp_us_ * 1000U);

  // Extract MSG text and store with timestamp
  if (def.name == "MSG") {
    for (const auto& field : def.fields) {
      if (field.label == "Message" && field.is_string) {
        std::string text = readCString(payload + field.offset, field.byte_size);
        if (!text.empty()) {
          msg_log_.push_back({static_cast<double>(last_timestamp_us_) * 1e-6, std::move(text)});
        }
        break;
      }
    }
  }

  if (def.stats_idx >= 0) {
    stats_[static_cast<size_t>(def.stats_idx)].count++;
  }

  if (!sink_) {
    return true;
  }

  // Resolve the group: fixed for plain messages, per instance for multi-sensor ones.
  size_t group = def.group;
  if (def.instance_idx >= 0) {
    const int instance = payload[def.fields[static_cast<size_t>(def.instance_idx)].offset];
    auto it = def.instance_groups.find(instance);
    if (it == def.instance_groups.end()) {
      const auto specs = buildFieldSpecs(def);
      series_count_ += specs.size();
      const std::string name = def.name + "/" + (options_.hash_instance ? "#" : "") + std::to_string(instance);
      it = def.instance_groups.emplace(instance, sink_->declareGroup(name, specs)).first;
    }
    group = it->second;
  }

  // Decode every numeric field (string and array fields are skipped).
  std::array<SampleValue, kMaxFields> values;
  size_t count = 0;
  for (const size_t idx : def.numeric_idx) {
    const FieldDef& field = def.fields[idx];
    if (field.scaled) {
      values[count++] = decodeDouble(payload + field.offset, field.fmt_char) * field.mult_val;
    } else {
      values[count++] = decodeNative(payload + field.offset, field.fmt_char);
    }
  }

  total_samples_ += count;
  return sink_->append(group, timestamp_ns, values.data(), count);
}

void Parser::parseUnitPacket(const uint8_t* payload, const MessageDef& def) {
  char type_id = 0;
  std::string unit_str;

  for (const auto& field : def.fields) {
    if (field.label == "Id" && (field.fmt_char == 'b' || field.fmt_char == 'B')) {
      type_id = static_cast<char>(readLe<int8_t>(payload + field.offset));
    } else if (field.label == "Label" && field.is_string) {
      unit_str = readCString(payload + field.offset, field.byte_size);
    }
  }

  if (type_id != 0) {
    unit_table_[type_id] = unit_str;
  }
}

void Parser::parseMultPacket(const uint8_t* payload, const MessageDef& def) {
  char type_id = 0;
  double mult_val = 1.0;

  for (const auto& field : def.fields) {
    if (field.label == "Id" && (field.fmt_char == 'b' || field.fmt_char == 'B')) {
      type_id = static_cast<char>(readLe<int8_t>(payload + field.offset));
    } else if (field.label == "Mult" && field.fmt_char == 'd') {
      mult_val = readLe<double>(payload + field.offset);
    }
  }

  if (type_id != 0) {
    mult_table_[type_id] = mult_val;
  }
}

void Parser::parseFmtuPacket(const uint8_t* payload, const MessageDef& def) {
  uint8_t fmt_type = 0;
  char units[16] = {};
  char mults[16] = {};

  for (const auto& field : def.fields) {
    if (field.label == "FmtType") {
      if (field.fmt_char == 'B' || field.fmt_char == 'b') {
        fmt_type = payload[field.offset];
      } else if (field.fmt_char == 'H') {
        fmt_type = static_cast<uint8_t>(readLe<uint16_t>(payload + field.offset));
      }
    } else if (field.label == "UnitIds" && field.is_string) {
      std::memcpy(units, payload + field.offset, std::min(field.byte_size, size_t{16}));
    } else if (field.label == "MultIds" && field.is_string) {
      std::memcpy(mults, payload + field.offset, std::min(field.byte_size, size_t{16}));
    }
  }

  if (fmt_valid_[fmt_type]) {
    applyFmtu(fmt_table_[fmt_type], units, mults);
  } else {
    std::memcpy(pending_fmtu_[fmt_type].units, units, 16);
    std::memcpy(pending_fmtu_[fmt_type].multipliers, mults, 16);
    pending_fmtu_valid_[fmt_type] = true;
  }
}

void Parser::applyFmtu(MessageDef& def, const char* units16, const char* mults16) {
  const size_t n = std::min(def.fields.size(), size_t{16});
  for (size_t i = 0; i < n; i++) {
    if (units16[i] != '\0') {
      def.fields[i].unit_id = units16[i];
    }
    if (mults16[i] != '\0') {
      def.fields[i].mult_id = mults16[i];
    }
  }
}

void Parser::applyPendingFmtu(uint8_t msg_type) {
  if (!pending_fmtu_valid_[msg_type]) {
    return;
  }

  applyFmtu(fmt_table_[msg_type], pending_fmtu_[msg_type].units, pending_fmtu_[msg_type].multipliers);
  pending_fmtu_valid_[msg_type] = false;
}

void Parser::parseFilePacket(const uint8_t* payload, const MessageDef& def) {
  std::string filename;
  uint32_t offset = 0;
  uint8_t length = 0;
  const uint8_t* data_ptr = nullptr;
  size_t data_size = 0;

  for (const auto& field : def.fields) {
    if (field.label == "FileName" && field.is_string) {
      filename = readCString(payload + field.offset, field.byte_size);
    } else if (field.label == "Offset" && field.fmt_char == 'I') {
      offset = readLe<uint32_t>(payload + field.offset);
    } else if (field.label == "Length" && field.fmt_char == 'B') {
      length = payload[field.offset];
    } else if (field.label == "Data") {
      data_ptr = payload + field.offset;
      data_size = field.byte_size;
    }
  }

  if (filename.empty() || data_ptr == nullptr || length == 0) {
    return;
  }
  const size_t chunk_len = std::min({static_cast<size_t>(length), size_t{64}, data_size});

  file_chunks_[filename].emplace_back(offset, std::vector<uint8_t>(data_ptr, data_ptr + chunk_len));
}

void Parser::assembleEmbeddedFiles() {
  for (auto& [name, chunks] : file_chunks_) {
    std::sort(chunks.begin(), chunks.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    size_t total = 0;
    for (const auto& [chunk_offset, chunk_data] : chunks) {
      const size_t end = chunk_offset + chunk_data.size();
      total = std::max(total, end);
    }
    if (total > kMaxEmbeddedFileSize) {
      continue;
    }

    EmbeddedFile ef;
    ef.name = name;
    ef.data.resize(total, 0);
    for (const auto& [chunk_offset, chunk_data] : chunks) {
      std::copy(chunk_data.begin(), chunk_data.end(), ef.data.begin() + static_cast<std::ptrdiff_t>(chunk_offset));
    }

    embedded_files_.push_back(std::move(ef));
  }
}

}  // namespace pj_drone::ardupilot
