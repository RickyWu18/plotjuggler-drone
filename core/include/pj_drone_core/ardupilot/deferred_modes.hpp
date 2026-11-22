// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "pj_drone_core/ardupilot/sample_sink.hpp"
#include "pj_drone_core/ardupilot/vehicle.hpp"

namespace pj_drone::ardupilot {

/// MODE rows held back until the whole log has been read. The VER / MSG that identify the vehicle can come after the
/// first MODE row, and one log must not be named by two different rule sets, so the "ModeName" field of every row is
/// only filled in once the vehicle is final.
class DeferredModes {
 public:
  /// Hold one row. `values` are the log's own fields of the MODE message; "ModeName" is appended by flush().
  void add(size_t group, int64_t timestamp_ns, int mode, const SampleValue* values, size_t count);

  /// Append every held row to `sink`, each with its mode name (see modeLabel) as the last value, then forget them.
  /// `samples` grows by the number of values delivered. Returns false when the sink refuses a row; the rows not
  /// yet delivered are dropped.
  bool flush(SampleSink& sink, const VehicleInfo& vehicle, const ModeOverrides& overrides, size_t& samples);

 private:
  struct Row {
    size_t group = 0;
    int64_t timestamp_ns = 0;
    int mode = 0;
    std::vector<SampleValue> values;
  };
  std::vector<Row> rows_;
};

}  // namespace pj_drone::ardupilot
