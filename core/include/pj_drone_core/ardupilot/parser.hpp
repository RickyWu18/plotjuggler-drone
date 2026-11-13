// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace pj_drone::ardupilot {

/// A decoded numeric sample in its native type. A field whose unit multiplier is not 1 (MULT / FMTU, or the
/// implicit scaling of the 'c' 'C' 'e' 'E' 'L' format characters) is delivered as double.
using SampleValue = std::variant<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double>;

enum class ValueType { kInt8, kUint8, kInt16, kUint16, kInt32, kUint32, kInt64, kUint64, kFloat32, kFloat64 };

struct FieldSpec {
  std::string name;
  ValueType type = ValueType::kFloat64;
  std::string unit;  // resolved when the group is declared; empty when the log gives none
};

/// Receives decoded data. Each group is one message type (or one message type + instance); its fields are the
/// message's numeric fields in log order (string and array fields are not delivered).
class SampleSink {
 public:
  virtual ~SampleSink() = default;

  /// `name` is "ATT", or "GPS/0" ("GPS/#0" with ParserOptions::hash_instance) for multi-instance messages.
  virtual size_t declareGroup(const std::string& name, const std::vector<FieldSpec>& fields) = 0;

  /// `values` holds one entry per declared field. Return false to abort parsing.
  virtual bool append(size_t group, int64_t timestamp_ns, const SampleValue* values, size_t count) = 0;
};

struct Parameter {
  std::string name;
  double value = 0.0;
};

/// One FILE message of an embedded file. The payload stays in the log; only its position is recorded.
struct FileChunk {
  uint32_t offset = 0;         // position of the chunk inside the embedded file
  uint8_t declared_length = 0; // the FILE message's Length field
  uint8_t length = 0;          // bytes actually stored (min of Length, 64 and the Data field size)
  size_t log_pos = 0;          // position of the chunk's first byte inside the log
};

/// An embedded file as described by the log's FILE messages. The content is not assembled here: it is produced
/// on demand by streamEmbeddedFile() so the scan keeps no copy of it in memory.
struct EmbeddedFile {
  std::string name;
  std::vector<FileChunk> chunks;  // log order
  size_t estimated_size = 0;      // max(offset + declared Length) over the chunks
  size_t size = 0;                // end of the assembled content (what streamEmbeddedFile() produces)
  size_t covered_bytes = 0;       // bytes backed by chunk data (size - gap_bytes)
  size_t gap_bytes = 0;           // bytes inside `size` that no chunk provides; a file with gaps is not exportable
  size_t overlap_bytes = 0;       // bytes provided by more than one chunk (the later chunk wins)
  std::string sha256;             // hex digest of the assembled content, taken at scan time
};

/// A run of the assembled content: either bytes copied from the log or a zero-filled gap.
struct FileSegment {
  size_t file_offset = 0;
  size_t length = 0;
  size_t log_pos = 0;  // unused for gaps
  bool is_gap = false;
};

/// Resolves the chunks of `file` into ordered, non-overlapping segments. Where chunks overlap, the one that
/// appears later in the log wins. Also fills the size / coverage fields from the chunks.
std::vector<FileSegment> layoutEmbeddedFile(EmbeddedFile& file);

/// Feeds the assembled content of `file` to `write` in order, reading from the log bytes. Returns false when the
/// file has a gap (missing chunks are never replaced by invented bytes), a chunk lies outside the log, or `write`
/// returns false.
bool streamEmbeddedFile(const EmbeddedFile& file, const uint8_t* log, size_t log_length,
                        const std::function<bool(const uint8_t*, size_t)>& write);

/// SHA-256 (hex) over the file's FILE records as they appear in the log: the file name, the chunk count, then for
/// every chunk, in log order, its offset, declared Length, stored length and data. Every field has a fixed or
/// length-prefixed size, so different records never produce the same byte sequence. The cost follows the bytes the
/// log holds, not the size of any gap. Returns an empty string when a chunk lies outside the log.
std::string embeddedFileDigest(const EmbeddedFile& file, const uint8_t* log, size_t log_length);

struct LogMessage {
  double timestamp = 0.0;  // seconds since boot
  std::string message;
};

struct MessageStats {
  std::string name;
  uint64_t count = 0;
};

struct ParserOptions {
  using ProgressCallback = std::function<bool(size_t pos, size_t total)>;  // return false to cancel

  bool load_files = true;      // index embedded FILE messages (content is only assembled on export)
  bool hash_instance = false;  // "GPS/#0" instead of "GPS/0"
  ProgressCallback progress;
};

class Parser {
 public:
  enum class Result { kOk, kCancelled, kSinkError };

  /// Parses `data` immediately. With a null `sink` only metadata is collected (parameters, messages, files,
  /// statistics), which is much cheaper than a full decode.
  Parser(const uint8_t* data, size_t length, ParserOptions options = {}, SampleSink* sink = nullptr);

  Result result() const {
    return result_;
  }
  const std::vector<MessageStats>& messageStats() const {
    return stats_;
  }
  const std::vector<Parameter>& parameters() const {
    return params_;
  }
  const std::vector<EmbeddedFile>& embeddedFiles() const {
    return embedded_files_;
  }
  const std::vector<LogMessage>& logMessages() const {
    return msg_log_;
  }
  size_t totalSamples() const {
    return total_samples_;
  }
  size_t seriesCount() const {
    return series_count_;
  }

 private:
  static constexpr size_t kNoGroup = std::numeric_limits<size_t>::max();
  static constexpr size_t kMaxFields = 16;

  struct FieldDef {
    char fmt_char = 0;
    size_t byte_size = 0;
    size_t offset = 0;  // byte offset inside the payload
    bool is_string = false;
    bool is_array = false;
    std::string label;
    char unit_id = '?';
    char mult_id = '?';
    double mult_val = 1.0;  // cached scaling factor, set in finalizeDef()
    bool scaled = false;    // mult_val != 1.0 -> delivered as double
    ValueType type = ValueType::kFloat64;
  };

  struct MessageDef {
    uint8_t msg_type = 0;
    uint8_t msg_len = 0;
    std::string name;
    std::vector<FieldDef> fields;
    size_t payload_size = 0;  // sum of the fields' byte sizes
    int timestamp_idx = -1;
    int instance_idx = -1;
    std::vector<size_t> numeric_idx;  // indices of the fields that are delivered to the sink
    size_t group = kNoGroup;          // non-instance messages
    std::unordered_map<int, size_t> instance_groups;
    int stats_idx = -1;
    bool finalized = false;
  };

  struct FmtuPending {
    char units[16] = {};
    char multipliers[16] = {};
  };

  void parse();
  bool parseSinglePass();
  void finalizeDef(MessageDef& def);

  static MessageDef buildMessageDef(const uint8_t* payload86);
  static size_t fieldByteSize(char c);
  static bool isStringField(char c);
  static bool isArrayField(char c);
  static std::vector<std::string> splitLabels(const char* buf, size_t len);
  static double float16ToDouble(uint16_t bits);
  static ValueType nativeType(char fmt_char);
  static SampleValue decodeNative(const uint8_t* p, char fmt_char);
  static double decodeDouble(const uint8_t* p, char fmt_char);

  void applyFmtu(MessageDef& def, const char* units16, const char* mults16);
  void applyPendingFmtu(uint8_t msg_type);

  bool parseDataPacket(const uint8_t* payload, MessageDef& def);
  void parseUnitPacket(const uint8_t* payload, const MessageDef& def);
  void parseMultPacket(const uint8_t* payload, const MessageDef& def);
  void parseFmtuPacket(const uint8_t* payload, const MessageDef& def);
  void parseFilePacket(const uint8_t* payload, const MessageDef& def);
  void indexEmbeddedFiles();

  std::vector<FieldSpec> buildFieldSpecs(const MessageDef& def) const;

  const uint8_t* data_ = nullptr;
  size_t length_ = 0;
  ParserOptions options_;
  SampleSink* sink_ = nullptr;
  Result result_ = Result::kOk;

  std::array<MessageDef, 256> fmt_table_;
  bool fmt_valid_[256] = {};
  std::unordered_map<char, std::string> unit_table_;
  std::unordered_map<char, double> mult_table_;
  std::array<FmtuPending, 256> pending_fmtu_;
  bool pending_fmtu_valid_[256] = {};

  uint8_t unit_msg_type_ = 0;
  uint8_t mult_msg_type_ = 0;
  uint8_t fmtu_msg_type_ = 0;
  uint8_t file_msg_type_ = 0;

  uint64_t last_timestamp_us_ = 0;

  std::vector<MessageStats> stats_;
  size_t total_samples_ = 0;
  size_t series_count_ = 0;
  std::vector<Parameter> params_;
  std::unordered_map<std::string, size_t> params_index_;

  std::unordered_map<std::string, std::vector<FileChunk>> file_chunks_;
  std::vector<EmbeddedFile> embedded_files_;
  std::vector<LogMessage> msg_log_;
};

}  // namespace pj_drone::ardupilot
