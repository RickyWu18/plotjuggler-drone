// SPDX-License-Identifier: MIT

#pragma once

#include <pj_base/builtin/plot_markers.hpp>
#include <pj_base/sdk/data_source_host_views.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "pj_drone_core/ardupilot/parser.hpp"

namespace ardupilot_detail {

/// The topic that carries the marker series. A marker set is drawn on a plot that shows the series it is addressed to
/// (`markerSeriesKey(kMarkerTopic, field)`), so the user picks which markers to see by dragging these series onto it.
inline constexpr std::string_view kMarkerTopic = "_markers";

/// A field of the "_markers" topic and the marker set addressed to it. A curve needs data to be put on a plot, so
/// publishing gives the field a NaN sample at each time the markers mention (see sampleTimes): the curve draws nothing
/// and only the markers show.
struct MarkerSeries {
  std::string field;
  PJ::sdk::PlotMarkers markers;
};

/// What the parser collected from the MSG, MODE and ARM messages. `log_end_ns` is where open-ended spans stop.
struct MarkerSource {
  const std::vector<pj_drone::ardupilot::LogMessage>& messages;
  const std::vector<pj_drone::ardupilot::ModeChange>& modes;
  const std::vector<pj_drone::ardupilot::ArmChange>& arming;
  int64_t log_end_ns = 0;
};

/// Turns the collected rows into marker series. Series with no marker are left out:
///  - "message"      a Label per MSG message, or an Event whose severity follows the level when the log records one;
///  - "mode"         a Region per MODE, from its change to the next one (the last runs to the end of the log);
///  - "arm event"    an Event per arm/disarm;
///  - "arm"          a Region per armed span.
std::vector<MarkerSeries> buildMarkerSeries(const MarkerSource& source);

/// The times, sorted and without repeats, at which a series needs a sample: where a marker starts, and for a Region
/// where it ends.
std::vector<int64_t> sampleTimes(const PJ::sdk::PlotMarkers& markers);

/// Writes the NaN rows of every series to the "_markers" topic, then registers the marker sets. The sets go last, so a
/// failure never leaves a set addressed to a series that did not get its rows. Returns the first error.
PJ::Status publishMarkers(
    const PJ::sdk::SourceWriteHostView& write_host, const PJ::sdk::SourceObjectWriteHostView& object_host,
    const std::vector<MarkerSeries>& series);

}  // namespace ardupilot_detail
