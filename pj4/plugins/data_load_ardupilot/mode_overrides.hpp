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
/// it matches (a log is named by one table only, never by a mix):
///
///   { "version": 1,
///     "firmwares": [
///       { "match": "MyBuild", "modes": { "0": "STABILIZE", "5": "LOITER", "30": "FOO" } } ] }
///
/// `match` (optional, default "") is looked for in the log's firmware string; `modes` (required, not empty) maps a
/// mode number (0-255) to its name. Unknown keys are ignored so the format can grow.
ModeOverridesResult loadModeOverrides(const std::filesystem::path& path);

/// The file may also be a MAVLink `ardupilotmega.xml` (from c_library_v2 or any other version): its COPTER_MODE,
/// PLANE_MODE, ROVER_MODE, SUB_MODE and TRACKER_MODE enums each become one entry that applies to logs of that vehicle
/// (Heli uses COPTER_MODE). An entry's name is its <description>, else the enum name without its prefix, the same
/// rule as the built-in tables. Enums the file lacks are skipped (those vehicles keep the built-in names), but at least
/// one must be present. The format is picked from the content: text starting with '<' is XML, anything else JSON.
///
/// Same as loadModeOverrides() for text that is already in memory.
ModeOverridesResult parseModeOverrides(const std::string& text);

/// The JSON format only (no content sniffing).
ModeOverridesResult parseModeJson(const std::string& json_text);

}  // namespace ardupilot_detail
