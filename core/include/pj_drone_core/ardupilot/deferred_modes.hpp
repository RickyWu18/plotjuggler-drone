// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "pj_drone_core/ardupilot/sample_sink.hpp"
#include "pj_drone_core/ardupilot/vehicle.hpp"

namespace pj_drone::ardupilot {

/// One MODE row of the log with its resolved name.
struct ModeChange {
  int64_t timestamp_ns = 0;
  int mode = 0;
  int reason = -1;   // the log's "Rsn" field; -1 when the message has none
  std::string name;  // modeLabel() of `mode` for the vehicle the log turned out to come from
};

/// MODE rows held back until the whole log has been read. The VER / MSG that identify the vehicle can come after the
/// first MODE row, and one log must not be named by two different rule sets, so the "ModeName" field of every row is
/// only filled in once the vehicle is final.
class DeferredModes {
 public:
  /// Hold one row. `values` are the log's own fields of the MODE message; "ModeName" is appended by flush(). Without a
  /// sink there are none, and `group` is unused. `reason` is the row's "Rsn" field, or -1 when the message has none.
  void add(size_t group, int64_t timestamp_ns, int mode, int reason, const SampleValue* values, size_t count);

  /// Name every held row for `vehicle` (see modeLabel), record it in changes(), and deliver it to `sink` with the name
  /// as its last value when there is one. Then forget the held rows. `samples` grows by the number of values
  /// delivered. Returns false when the sink refuses a row; the rows not yet delivered are dropped.
  bool flush(SampleSink* sink, const VehicleInfo& vehicle, const ModeOverrides& overrides, size_t& samples);

  /// Every row flush() has named, in log order. Filled whether or not there is a sink.
  const std::vector<ModeChange>& changes() const {
    return changes_;
  }

 private:
  struct Row {
    size_t group = 0;
    ModeChange change;  // `name` is empty until flush()
    std::vector<SampleValue> values;
  };
  std::vector<Row> rows_;
  std::vector<ModeChange> changes_;
};

}  // namespace pj_drone::ardupilot
