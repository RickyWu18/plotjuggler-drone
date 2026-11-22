// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace pj_drone::ardupilot {

/// A decoded numeric sample in its native type. A field whose unit multiplier is not 1 (MULT / FMTU, or the
/// implicit scaling of the 'c' 'C' 'e' 'E' 'L' format characters) is delivered as double. A string field
/// ('n' 'N' 'Z') is a view into the log: valid during append() only.
using SampleValue = std::variant<
    int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double, std::string_view>;

enum class ValueType { kInt8, kUint8, kInt16, kUint16, kInt32, kUint32, kInt64, kUint64, kFloat32, kFloat64, kString };

struct FieldSpec {
  std::string name;
  ValueType type = ValueType::kFloat64;
  std::string unit;  // resolved when the group is declared; empty when the log gives none
};

/// Receives decoded data. Each group is one message type (or one message type + instance); its fields are the
/// message's numeric and string fields in log order (array fields are not delivered).
class SampleSink {
 public:
  virtual ~SampleSink() = default;

  /// `name` is "ATT", or "GPS/0" ("GPS/#0" with ParserOptions::hash_instance) for multi-instance messages.
  virtual size_t declareGroup(const std::string& name, const std::vector<FieldSpec>& fields) = 0;

  /// `values` holds one entry per declared field. Return false to abort parsing.
  virtual bool append(size_t group, int64_t timestamp_ns, const SampleValue* values, size_t count) = 0;
};

}  // namespace pj_drone::ardupilot
