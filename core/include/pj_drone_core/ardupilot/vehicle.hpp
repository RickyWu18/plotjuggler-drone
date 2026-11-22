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

/// Flight mode name for the vehicle's custom-mode number (the MODE message's Mode field), e.g. (kCopter, 5) ->
/// "LOITER". Empty when the vehicle is unknown or has no name for `mode`.
std::string_view modeName(VehicleType type, int mode);

/// A complete, user supplied mode enum that replaces the built-in tables for the logs it matches. It is meant for
/// custom firmware and for logs whose vehicle sources disagree. A log is named by exactly one table: this one as a
/// whole or the built-in one as a whole, never a mix, so a mode this table does not list stays unnamed.
struct ModeOverride {
  std::optional<VehicleType> vehicle;  // when set, only logs of this vehicle match (kHeli counts as kCopter)
  std::string match;                   // a case-insensitive substring of VehicleInfo::firmware; empty matches any log
  std::map<int, std::string> modes;    // mode number -> name: the whole enum
};
using ModeOverrides = std::vector<ModeOverride>;

/// Index of the first entry of `overrides` that applies to `info`; -1 when none does.
int findModeOverride(const ModeOverrides& overrides, const VehicleInfo& info);

/// The text shown for `mode`: the name in `override_entry` when there is one, otherwise the built-in name for the
/// detected vehicle; "Unknown(<mode>)" when the table in use has no such mode. `override_entry` may be null.
std::string modeLabel(const VehicleInfo& info, const ModeOverride* override_entry, int mode);

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
