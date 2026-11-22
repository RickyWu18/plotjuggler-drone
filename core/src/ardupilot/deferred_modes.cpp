// SPDX-License-Identifier: MIT

#include "pj_drone_core/ardupilot/deferred_modes.hpp"

#include <string>
#include <string_view>

namespace pj_drone::ardupilot {

void DeferredModes::add(size_t group, int64_t timestamp_ns, int mode, const SampleValue* values, size_t count) {
  rows_.push_back({group, timestamp_ns, mode, std::vector<SampleValue>(values, values + count)});
}

bool DeferredModes::flush(
    SampleSink& sink, const VehicleInfo& vehicle, const ModeOverrides& overrides, size_t& samples) {
  const ModeOverride* entry = findModeOverride(overrides, vehicle);
  std::string label;  // backs the string_view handed to the sink
  bool accepted = true;
  for (Row& row : rows_) {
    label = modeLabel(vehicle, entry, row.mode);
    row.values.emplace_back(std::string_view(label));
    samples += row.values.size();
    if (!sink.append(row.group, row.timestamp_ns, row.values.data(), row.values.size())) {
      accepted = false;
      break;
    }
  }
  rows_.clear();
  return accepted;
}

}  // namespace pj_drone::ardupilot
