// SPDX-License-Identifier: MIT

#pragma once

#include <filesystem>
#include <string>

#include "pj_drone_core/ardupilot/vehicle.hpp"

namespace ardupilot_detail {

struct ModeOverridesResult {
  pj_drone::ardupilot::ModeOverrides overrides;
  std::string error;  // empty on success; the whole file is rejected on the first problem and `overrides` stays empty
};

/// Reads the user's mode-name file. Each entry is a complete mode enum that replaces the built-in tables for the logs
/// it matches (a log is named by one table only, never by a mix). The format is picked from the content: text starting
/// with '<' is XML, anything else JSON.
///
/// JSON:
///
///   { "firmwares": [
///       { "match": "MyBuild", "modes": { "0": "STABILIZE", "5": "LOITER", "30": "FOO" } } ] }
///
/// `match` (optional, default "") is looked for in the log's firmware string; `modes` (required, not empty) maps a
/// mode number (0-255) to its name. Unknown keys are ignored.
///
/// XML: a MAVLink `ardupilotmega.xml` (from c_library_v2 or any other version). Its COPTER_MODE, PLANE_MODE,
/// ROVER_MODE, SUB_MODE and TRACKER_MODE enums each become one entry that applies to logs of that vehicle (Heli uses
/// COPTER_MODE). An entry's name is its <description>, else the enum name without its prefix, the same rule as the
/// built-in tables. Enums the file lacks are skipped (those vehicles keep the built-in names), but at least one must
/// be present.
ModeOverridesResult loadModeOverrides(const std::filesystem::path& path);

}  // namespace ardupilot_detail
