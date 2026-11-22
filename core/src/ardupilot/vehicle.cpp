// SPDX-License-Identifier: MIT

#include "pj_drone_core/ardupilot/vehicle.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>

namespace pj_drone::ardupilot {

namespace {

// Heli is a Copter with the same firmware name; for comparing the two sources they are one family.
VehicleType family(VehicleType type) {
  return type == VehicleType::kHeli ? VehicleType::kCopter : type;
}

struct FirmwarePrefix {
  std::string_view prefix;
  VehicleType type;
};

// "ArduRover", not "Rover". Heli firmware reports "ArduCopter"; AP_Periph's prefix was not verified.
constexpr FirmwarePrefix kFirmwarePrefixes[] = {
    {"ArduCopter", VehicleType::kCopter}, {"ArduPlane", VehicleType::kPlane}, {"AntennaTracker", VehicleType::kTracker},
    {"ArduSub", VehicleType::kSub},       {"Blimp", VehicleType::kBlimp},     {"ArduRover", VehicleType::kRover},
};

}  // namespace

VehicleType vehicleFromBuildId(int build_id) {
  switch (build_id) {
    case 1:
      return VehicleType::kRover;
    case 2:
      return VehicleType::kCopter;
    case 3:
      return VehicleType::kPlane;
    case 4:
      return VehicleType::kTracker;
    case 7:
      return VehicleType::kSub;
    case 9:
      return VehicleType::kPeriph;
    case 12:
      return VehicleType::kBlimp;
    case 13:
      return VehicleType::kHeli;
    default:
      return VehicleType::kUnknown;
  }
}

VehicleType vehicleFromFirmwareString(std::string_view firmware, bool* custom) {
  if (custom != nullptr) {
    *custom = false;
  }
  const size_t open = firmware.rfind('[');
  if (open != std::string_view::npos) {
    const size_t close = firmware.find(']', open);
    if (close != std::string_view::npos) {
      firmware = firmware.substr(open + 1, close - open - 1);
      if (custom != nullptr) {
        *custom = true;
      }
    }
  }
  for (const FirmwarePrefix& entry : kFirmwarePrefixes) {
    if (firmware.substr(0, entry.prefix.size()) == entry.prefix) {
      return entry.type;
    }
  }
  return VehicleType::kUnknown;
}

const char* vehicleName(VehicleType type) {
  switch (type) {
    case VehicleType::kRover:
      return "Rover";
    case VehicleType::kCopter:
      return "Copter";
    case VehicleType::kPlane:
      return "Plane";
    case VehicleType::kTracker:
      return "Tracker";
    case VehicleType::kSub:
      return "Sub";
    case VehicleType::kPeriph:
      return "AP_Periph";
    case VehicleType::kBlimp:
      return "Blimp";
    case VehicleType::kHeli:
      return "Heli";
    case VehicleType::kUnknown:
      break;
  }
  return "Unknown";
}

const ModeOverrides& builtinModeOverrides() {
  // One entry per vehicle with a mode enum in the MAVLink ardupilotmega.xml dialect (Blimp and AP_Periph have none),
  // generated at build time by core/scripts/gen_mode_tables.py (see core/CMakeLists.txt).
  static const ModeOverrides kTables = {
#include "pj_drone_mode_tables.inc"
  };
  return kTables;
}

namespace {

// ASCII case-insensitive substring test; an empty needle is found everywhere.
bool containsIgnoreCase(std::string_view haystack, std::string_view needle) {
  if (needle.empty()) {
    return true;
  }
  const auto same = [](char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
  };
  return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(), same) != haystack.end();
}

}  // namespace

const ModeOverride* findModeOverride(const ModeOverrides& overrides, const VehicleInfo& info) {
  for (const ModeOverride& entry : overrides) {
    if (entry.vehicle && family(*entry.vehicle) != family(info.type)) {
      continue;
    }
    if (containsIgnoreCase(info.firmware, entry.match)) {
      return &entry;
    }
  }
  return nullptr;
}

const ModeOverride* resolveModeOverride(const ModeOverrides& user, const VehicleInfo& info) {
  const ModeOverride* entry = findModeOverride(user, info);
  return entry != nullptr ? entry : findModeOverride(builtinModeOverrides(), info);
}

std::string modeLabel(const ModeOverride* entry, int mode) {
  // One table per log: a mode the table lacks is unknown, not looked up in another one.
  if (entry != nullptr) {
    const auto named = entry->modes.find(mode);
    if (named != entry->modes.end()) {
      return named->second;
    }
  }
  return "Unknown(" + std::to_string(mode) + ")";
}

void VehicleDetector::setBuildId(int build_id) {
  if (!build_id_) {
    build_id_ = build_id;
  }
}

void VehicleDetector::setVerFirmware(std::string_view firmware) {
  if (ver_firmware_.empty()) {
    ver_firmware_ = std::string(firmware);
  }
}

void VehicleDetector::offerMessage(std::string_view text) {
  if (msg_firmware_.empty() && vehicleFromFirmwareString(text) != VehicleType::kUnknown) {
    msg_firmware_ = std::string(text);
  }
}

VehicleInfo VehicleDetector::info() const {
  VehicleInfo info;
  info.build_id = build_id_;
  if (build_id_) {
    info.from_build_id = vehicleFromBuildId(*build_id_);
  }

  // The firmware-string result: VER.FWS first, the MSG copy only when VER gives nothing usable.
  VehicleSource fw_source = VehicleSource::kNone;
  if (!ver_firmware_.empty()) {
    info.from_firmware = vehicleFromFirmwareString(ver_firmware_, &info.custom);
    info.firmware = ver_firmware_;
    fw_source = VehicleSource::kVerFirmware;
  }
  if (info.from_firmware == VehicleType::kUnknown && !msg_firmware_.empty()) {
    info.from_firmware = vehicleFromFirmwareString(msg_firmware_, &info.custom);
    info.firmware = msg_firmware_;
    fw_source = VehicleSource::kMsgFirmware;
  }

  const bool have_build = info.from_build_id != VehicleType::kUnknown;
  const bool have_fw = info.from_firmware != VehicleType::kUnknown;
  if (have_build && have_fw && family(info.from_build_id) != family(info.from_firmware)) {
    info.conflict = true;  // report both, pick neither
  } else if (have_build) {
    info.type = info.from_build_id;
    info.source = VehicleSource::kVerBuildId;
  } else if (have_fw) {
    info.type = info.from_firmware;
    info.source = fw_source;
  }
  return info;
}

}  // namespace pj_drone::ardupilot
