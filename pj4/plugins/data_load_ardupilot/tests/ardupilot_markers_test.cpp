// SPDX-License-Identifier: MIT

#include "../ardupilot_markers.hpp"

#include <gtest/gtest.h>

namespace {

namespace ap = pj_drone::ardupilot;
using ardupilot_detail::buildMarkerSeries;
using ardupilot_detail::MarkerSeries;
using ardupilot_detail::MarkerSource;
using ardupilot_detail::sampleTimes;
using PJ::sdk::MarkerKind;
using PJ::sdk::MarkerSeverity;

constexpr int64_t kSecond = 1000000000;

const MarkerSeries* find(const std::vector<MarkerSeries>& all, const std::string& field) {
  for (const MarkerSeries& series : all) {
    if (series.field == field) {
      return &series;
    }
  }
  return nullptr;
}

ap::ArmChange arm(int64_t t, bool armed, bool forced = false, int method = 2) {
  ap::ArmChange change;
  change.timestamp_ns = t;
  change.armed = armed;
  change.forced = forced;
  change.method = method;
  return change;
}

std::vector<MarkerSeries> build(
    const std::vector<ap::LogMessage>& messages, const std::vector<ap::ModeChange>& modes,
    const std::vector<ap::ArmChange>& arming, int64_t log_end_ns) {
  return buildMarkerSeries(MarkerSource{messages, modes, arming, log_end_ns});
}

TEST(ArdupilotMarkers, EmptyInputGivesNoSeries) {
  EXPECT_TRUE(build({}, {}, {}, 10 * kSecond).empty());
}

TEST(ArdupilotMarkers, MessageWithoutSeverityIsALabelAndWithOneIsAnEvent) {
  const auto all = build({{kSecond, "PreArm: Compass", -1}, {2 * kSecond, "Battery failsafe", 3}}, {}, {}, 5 * kSecond);

  const MarkerSeries* messages = find(all, "message");
  ASSERT_NE(messages, nullptr);
  ASSERT_EQ(messages->markers.markers.size(), 2U);
  EXPECT_EQ(messages->markers.markers[0].kind, MarkerKind::kLabel);
  EXPECT_TRUE(messages->markers.markers[0].metadata.empty());
  EXPECT_EQ(messages->markers.markers[1].kind, MarkerKind::kEvent);
  EXPECT_EQ(messages->markers.markers[1].severity, MarkerSeverity::kError);
}

TEST(ArdupilotMarkers, ModeRegionsRunToTheNextChangeAndTheLastToTheLogEnd) {
  const auto all = build({}, {{kSecond, 5, 1, "FBWA"}, {4 * kSecond, 11, -1, "RTL"}}, {}, 9 * kSecond);

  const MarkerSeries* modes = find(all, "mode");
  ASSERT_NE(modes, nullptr);
  ASSERT_EQ(modes->markers.markers.size(), 2U);
  EXPECT_EQ(modes->markers.markers[0].t_start, kSecond);
  EXPECT_EQ(modes->markers.markers[0].t_end, 4 * kSecond);
  EXPECT_EQ(modes->markers.markers[0].label, "FBWA");
  EXPECT_EQ(modes->markers.markers[1].t_end, 9 * kSecond);
  // `reason` is only there when the log had one.
  EXPECT_EQ(modes->markers.markers[0].metadata.size(), 2U);
  EXPECT_EQ(modes->markers.markers[1].metadata.size(), 1U);
}

TEST(ArdupilotMarkers, ArmRowsMakeEventsAndRegionsAndIgnoreRepeats) {
  const auto all =
      build({}, {}, {arm(kSecond, true), arm(2 * kSecond, true), arm(5 * kSecond, false, true)}, 9 * kSecond);

  const MarkerSeries* events = find(all, "arm event");
  const MarkerSeries* regions = find(all, "arm");
  ASSERT_NE(events, nullptr);
  ASSERT_NE(regions, nullptr);
  ASSERT_EQ(events->markers.markers.size(), 2U);  // the repeated "armed" row is not a transition
  EXPECT_EQ(events->markers.markers[0].label, "ARM");
  EXPECT_EQ(events->markers.markers[1].label, "DISARM");
  EXPECT_EQ(events->markers.markers[1].severity, MarkerSeverity::kWarning);  // forced
  ASSERT_EQ(regions->markers.markers.size(), 1U);
  EXPECT_EQ(regions->markers.markers[0].t_start, kSecond);
  EXPECT_EQ(regions->markers.markers[0].t_end, 5 * kSecond);
}

TEST(ArdupilotMarkers, StillArmedAtTheEndOfTheLogClosesTheRegionThere) {
  const auto all = build({}, {}, {arm(kSecond, true)}, 7 * kSecond);

  const MarkerSeries* regions = find(all, "arm");
  ASSERT_NE(regions, nullptr);
  ASSERT_EQ(regions->markers.markers.size(), 1U);
  EXPECT_EQ(regions->markers.markers[0].t_end, 7 * kSecond);
}

TEST(ArdupilotMarkers, ADisarmRowBeforeAnyArmIsIgnored) {
  const auto all = build({}, {}, {arm(kSecond, false)}, 7 * kSecond);
  EXPECT_TRUE(all.empty());
}

TEST(ArdupilotMarkers, ARegionEndingBeforeItStartsIsClamped) {
  // A log whose last timestamp is behind the arm row (no later message carried a time).
  const auto all = build({}, {}, {arm(5 * kSecond, true)}, 0);

  const MarkerSeries* regions = find(all, "arm");
  ASSERT_NE(regions, nullptr);
  EXPECT_EQ(regions->markers.markers[0].t_end, 5 * kSecond);
}

TEST(ArdupilotMarkers, SampleTimesAreSortedWithoutRepeatsAndRegionsCountBothEnds) {
  const auto all = build({{2 * kSecond, "a", -1}, {2 * kSecond, "b", -1}}, {{kSecond, 5, -1, "FBWA"}}, {}, 4 * kSecond);

  const MarkerSeries* messages = find(all, "message");
  const MarkerSeries* modes = find(all, "mode");
  ASSERT_NE(messages, nullptr);
  ASSERT_NE(modes, nullptr);
  EXPECT_EQ(sampleTimes(messages->markers), (std::vector<int64_t>{2 * kSecond}));
  EXPECT_EQ(sampleTimes(modes->markers), (std::vector<int64_t>{kSecond, 4 * kSecond}));
}

}  // namespace
