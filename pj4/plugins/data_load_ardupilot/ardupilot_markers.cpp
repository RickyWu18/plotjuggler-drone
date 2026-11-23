// SPDX-License-Identifier: MIT

#include "ardupilot_markers.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <pj_base/builtin/plot_markers_codec.hpp>
#include <utility>

namespace ardupilot_detail {

namespace {

namespace ap = pj_drone::ardupilot;
using PJ::sdk::MarkerKind;
using PJ::sdk::MarkerSeverity;
using PJ::sdk::PlotMarker;

PlotMarker makeLabel(int64_t ts_ns, std::string category, std::string label) {
  PlotMarker marker;
  marker.kind = MarkerKind::kLabel;
  marker.t_start = ts_ns;
  marker.category = std::move(category);
  marker.label = std::move(label);
  return marker;
}

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

/// Adds `key = value` to the marker's metadata, unless the log had no such field (the parser reports that as < 0).
void addIfPresent(PlotMarker& marker, const char* key, int value) {
  if (value >= 0) {
    marker.metadata.push_back({key, std::to_string(value)});
  }
}

/// MAV_SEVERITY: 0 emergency, 1 alert, 2 critical, 3 error, 4 warning, 5 notice, 6 info, 7 debug. Notice, info and
/// debug share the Info look: they are all routine.
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

MarkerSeries messageSeries(const std::vector<ap::LogMessage>& messages) {
  MarkerSeries out{"message", {}};
  for (const ap::LogMessage& message : messages) {
    // The log may give no level: then the message is a plain label.
    PlotMarker marker =
        message.severity < 0
            ? makeLabel(message.timestamp_ns, "log_message", message.message)
            : makeEvent(message.timestamp_ns, "log_message", message.message, severityOf(message.severity));
    addIfPresent(marker, "severity", message.severity);
    out.markers.markers.push_back(std::move(marker));
  }
  return out;
}

/// One colour per mode number, so a mode keeps its colour through the whole log. None is red: that is the arming
/// colour. Modes 8 apart share a colour; the label tells them apart.
PJ::sdk::ColorRGBA modeColor(int mode) {
  static constexpr PJ::sdk::ColorRGBA kPalette[] = {
      {0, 114, 178, 255},  {0, 158, 115, 255},  {148, 103, 189, 255}, {86, 180, 233, 255},
      {120, 140, 30, 255}, {200, 120, 20, 255}, {110, 125, 140, 255}, {190, 70, 150, 255},
  };
  return kPalette[static_cast<size_t>(mode) % std::size(kPalette)];
}

MarkerSeries modeSeries(const std::vector<ap::ModeChange>& changes, int64_t log_end_ns) {
  MarkerSeries out{"mode", {}};
  for (size_t i = 0; i < changes.size(); ++i) {
    const int64_t end_ns = i + 1 < changes.size() ? changes[i + 1].timestamp_ns : log_end_ns;
    PlotMarker marker =
        makeRegion(changes[i].timestamp_ns, end_ns, "flight_mode", changes[i].name, MarkerSeverity::kInfo);
    marker.color = modeColor(changes[i].mode);
    addIfPresent(marker, "mode", changes[i].mode);
    addIfPresent(marker, "reason", changes[i].reason);
    out.markers.markers.push_back(std::move(marker));
  }
  return out;
}

/// Splits the ARM rows into one Event per arm/disarm and one Region per armed span.
std::pair<MarkerSeries, MarkerSeries> armingSeries(const std::vector<ap::ArmChange>& changes, int64_t log_end_ns) {
  std::pair<MarkerSeries, MarkerSeries> out{MarkerSeries{"arm event", {}}, MarkerSeries{"arm", {}}};
  auto& [events, regions] = out;
  const ap::ArmChange* armed_since = nullptr;
  const auto close_region = [&](int64_t end_ns) {
    PlotMarker marker = makeRegion(armed_since->timestamp_ns, end_ns, "arm_region", "ARMED", MarkerSeverity::kError);
    addIfPresent(marker, "arm_method", armed_since->method);
    regions.markers.markers.push_back(std::move(marker));
    armed_since = nullptr;
  };

  for (const ap::ArmChange& change : changes) {
    const bool is_transition = armed_since == nullptr ? change.armed : !change.armed;
    if (!is_transition) {
      continue;  // a repeated row of the state the vehicle is already in
    }
    PlotMarker event = makeEvent(
        change.timestamp_ns, "arm_event", change.armed ? "ARM" : "DISARM",
        change.forced ? MarkerSeverity::kWarning : MarkerSeverity::kInfo);
    addIfPresent(event, "method", change.method);
    event.metadata.push_back({"checks", std::to_string(change.checks)});
    event.metadata.push_back({"forced", change.forced ? "true" : "false"});
    events.markers.markers.push_back(std::move(event));

    if (change.armed) {
      armed_since = &change;
    } else {
      close_region(change.timestamp_ns);
    }
  }
  if (armed_since != nullptr) {
    close_region(log_end_ns);  // still armed when the log ends
  }
  return out;
}

/// Registers one marker set. The overlay reads the latest set whatever the playback position; stamping it at its first
/// time keeps the topic inside the log's own time range instead of stretching it to zero.
PJ::Status pushSet(const PJ::sdk::SourceObjectWriteHostView& object_host, const MarkerSeries& entry) {
  const std::string key = PJ::sdk::markerSeriesKey(kMarkerTopic, entry.field);
  const auto topic =
      object_host.registerTopic(PJ::sdk::markerObjectTopicName(key), PJ::sdk::BuiltinObjectType::kPlotMarkers);
  if (!topic) {
    return PJ::unexpected(topic.error());
  }
  const std::vector<uint8_t> bytes = PJ::serializePlotMarkers(entry.markers);
  const auto pushed = object_host.pushOwned(
      *topic, PJ::Timestamp{sampleTimes(entry.markers).front()}, PJ::Span<const uint8_t>(bytes.data(), bytes.size()));
  if (!pushed) {
    return PJ::unexpected(pushed.error());
  }
  return PJ::okStatus();
}

}  // namespace

std::vector<MarkerSeries> buildMarkerSeries(const MarkerSource& source) {
  auto [arm_events, arm_regions] = armingSeries(source.arming, source.log_end_ns);
  std::vector<MarkerSeries> candidates;
  candidates.push_back(messageSeries(source.messages));
  candidates.push_back(modeSeries(source.modes, source.log_end_ns));
  candidates.push_back(std::move(arm_events));
  candidates.push_back(std::move(arm_regions));

  std::vector<MarkerSeries> all;
  for (MarkerSeries& series : candidates) {
    if (!series.markers.empty()) {
      all.push_back(std::move(series));
    }
  }
  return all;
}

std::vector<int64_t> sampleTimes(const PJ::sdk::PlotMarkers& markers) {
  std::vector<int64_t> times;
  for (const PlotMarker& marker : markers.markers) {
    times.push_back(marker.t_start);
    if (marker.kind == MarkerKind::kRegion) {
      times.push_back(marker.t_end);
    }
  }
  std::sort(times.begin(), times.end());
  times.erase(std::unique(times.begin(), times.end()), times.end());
  return times;
}

PJ::Status publishMarkers(
    const PJ::sdk::SourceWriteHostView& write_host, const PJ::sdk::SourceObjectWriteHostView& object_host,
    const std::vector<MarkerSeries>& series) {
  const auto topic = write_host.ensureTopic(std::string(kMarkerTopic));
  if (!topic) {
    return PJ::unexpected(topic.error());
  }

  struct Row {
    int64_t timestamp_ns;
    PJ::sdk::FieldHandle field;
  };
  std::vector<Row> rows;
  for (const MarkerSeries& entry : series) {
    const auto field = write_host.ensureField(*topic, entry.field, PJ::PrimitiveType::kFloat64);
    if (!field) {
      return PJ::unexpected(field.error());
    }
    for (const int64_t time_ns : sampleTimes(entry.markers)) {
      rows.push_back({time_ns, *field});
    }
  }
  // One topic takes its rows in time order, and the series interleave.
  std::stable_sort(
      rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.timestamp_ns < b.timestamp_ns; });
  for (const Row& row : rows) {
    const PJ::sdk::BoundFieldValue value{.field = row.field, .value = std::numeric_limits<double>::quiet_NaN()};
    const auto status = write_host.appendBoundRecord(
        *topic, PJ::Timestamp{row.timestamp_ns}, PJ::Span<const PJ::sdk::BoundFieldValue>(&value, 1));
    if (!status) {
      return PJ::unexpected(status.error());
    }
  }

  for (const MarkerSeries& entry : series) {
    if (const PJ::Status pushed = pushSet(object_host, entry); !pushed) {
      return pushed;
    }
  }
  return PJ::okStatus();
}

}  // namespace ardupilot_detail
