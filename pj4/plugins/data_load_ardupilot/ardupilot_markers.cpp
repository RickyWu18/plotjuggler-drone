// SPDX-License-Identifier: MIT

#include "ardupilot_markers.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

namespace ardupilot_detail {

namespace {

namespace ap = pj_drone::ardupilot;
using PJ::sdk::MarkerKind;
using PJ::sdk::MarkerSeverity;
using PJ::sdk::PlotMarker;

PlotMarker makeEvent(int64_t ts_ns, std::string category, std::string label, MarkerSeverity severity) {
  PlotMarker marker;
  marker.kind = MarkerKind::kEvent;
  marker.t_start = ts_ns;
  marker.t_end = ts_ns;
  marker.severity = severity;
  marker.category = std::move(category);
  marker.label = std::move(label);
  return marker;
}

PlotMarker makeRegion(
    int64_t start_ns, int64_t end_ns, std::string category, std::string label, MarkerSeverity severity) {
  PlotMarker marker;
  marker.kind = MarkerKind::kRegion;
  marker.t_start = start_ns;
  marker.t_end = std::max(start_ns, end_ns);
  marker.severity = severity;
  marker.category = std::move(category);
  marker.label = std::move(label);
  return marker;
}

/// MAV_SEVERITY: 0 emergency, 1 alert, 2 critical, 3 error, 4 warning, 5 notice, 6 info, 7 debug.
MarkerSeverity severityOf(int mav_severity) {
  if (mav_severity <= 2) {
    return MarkerSeverity::kCritical;
  }
  if (mav_severity == 3) {
    return MarkerSeverity::kError;
  }
  if (mav_severity == 4) {
    return MarkerSeverity::kWarning;
  }
  return MarkerSeverity::kInfo;
}

void addMessages(const ap::Parser& parser, MarkerSeries& out) {
  for (const ap::LogMessage& message : parser.logMessages()) {
    out.times_ns.push_back(message.timestamp_ns);
    if (message.severity < 0) {
      // The log gives no level: every message is a plain label.
      PlotMarker marker;
      marker.kind = MarkerKind::kLabel;
      marker.t_start = message.timestamp_ns;
      marker.category = "log_message";
      marker.label = message.message;
      out.markers.markers.push_back(std::move(marker));
      continue;
    }
    PlotMarker marker = makeEvent(message.timestamp_ns, "log_message", message.message, severityOf(message.severity));
    marker.metadata.push_back({"severity", std::to_string(message.severity)});
    out.markers.markers.push_back(std::move(marker));
  }
}

/// One colour per mode number, so a mode keeps its colour through the whole log. None is red: that is the arming
/// colour. Modes 8 apart share a colour; the label tells them apart.
PJ::sdk::ColorRGBA modeColor(int mode) {
  static constexpr PJ::sdk::ColorRGBA kPalette[] = {
      {0, 114, 178, 255},  {0, 158, 115, 255},  {148, 103, 189, 255}, {86, 180, 233, 255},
      {120, 140, 30, 255}, {200, 120, 20, 255}, {110, 125, 140, 255}, {190, 70, 150, 255},
  };
  constexpr int kCount = static_cast<int>(std::size(kPalette));
  return kPalette[((mode % kCount) + kCount) % kCount];
}

void addModes(const ap::Parser& parser, MarkerSeries& out) {
  const auto& changes = parser.modeChanges();
  for (size_t i = 0; i < changes.size(); ++i) {
    const int64_t end_ns = i + 1 < changes.size() ? changes[i + 1].timestamp_ns : parser.lastTimestampNs();
    PlotMarker marker =
        makeRegion(changes[i].timestamp_ns, end_ns, "flight_mode", changes[i].name, MarkerSeverity::kInfo);
    marker.color = modeColor(changes[i].mode);
    marker.metadata.push_back({"mode", std::to_string(changes[i].mode)});
    if (changes[i].reason >= 0) {
      marker.metadata.push_back({"reason", std::to_string(changes[i].reason)});
    }
    out.markers.markers.push_back(std::move(marker));
    out.times_ns.push_back(changes[i].timestamp_ns);
  }
}

void addArming(const ap::Parser& parser, MarkerSeries& events, MarkerSeries& regions) {
  const ap::ArmChange* armed_since = nullptr;
  const auto close_region = [&](int64_t end_ns) {
    PlotMarker marker = makeRegion(armed_since->timestamp_ns, end_ns, "arm_region", "ARMED", MarkerSeverity::kError);
    marker.metadata.push_back({"arm_method", std::to_string(armed_since->method)});
    regions.markers.markers.push_back(std::move(marker));
    regions.times_ns.push_back(end_ns);
    armed_since = nullptr;
  };

  for (const ap::ArmChange& change : parser.armChanges()) {
    const bool is_transition = armed_since == nullptr ? change.armed : !change.armed;
    if (!is_transition) {
      continue;  // a repeated row of the state the vehicle is already in
    }
    PlotMarker event = makeEvent(
        change.timestamp_ns, "arm_event", change.armed ? "ARM" : "DISARM",
        change.forced ? MarkerSeverity::kWarning : MarkerSeverity::kInfo);
    event.metadata.push_back({"method", std::to_string(change.method)});
    event.metadata.push_back({"checks", std::to_string(change.checks)});
    event.metadata.push_back({"forced", change.forced ? "true" : "false"});
    events.markers.markers.push_back(std::move(event));
    events.times_ns.push_back(change.timestamp_ns);

    if (change.armed) {
      armed_since = &change;
      regions.times_ns.push_back(change.timestamp_ns);
    } else {
      close_region(change.timestamp_ns);
    }
  }
  if (armed_since != nullptr) {
    close_region(parser.lastTimestampNs());  // still armed when the log ends
  }
}

}  // namespace

std::vector<MarkerSeries> buildMarkerSeries(const pj_drone::ardupilot::Parser& parser) {
  MarkerSeries messages{"message", {}, {}};
  MarkerSeries modes{"mode", {}, {}};
  MarkerSeries arm_events{"arm event", {}, {}};
  MarkerSeries arm_regions{"arm", {}, {}};
  addMessages(parser, messages);
  addModes(parser, modes);
  addArming(parser, arm_events, arm_regions);

  std::vector<MarkerSeries> all;
  for (MarkerSeries* series : {&messages, &modes, &arm_events, &arm_regions}) {
    if (!series->markers.empty()) {
      all.push_back(std::move(*series));
    }
  }
  return all;
}

}  // namespace ardupilot_detail
