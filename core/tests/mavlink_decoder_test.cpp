/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <gtest/gtest.h>

#include <vector>

#include "pj_drone_core/mavlink/decoder.hpp"

#define MAVLINK_USE_MESSAGE_INFO
#include "all/mavlink.h"

namespace mav = pj_drone::mavlink;

namespace {

std::vector<uint8_t> toBytes(const mavlink_message_t& msg) {
  uint8_t buf[MAVLINK_MAX_PACKET_LEN];
  const int len = mavlink_msg_to_send_buffer(buf, &msg);
  return {buf, buf + len};
}

std::vector<mav::Message> parse(mav::StreamParser& parser, const std::vector<uint8_t>& bytes) {
  std::vector<mav::Message> out;
  parser.feed(bytes.data(), bytes.size(), out);
  return out;
}

size_t indexOf(const mav::Message& m, const std::string& name) {
  for (size_t i = 0; i < m.schema->size(); ++i) {
    if ((*m.schema)[i].name == name) {
      return i;
    }
  }
  return m.schema->size();
}

}  // namespace

TEST(MavlinkDecoder, DecodesAttitudeWithBootTimeAndNativeTypes) {
  mavlink_message_t msg;
  mavlink_msg_attitude_pack(7, 3, &msg, 1500, 0.5F, -0.25F, 1.0F, 0.1F, 0.2F, 0.3F);

  mav::StreamParser parser;
  parser.reset();
  const auto out = parse(parser, toBytes(msg));
  ASSERT_EQ(out.size(), 1U);
  const auto& m = out[0];

  EXPECT_EQ(m.topic, "mav/7.3/ATTITUDE");
  EXPECT_EQ(m.sysid, 7);
  EXPECT_EQ(m.compid, 3);
  ASSERT_TRUE(m.embedded_timestamp_ns.has_value());
  EXPECT_EQ(*m.embedded_timestamp_ns, 1500LL * 1000000LL);

  ASSERT_EQ(m.values.size(), m.schema->size());
  const size_t roll = indexOf(m, "roll");
  ASSERT_LT(roll, m.schema->size());
  EXPECT_EQ((*m.schema)[roll].type, mav::ValueType::kFloat32);
  EXPECT_FLOAT_EQ(std::get<float>(m.values[roll]), 0.5F);
  const size_t boot = indexOf(m, "time_boot_ms");
  ASSERT_LT(boot, m.schema->size());
  EXPECT_EQ(std::get<uint32_t>(m.values[boot]), 1500U);
}

TEST(MavlinkDecoder, ZeroEmbeddedTimeMeansNoTimestamp) {
  mavlink_message_t msg;
  mavlink_msg_attitude_pack(1, 1, &msg, 0, 0, 0, 0, 0, 0, 0);

  mav::StreamParser parser;
  parser.reset();
  const auto out = parse(parser, toBytes(msg));
  ASSERT_EQ(out.size(), 1U);
  EXPECT_FALSE(out[0].embedded_timestamp_ns.has_value());
}

TEST(MavlinkDecoder, TimeUsecIsConvertedToNanoseconds) {
  mavlink_message_t msg;
  const float q[4] = {1.0F, 0.0F, 0.0F, 0.0F};
  const float cov[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  mavlink_msg_attitude_quaternion_cov_pack(1, 1, &msg, 1700000000123456ULL, q, 0.1F, 0.2F, 0.3F, cov);

  mav::StreamParser parser;
  parser.reset();
  const auto out = parse(parser, toBytes(msg));
  ASSERT_EQ(out.size(), 1U);
  ASSERT_TRUE(out[0].embedded_timestamp_ns.has_value());
  EXPECT_EQ(*out[0].embedded_timestamp_ns, 1700000000123456000LL);
  const size_t usec = indexOf(out[0], "time_usec");
  EXPECT_EQ(std::get<uint64_t>(out[0].values[usec]), 1700000000123456ULL);
}

TEST(MavlinkDecoder, SystemTimeUsesBootTimeNotUnixTime) {
  mavlink_message_t msg;
  mavlink_msg_system_time_pack(1, 1, &msg, 1700000000123456ULL, 42);

  mav::StreamParser parser;
  parser.reset();
  const auto out = parse(parser, toBytes(msg));
  ASSERT_EQ(out.size(), 1U);
  ASSERT_TRUE(out[0].embedded_timestamp_ns.has_value());
  EXPECT_EQ(*out[0].embedded_timestamp_ns, 42LL * 1000000LL);
}

TEST(MavlinkDecoder, MessageWithoutTimeFieldHasNoEmbeddedTimestamp) {
  mavlink_message_t msg;
  mavlink_msg_heartbeat_pack(1, 1, &msg, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_ARDUPILOTMEGA, 81, 5, MAV_STATE_ACTIVE);

  mav::StreamParser parser;
  parser.reset();
  const auto out = parse(parser, toBytes(msg));
  ASSERT_EQ(out.size(), 1U);
  EXPECT_EQ(out[0].topic, "mav/1.1/HEARTBEAT");
  EXPECT_FALSE(out[0].embedded_timestamp_ns.has_value());
  const size_t custom = indexOf(out[0], "custom_mode");
  EXPECT_EQ(std::get<uint32_t>(out[0].values[custom]), 5U);
}

TEST(MavlinkDecoder, ArrayFieldsGetIndexedNamesAndCharFieldsAreSkipped) {
  mavlink_message_t msg;
  const float q[4] = {1.0F, 0.0F, 0.0F, 0.0F};
  const float cov[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  mavlink_msg_attitude_quaternion_cov_pack(1, 1, &msg, 10, q, 0.1F, 0.2F, 0.3F, cov);

  mav::StreamParser parser;
  parser.reset();
  const auto out = parse(parser, toBytes(msg));
  ASSERT_EQ(out.size(), 1U);
  EXPECT_LT(indexOf(out[0], "q.0"), out[0].schema->size());
  EXPECT_LT(indexOf(out[0], "q.3"), out[0].schema->size());
  EXPECT_EQ(indexOf(out[0], "q"), out[0].schema->size());
  EXPECT_FLOAT_EQ(std::get<float>(out[0].values[indexOf(out[0], "q.0")]), 1.0F);

  mavlink_message_t text;
  // The pack function copies the full fixed-size text field (50 bytes), so pass a full-size buffer.
  char status_text[50] = "hello";
  mavlink_msg_statustext_pack(1, 1, &text, MAV_SEVERITY_INFO, status_text, 0, 0);
  const auto text_out = parse(parser, toBytes(text));
  ASSERT_EQ(text_out.size(), 1U);
  EXPECT_EQ(indexOf(text_out[0], "text"), text_out[0].schema->size());
  EXPECT_LT(indexOf(text_out[0], "severity"), text_out[0].schema->size());
}

TEST(MavlinkDecoder, ReassemblesFramesSplitAcrossFeeds) {
  mavlink_message_t a;
  mavlink_message_t b;
  mavlink_msg_attitude_pack(1, 1, &a, 100, 1, 2, 3, 4, 5, 6);
  mavlink_msg_attitude_pack(1, 1, &b, 200, 1, 2, 3, 4, 5, 6);
  auto bytes = toBytes(a);
  const auto second = toBytes(b);
  bytes.insert(bytes.end(), second.begin(), second.end());

  mav::StreamParser parser;
  parser.reset();
  std::vector<mav::Message> out;
  const size_t half = bytes.size() / 2;
  parser.feed(bytes.data(), half, out);
  EXPECT_LE(out.size(), 1U);
  parser.feed(bytes.data() + half, bytes.size() - half, out);
  ASSERT_EQ(out.size(), 2U);
  EXPECT_EQ(*out[1].embedded_timestamp_ns, 200LL * 1000000LL);
}

TEST(MavlinkDecoder, GarbageBytesProduceNoMessages) {
  mav::StreamParser parser;
  parser.reset();
  const std::vector<uint8_t> noise = {0x01, 0x02, 0x03, 0xAA, 0x55, 0x00, 0xFF};
  EXPECT_TRUE(parse(parser, noise).empty());
}
