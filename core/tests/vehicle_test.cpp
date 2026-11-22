#include "pj_drone_core/ardupilot/vehicle.hpp"

#include <gtest/gtest.h>

using namespace pj_drone::ardupilot;

TEST(Vehicle, BuildIdTable) {
  EXPECT_EQ(vehicleFromBuildId(1), VehicleType::kRover);
  EXPECT_EQ(vehicleFromBuildId(2), VehicleType::kCopter);
  EXPECT_EQ(vehicleFromBuildId(3), VehicleType::kPlane);
  EXPECT_EQ(vehicleFromBuildId(4), VehicleType::kTracker);
  EXPECT_EQ(vehicleFromBuildId(7), VehicleType::kSub);
  EXPECT_EQ(vehicleFromBuildId(9), VehicleType::kPeriph);
  EXPECT_EQ(vehicleFromBuildId(12), VehicleType::kBlimp);
  EXPECT_EQ(vehicleFromBuildId(13), VehicleType::kHeli);
  EXPECT_EQ(vehicleFromBuildId(5), VehicleType::kUnknown);
  EXPECT_EQ(vehicleFromBuildId(6), VehicleType::kUnknown);
}

TEST(Vehicle, FirmwareStringPrefixes) {
  EXPECT_EQ(vehicleFromFirmwareString("ArduCopter V4.8.0-dev (abc)"), VehicleType::kCopter);
  EXPECT_EQ(vehicleFromFirmwareString("ArduPlane V4.5.0"), VehicleType::kPlane);
  EXPECT_EQ(vehicleFromFirmwareString("AntennaTracker V4.5.0"), VehicleType::kTracker);
  EXPECT_EQ(vehicleFromFirmwareString("ArduSub V4.5.0"), VehicleType::kSub);
  EXPECT_EQ(vehicleFromFirmwareString("Blimp V4.5.0"), VehicleType::kBlimp);
  EXPECT_EQ(vehicleFromFirmwareString("ArduRover V4.5.0"), VehicleType::kRover);
  EXPECT_EQ(vehicleFromFirmwareString("Rover V4.5.0"), VehicleType::kUnknown);  // not the firmware's name
  EXPECT_EQ(vehicleFromFirmwareString("Arming checks passed"), VehicleType::kUnknown);
}

TEST(Vehicle, CustomFirmwareUsesTheBracketedOriginalName) {
  bool custom = false;
  EXPECT_EQ(vehicleFromFirmwareString("MyBuild 1.0 [ArduPlane V4.5.0]", &custom), VehicleType::kPlane);
  EXPECT_TRUE(custom);
  EXPECT_EQ(vehicleFromFirmwareString("ArduPlane V4.5.0", &custom), VehicleType::kPlane);
  EXPECT_FALSE(custom);
}

namespace {

// The built-in name of `mode` for a log of `type` (no user entries, no firmware string).
std::string builtinLabel(VehicleType type, int mode) {
  VehicleInfo info;
  info.type = type;
  return modeLabel(resolveModeOverride({}, info), mode);
}

}  // namespace

TEST(Vehicle, BuiltInModeNames) {
  EXPECT_EQ(builtinLabel(VehicleType::kCopter, 5), "LOITER");
  EXPECT_EQ(builtinLabel(VehicleType::kHeli, 5), "LOITER");
  EXPECT_EQ(builtinLabel(VehicleType::kPlane, 5), "FBWA");  // the XML <description>, not the enum name
  EXPECT_EQ(builtinLabel(VehicleType::kCopter, 2), "ALT HOLD");
  EXPECT_EQ(builtinLabel(VehicleType::kRover, 4), "HOLD");
  EXPECT_EQ(builtinLabel(VehicleType::kSub, 19), "MANUAL");
  EXPECT_EQ(builtinLabel(VehicleType::kTracker, 10), "AUTO");
  EXPECT_EQ(builtinLabel(VehicleType::kCopter, 8), "Unknown(8)");  // hole in the table
  EXPECT_EQ(builtinLabel(VehicleType::kBlimp, 0), "Unknown(0)");
  EXPECT_EQ(builtinLabel(VehicleType::kPeriph, 0), "Unknown(0)");
  EXPECT_EQ(builtinLabel(VehicleType::kUnknown, 0), "Unknown(0)");
}

TEST(Vehicle, BuiltInTablesAreOneEntryPerVehicle) {
  const ModeOverrides& tables = builtinModeOverrides();
  ASSERT_EQ(tables.size(), 5u);
  for (const ModeOverride& entry : tables) {
    EXPECT_TRUE(entry.vehicle.has_value());
    EXPECT_TRUE(entry.match.empty());
    EXPECT_FALSE(entry.modes.empty());
  }
}

namespace {

VehicleInfo customCopter() {
  VehicleDetector d;
  d.setVerFirmware("MyBuild 1.0 [ArduCopter V4.5.0]");
  return d.info();
}

}  // namespace

TEST(ModeOverride, FindsTheFirstEntryWhoseMatchIsInTheFirmwareString) {
  const VehicleInfo info = customCopter();
  ModeOverrides overrides(3);
  overrides[0].match = "Other";
  overrides[1].match = "MyBuild";
  overrides[2].match = "";  // matches everything, but comes later
  EXPECT_EQ(findModeOverride(overrides, info), &overrides[1]);

  overrides[1].match = "Nope";
  EXPECT_EQ(findModeOverride(overrides, info), &overrides[2]);
  overrides.pop_back();
  EXPECT_EQ(findModeOverride(overrides, info), nullptr);
}

TEST(ModeOverride, MatchIgnoresCase) {
  const VehicleInfo info = customCopter();  // "MyBuild 1.0 [ArduCopter V4.5.0]"
  ModeOverrides overrides(1);
  overrides[0].match = "mybuild";
  EXPECT_EQ(findModeOverride(overrides, info), &overrides[0]);
  overrides[0].match = "ARDUCOPTER v4";
  EXPECT_EQ(findModeOverride(overrides, info), &overrides[0]);
  overrides[0].match = "mybuild 2";
  EXPECT_EQ(findModeOverride(overrides, info), nullptr);
}

TEST(ModeOverride, VehicleRestrictsTheEntryAndHeliCountsAsCopter) {
  VehicleInfo info;  // detected from VER.BU only: there is no firmware string
  info.type = VehicleType::kHeli;
  ModeOverrides overrides(2);
  overrides[0].vehicle = VehicleType::kPlane;
  overrides[1].vehicle = VehicleType::kCopter;
  EXPECT_EQ(findModeOverride(overrides, info), &overrides[1]);

  info.type = VehicleType::kRover;
  EXPECT_EQ(findModeOverride(overrides, info), nullptr);
}

TEST(ModeOverride, TheEntryIsTheWholeEnumAndNeverMixedWithTheBuiltInTable) {
  const VehicleInfo info = customCopter();  // Copter, custom
  ModeOverride entry;
  entry.modes[30] = "FOO";
  entry.modes[5] = "MY_LOITER";

  const ModeOverrides user = {entry};
  const ModeOverride* resolved = resolveModeOverride(user, info);
  ASSERT_EQ(resolved, &user[0]);
  EXPECT_EQ(modeLabel(resolved, 30), "FOO");
  EXPECT_EQ(modeLabel(resolved, 5), "MY_LOITER");   // replaces the built-in LOITER
  EXPECT_EQ(modeLabel(resolved, 6), "Unknown(6)");  // built-in Copter has RTL here, but the entry does not

  // Without a user entry the built-in table of the vehicle is used as a whole.
  EXPECT_EQ(modeLabel(resolveModeOverride({}, info), 6), "RTL");
}

TEST(ModeOverride, ResolvesAVehicleConflict) {
  VehicleDetector d;
  d.setBuildId(3);
  d.setVerFirmware("ArduCopter V4.5.0");
  const VehicleInfo info = d.info();
  ASSERT_TRUE(info.conflict);
  EXPECT_EQ(resolveModeOverride({}, info), nullptr);  // neither source is picked, so no built-in table applies
  EXPECT_EQ(modeLabel(nullptr, 5), "Unknown(5)");

  ModeOverride entry;
  entry.match = "ArduCopter";
  entry.modes = {{5, "LOITER"}};
  const ModeOverrides user = {entry};
  EXPECT_EQ(modeLabel(resolveModeOverride(user, info), 5), "LOITER");
}

TEST(VehicleDetector, BuildIdWinsWhenFirmwareAgrees) {
  VehicleDetector d;
  d.setBuildId(2);
  d.setVerFirmware("ArduCopter V4.5.0");
  const VehicleInfo info = d.info();
  EXPECT_EQ(info.type, VehicleType::kCopter);
  EXPECT_EQ(info.source, VehicleSource::kVerBuildId);
  EXPECT_FALSE(info.conflict);
}

TEST(VehicleDetector, HeliBuildIdAgreesWithCopterFirmware) {
  VehicleDetector d;
  d.setBuildId(13);
  d.setVerFirmware("ArduCopter V4.5.0");
  const VehicleInfo info = d.info();
  EXPECT_EQ(info.type, VehicleType::kHeli);
  EXPECT_FALSE(info.conflict);
}

TEST(VehicleDetector, DisagreeingSourcesPickNeither) {
  VehicleDetector d;
  d.setBuildId(3);
  d.setVerFirmware("ArduCopter V4.5.0");
  const VehicleInfo info = d.info();
  EXPECT_TRUE(info.conflict);
  EXPECT_EQ(info.type, VehicleType::kUnknown);
  EXPECT_EQ(info.from_build_id, VehicleType::kPlane);
  EXPECT_EQ(info.from_firmware, VehicleType::kCopter);
}

TEST(VehicleDetector, FallsBackToVerFirmwareThenMessage) {
  VehicleDetector ver;
  ver.setVerFirmware("ArduSub V4.5.0");
  EXPECT_EQ(ver.info().type, VehicleType::kSub);
  EXPECT_EQ(ver.info().source, VehicleSource::kVerFirmware);

  VehicleDetector msg;
  msg.offerMessage("Arming checks passed");  // not a firmware banner
  EXPECT_EQ(msg.info().type, VehicleType::kUnknown);
  msg.offerMessage("ArduRover V4.5.0");
  msg.offerMessage("ArduPlane V4.5.0");  // a later banner does not replace the first
  EXPECT_EQ(msg.info().type, VehicleType::kRover);
  EXPECT_EQ(msg.info().source, VehicleSource::kMsgFirmware);
}

TEST(VehicleDetector, NothingKnown) {
  VehicleDetector d;
  const VehicleInfo info = d.info();
  EXPECT_EQ(info.type, VehicleType::kUnknown);
  EXPECT_EQ(info.source, VehicleSource::kNone);
  EXPECT_FALSE(info.conflict);
}
