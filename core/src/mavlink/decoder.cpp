// SPDX-License-Identifier: MIT

#include "pj_drone_core/mavlink/decoder.hpp"

#include <cstring>
#include <unordered_map>

#define MAVLINK_USE_MESSAGE_INFO
#include "all/mavlink.h"

namespace pj_drone::mavlink {

namespace {

constexpr uint8_t kChannel = MAVLINK_COMM_0;
constexpr const char* kFieldTimeUsec = "time_usec";
constexpr const char* kFieldTimeBootMs = "time_boot_ms";
// Indexed by MAVLINK_TYPE_* (0=CHAR, 1=UINT8, 2=INT8, 3=UINT16, 4=INT16, 5=UINT32, 6=INT32, 7=UINT64, 8=INT64,
// 9=FLOAT, 10=DOUBLE).
constexpr uint8_t kTypeSize[11] = {1, 1, 1, 2, 2, 4, 4, 8, 8, 4, 8};

ValueType toValueType(uint8_t mavlink_type) {
  switch (mavlink_type) {
    case MAVLINK_TYPE_UINT8_T:
      return ValueType::kUint8;
    case MAVLINK_TYPE_INT8_T:
      return ValueType::kInt8;
    case MAVLINK_TYPE_UINT16_T:
      return ValueType::kUint16;
    case MAVLINK_TYPE_INT16_T:
      return ValueType::kInt16;
    case MAVLINK_TYPE_UINT32_T:
      return ValueType::kUint32;
    case MAVLINK_TYPE_INT32_T:
      return ValueType::kInt32;
    case MAVLINK_TYPE_UINT64_T:
      return ValueType::kUint64;
    case MAVLINK_TYPE_INT64_T:
      return ValueType::kInt64;
    case MAVLINK_TYPE_FLOAT:
      return ValueType::kFloat32;
    default:
      return ValueType::kFloat64;
  }
}

template <typename T>
T load(const char* payload, unsigned offset) {
  T v;
  std::memcpy(&v, payload + offset, sizeof(T));
  return v;
}

Value readValue(uint8_t mavlink_type, const char* payload, unsigned offset) {
  switch (mavlink_type) {
    case MAVLINK_TYPE_UINT8_T:
      return load<uint8_t>(payload, offset);
    case MAVLINK_TYPE_INT8_T:
      return load<int8_t>(payload, offset);
    case MAVLINK_TYPE_UINT16_T:
      return load<uint16_t>(payload, offset);
    case MAVLINK_TYPE_INT16_T:
      return load<int16_t>(payload, offset);
    case MAVLINK_TYPE_UINT32_T:
      return load<uint32_t>(payload, offset);
    case MAVLINK_TYPE_INT32_T:
      return load<int32_t>(payload, offset);
    case MAVLINK_TYPE_UINT64_T:
      return load<uint64_t>(payload, offset);
    case MAVLINK_TYPE_INT64_T:
      return load<int64_t>(payload, offset);
    case MAVLINK_TYPE_FLOAT:
      return load<float>(payload, offset);
    default:
      return load<double>(payload, offset);
  }
}

std::shared_ptr<const Schema> buildSchema(const mavlink_message_info_t& info) {
  auto schema = std::make_shared<Schema>();
  for (unsigned i = 0; i < info.num_fields; ++i) {
    const mavlink_field_info_t& f = info.fields[i];
    if (f.type == MAVLINK_TYPE_CHAR) {
      continue;
    }
    const unsigned count = (f.array_length > 0) ? f.array_length : 1;
    for (unsigned j = 0; j < count; ++j) {
      std::string name = f.name;
      if (f.array_length > 0) {
        name += "." + std::to_string(j);
      }
      schema->push_back({std::move(name), toValueType(static_cast<uint8_t>(f.type))});
    }
  }
  return schema;
}

// First pass of the PJ3 plugin: the first scalar time_usec (uint64) / time_boot_ms (uint32) field decides; a zero value
// ends the search without a timestamp.
std::optional<int64_t> findEmbeddedTimestamp(const mavlink_message_info_t& info, const char* payload) {
  for (unsigned i = 0; i < info.num_fields; ++i) {
    const mavlink_field_info_t& f = info.fields[i];
    if (f.array_length != 0) {
      continue;
    }
    if (std::strcmp(f.name, kFieldTimeUsec) == 0 && f.type == MAVLINK_TYPE_UINT64_T) {
      const auto v = load<uint64_t>(payload, f.wire_offset);
      if (v > 0) {
        return static_cast<int64_t>(v) * 1000;
      }
      return std::nullopt;
    }
    if (std::strcmp(f.name, kFieldTimeBootMs) == 0 && f.type == MAVLINK_TYPE_UINT32_T) {
      const auto v = load<uint32_t>(payload, f.wire_offset);
      if (v > 0) {
        return static_cast<int64_t>(v) * 1000000;
      }
      return std::nullopt;
    }
  }
  return std::nullopt;
}

}  // namespace

struct StreamParser::Impl {
  mavlink_status_t status{};
  mavlink_message_t msg{};
  std::unordered_map<uint32_t, std::shared_ptr<const Schema>> schemas;

  void decode(std::vector<Message>& out) {
    const mavlink_message_info_t* info = mavlink_get_message_info(&msg);
    if (info == nullptr) {
      return;
    }
    const char* payload = _MAV_PAYLOAD(&msg);

    Message m;
    m.sysid = msg.sysid;
    m.compid = msg.compid;
    m.msgid = msg.msgid;
    m.topic = "mav/" + std::to_string(msg.sysid) + "." + std::to_string(msg.compid) + "/" + info->name;
    m.embedded_timestamp_ns = findEmbeddedTimestamp(*info, payload);

    auto& schema = schemas[msg.msgid];
    if (!schema) {
      schema = buildSchema(*info);
    }
    m.schema = schema;

    m.values.reserve(schema->size());
    for (unsigned i = 0; i < info->num_fields; ++i) {
      const mavlink_field_info_t& f = info->fields[i];
      if (f.type == MAVLINK_TYPE_CHAR) {
        continue;
      }
      const unsigned count = (f.array_length > 0) ? f.array_length : 1;
      const unsigned elem_size = kTypeSize[static_cast<uint8_t>(f.type)];
      for (unsigned j = 0; j < count; ++j) {
        m.values.push_back(readValue(static_cast<uint8_t>(f.type), payload, f.wire_offset + j * elem_size));
      }
    }
    out.push_back(std::move(m));
  }
};

StreamParser::StreamParser() : impl_(std::make_unique<Impl>()) {}
StreamParser::~StreamParser() = default;

void StreamParser::reset() {
  mavlink_reset_channel_status(kChannel);
  impl_->status = {};
  impl_->msg = {};
}

void StreamParser::feed(const uint8_t* data, size_t size, std::vector<Message>& out) {
  for (size_t i = 0; i < size; ++i) {
    if (mavlink_parse_char(kChannel, data[i], &impl_->msg, &impl_->status)) {
      impl_->decode(out);
    }
  }
}

}  // namespace pj_drone::mavlink
