/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

// ardupilot_mode_tables_gen.h is generated at build time by gen_mode_tables.py
// from ardupilotmega.xml into ${CMAKE_CURRENT_BINARY_DIR}.
#include "ardupilot_mode_tables_gen.h"

#include <cstdint>
#include <string_view>

namespace ap_mode_tables {

// Returns the human-readable mode name for a given MAV_TYPE + mode number.
// Table data is baked in at compile time; returns "UNKNOWN" for any
// unrecognised vehicle type or mode number.
inline std::string_view modeToStringView(uint8_t mav_type, uint8_t mode_num)
{
  static constexpr const std::string_view* kTables[] = {
      kPlaneNames, kCopterNames, kRoverNames, kSubNames, kTrackerNames,
  };

  const int group = [&]() -> int {
    switch (mav_type)
    {
      case 1:
      case 7: case 8: case 16: case 17: case 28:
      case 19: case 20: case 21: case 22:
      case 23: case 24: case 25: case 47:
        return 0;  // Plane
      case 2: case 3: case 4:
      case 13: case 14: case 15:
      case 29: case 35: case 43:
        return 1;  // Copter
      case 10: case 11:
        return 2;  // Rover
      case 12:
        return 3;  // Sub
      case 5:
        return 4;  // Tracker
      default:
        return -1;
    }
  }();

  if (group < 0) return "UNKNOWN";
  const auto sv = kTables[group][mode_num];
  return sv.empty() ? std::string_view{"UNKNOWN"} : sv;
}

}  // namespace ap_mode_tables
