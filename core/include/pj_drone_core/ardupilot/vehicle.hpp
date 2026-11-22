// SPDX-License-Identifier: MIT

#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pj_drone::ardupilot {

/// Vehicle kinds a log can come from. kHeli shares ArduCopter's firmware name and flight modes.
enum class VehicleType { kUnknown, kRover, kCopter, kPlane, kTracker, kSub, kPeriph, kBlimp, kHeli };

/// Where VehicleInfo::type came from, most trusted first. See docs/ardupilot/log_vehicle_type.md.
enum class VehicleSource { kNone, kVerBuildId, kVerFirmware, kMsgFirmware };

struct VehicleInfo {
  VehicleType type = VehicleType::kUnknown;  // kUnknown also when the sources disagree (see `conflict`)
  VehicleSource source = VehicleSource::kNone;
  bool conflict = false;  // VER.BU and the firmware string name different vehicles: neither is picked
  bool custom = false;    // the firmware string was "custom [original]"
  std::optional<int> build_id;
  VehicleType from_build_id = VehicleType::kUnknown;
  VehicleType from_firmware = VehicleType::kUnknown;
  std::string firmware;  // the string the firmware-based result came from; empty when there is none
};

/// VER.BU -> vehicle (AP_Vehicle_Type.h). kUnknown for values that are not in the table.
VehicleType vehicleFromBuildId(int build_id);

/// Vehicle named by a firmware string such as "ArduCopter V4.8.0-dev (abc)". Text inside the last "[...]" replaces the
/// string first (custom firmware writes "custom name [original name]"); `custom` reports whether that happened.
/// kUnknown when the prefix is not a known firmware name.
VehicleType vehicleFromFirmwareString(std::string_view firmware, bool* custom = nullptr);

const char* vehicleName(VehicleType type);

/// A complete mode enum: the names of the custom-mode numbers (the MODE message's Mode field) of the logs it matches.
/// The built-in tables are such entries, one per vehicle; a user supplied entry (for custom firmware, or for logs whose
/// vehicle sources disagree) is looked at before them. A log is named by exactly one entry as a whole, never a mix, so
/// a mode the entry does not list stays unnamed.
struct ModeOverride {
  std::optional<VehicleType> vehicle;  // when set, only logs of this vehicle match (kHeli counts as kCopter)
  std::string match;                   // a case-insensitive substring of VehicleInfo::firmware; empty matches any log
  std::map<int, std::string> modes;    // mode number -> name: the whole enum
};
using ModeOverrides = std::vector<ModeOverride>;

/// The first entry of `overrides` that applies to `info`; null when none does. The pointer points into `overrides`.
const ModeOverride* findModeOverride(const ModeOverrides& overrides, const VehicleInfo& info);

/// The built-in tables: one entry per vehicle that has a mode enum in the MAVLink dialect (Copter, Plane, Rover, Sub,
/// Tracker; Heli uses Copter's). Blimp and AP_Periph have none.
const ModeOverrides& builtinModeOverrides();

/// The entry that names the log `info`: the first of `user` that applies, else the built-in one of the vehicle; null
/// when there is none (vehicle unknown or in conflict, or without a table). Points into `user` or the built-in tables.
const ModeOverride* resolveModeOverride(const ModeOverrides& user, const VehicleInfo& info);

/// The text shown for `mode`: its name in `entry`, or "Unknown(<mode>)" when `entry` is null or lacks it.
std::string modeLabel(const ModeOverride* entry, int mode);

/// Collects the vehicle evidence of a log (VER.BU, VER.FWS, MSG text) and applies the rules of
/// docs/ardupilot/log_vehicle_type.md section 6.
class VehicleDetector {
 public:
  /// The first VER of a log wins: a later one (a log with several boots joined) does not change the vehicle.
  void setBuildId(int build_id);
  void setVerFirmware(std::string_view firmware);
  /// Offer a MSG text; only the texts up to the first one naming a firmware are used.
  void offerMessage(std::string_view text);

  VehicleInfo info() const;

 private:
  std::optional<int> build_id_;
  std::string ver_firmware_;
  std::string msg_firmware_;
};

}  // namespace pj_drone::ardupilot
