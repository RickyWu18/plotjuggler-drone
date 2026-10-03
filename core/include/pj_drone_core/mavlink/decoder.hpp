/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace pj_drone::mavlink {

enum class ValueType { kInt8, kUint8, kInt16, kUint16, kInt32, kUint32, kInt64, kUint64, kFloat32, kFloat64 };

using Value = std::variant<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double>;

struct FieldSpec {
  std::string name;  // "roll" or, for array fields, "covariance.3"
  ValueType type;
};

/// Numeric layout of one MAVLink message id. Char fields are not part of it.
using Schema = std::vector<FieldSpec>;

struct Message {
  uint8_t sysid = 0;
  uint8_t compid = 0;
  uint32_t msgid = 0;
  std::string topic;  // "mav/<sysid>.<compid>/<MSG_NAME>"
  std::shared_ptr<const Schema> schema;
  std::vector<Value> values;  // parallel to *schema
  /// Hardware time embedded in the payload (`time_usec` or `time_boot_ms`), in nanoseconds. Empty when the message
  /// has none or it is zero; callers then use the receive time.
  std::optional<int64_t> embedded_timestamp_ns;
};

/// Stateful MAVLink byte-stream parser (wraps mavlink_parse_char on channel 0, as the PJ3 plugin did).
/// The MAVLink library keeps its framing state per channel, globally: only one StreamParser may be active at a time.
class StreamParser {
 public:
  StreamParser();
  ~StreamParser();
  StreamParser(const StreamParser&) = delete;
  StreamParser& operator=(const StreamParser&) = delete;

  /// Drops any partially received frame.
  void reset();

  /// Feeds raw bytes; every complete frame with a known message id is appended to `out`.
  void feed(const uint8_t* data, size_t size, std::vector<Message>& out);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace pj_drone::mavlink
