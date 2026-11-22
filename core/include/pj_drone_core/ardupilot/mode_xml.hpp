// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <string_view>

#include "pj_drone_core/ardupilot/vehicle.hpp"

namespace pj_drone::ardupilot {

/// The entries read from a mode file, or why it was rejected.
struct ModeOverridesResult {
  ModeOverrides overrides;
  std::string error;  // empty on success; the whole file is rejected on the first problem and `overrides` stays empty
};

/// True when this build can read MAVLink XML (pugixml was found at configure time). PJ4 builds have it; a build
/// without it still has the built-in tables and every other feature.
#ifdef PJ_DRONE_HAS_MODE_XML
inline constexpr bool kHasModeXml = true;
#else
inline constexpr bool kHasModeXml = false;
#endif

/// Reads the mode enums of a MAVLink `ardupilotmega.xml` (from c_library_v2 or any other version): COPTER_MODE,
/// PLANE_MODE, ROVER_MODE, SUB_MODE and TRACKER_MODE each become one entry that applies to logs of that vehicle (Heli
/// uses COPTER_MODE). An entry's name is its <description> with whitespace collapsed, else the enum name without its
/// prefix, the same rule as the built-in tables (core/scripts/gen_mode_tables.py). Enums the file lacks are skipped,
/// but at least one must be present; a value outside 0-255 or a duplicate rejects the file.
/// Without XML support (kHasModeXml == false) this always fails.
ModeOverridesResult parseMavlinkModeXml(std::string_view xml);

}  // namespace pj_drone::ardupilot
