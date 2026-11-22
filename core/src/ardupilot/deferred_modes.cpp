// SPDX-License-Identifier: MIT

#include "pj_drone_core/ardupilot/deferred_modes.hpp"

#include <string>
#include <string_view>

namespace pj_drone::ardupilot {

void DeferredModes::add(
    size_t group, int64_t timestamp_ns, int mode, int reason, const SampleValue* values, size_t count) {
  rows_.push_back({group, timestamp_ns, mode, reason, std::vector<SampleValue>(values, values + count)});
}

bool DeferredModes::flush(
    SampleSink& sink, const VehicleInfo& vehicle, const ModeOverrides& overrides, size_t& samples,
    std::vector<ModeChange>& changes) {
  const ModeOverride* entry = resolveModeOverride(overrides, vehicle);
  std::string label;  // backs the string_view handed to the sink
  bool accepted = true;
  for (Row& row : rows_) {
    label = modeLabel(entry, row.mode);
    row.values.emplace_back(std::string_view(label));
    samples += row.values.size();
    if (!sink.append(row.group, row.timestamp_ns, row.values.data(), row.values.size())) {
      accepted = false;
      break;
    }
    changes.push_back({row.timestamp_ns, row.mode, row.reason, label});
  }
  rows_.clear();
  return accepted;
}

}  // namespace pj_drone::ardupilot
