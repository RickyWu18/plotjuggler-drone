// SPDX-License-Identifier: MIT

#pragma once

#include <filesystem>
#include <string>

#include "pj_drone_core/ardupilot/mode_xml.hpp"

namespace ardupilot_detail {

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
/// XML: a MAVLink `ardupilotmega.xml`, see pj_drone::ardupilot::parseMavlinkModeXml. Enums the file lacks are skipped
/// (those vehicles keep the built-in names).
pj_drone::ardupilot::ModeOverridesResult loadModeOverrides(const std::filesystem::path& path);

}  // namespace ardupilot_detail
