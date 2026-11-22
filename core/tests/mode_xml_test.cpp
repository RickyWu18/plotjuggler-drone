#include "pj_drone_core/ardupilot/mode_xml.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

using namespace pj_drone::ardupilot;

namespace {

#define SKIP_WITHOUT_XML()                                   \
  if (!kHasModeXml) {                                        \
    GTEST_SKIP() << "built without pugixml: no XML support"; \
  }

std::string wrap(const std::string& enum_xml) {
  return "<mavlink><enums>" + enum_xml + "</enums></mavlink>";
}

}  // namespace

TEST(ModeXml, WithoutSupportItFailsInsteadOfPretending) {
  if (kHasModeXml) {
    GTEST_SKIP() << "built with pugixml";
  }
  const ModeOverridesResult result = parseMavlinkModeXml(wrap(""));
  EXPECT_TRUE(result.overrides.empty());
  EXPECT_FALSE(result.error.empty());
}

TEST(ModeXml, NameIsTheCollapsedDescriptionElseTheEnumNameWithoutItsPrefix) {
  SKIP_WITHOUT_XML();
  const ModeOverridesResult result = parseMavlinkModeXml(wrap(R"(
    <enum name="COPTER_MODE">
      <entry value="2" name="COPTER_MODE_ALT_HOLD"><description>  ALT
         HOLD </description></entry>
      <entry value="5" name="COPTER_MODE_LOITER"/>
    </enum>
    <enum name="SOMETHING_ELSE"><entry value="1" name="SOMETHING_ELSE_X"/></enum>)"));
  ASSERT_TRUE(result.error.empty()) << result.error;
  ASSERT_EQ(result.overrides.size(), 1u);
  EXPECT_EQ(result.overrides[0].vehicle, VehicleType::kCopter);
  EXPECT_TRUE(result.overrides[0].match.empty());
  EXPECT_EQ(result.overrides[0].modes.at(2), "ALT HOLD");
  EXPECT_EQ(result.overrides[0].modes.at(5), "LOITER");
}

TEST(ModeXml, RejectsTheWholeFileOnTheFirstProblem) {
  SKIP_WITHOUT_XML();
  const auto rejected = [](const std::string& xml) { return !parseMavlinkModeXml(xml).error.empty(); };
  EXPECT_TRUE(rejected("<mavlink><enums>"));                          // not XML
  EXPECT_TRUE(rejected(wrap("")));                                    // none of the five enums
  EXPECT_TRUE(rejected(wrap(R"(<enum name="PLANE_MODE"></enum>)")));  // no entries
  EXPECT_TRUE(rejected(wrap(R"(<enum name="PLANE_MODE">
      <entry value="256" name="PLANE_MODE_X"/></enum>)")));           // out of range
  EXPECT_TRUE(rejected(wrap(R"(<enum name="PLANE_MODE">
      <entry value="1" name="PLANE_MODE_X"/><entry value="1" name="PLANE_MODE_Y"/></enum>)")));  // duplicate
  EXPECT_TRUE(rejected(wrap(R"(<enum name="PLANE_MODE">
      <entry value="1" name="OTHER_X"/></enum>)")));                                             // no name to show
}

// Drift check: the built-in tables are generated from this file by core/scripts/gen_mode_tables.py, the run-time
// reader is parseMavlinkModeXml. They are two implementations of one rule and must agree on the real dialect.
TEST(ModeXml, ReadsTheDialectFileLikeTheBuiltInTables) {
  SKIP_WITHOUT_XML();
  std::ifstream file(PJ_ARDUPILOTMEGA_XML, std::ios::binary);
  ASSERT_TRUE(file) << PJ_ARDUPILOTMEGA_XML;
  std::ostringstream text;
  text << file.rdbuf();

  const ModeOverridesResult parsed = parseMavlinkModeXml(text.str());
  ASSERT_TRUE(parsed.error.empty()) << parsed.error;

  const ModeOverrides& builtin = builtinModeOverrides();
  ASSERT_EQ(parsed.overrides.size(), builtin.size());
  for (const ModeOverride& entry : parsed.overrides) {  // in the file's order, which is not the built-in one
    ASSERT_TRUE(entry.vehicle.has_value());
    VehicleInfo info;
    info.type = *entry.vehicle;
    const ModeOverride* expected = findModeOverride(builtin, info);
    ASSERT_NE(expected, nullptr) << vehicleName(*entry.vehicle);
    EXPECT_EQ(entry.match, expected->match) << vehicleName(*entry.vehicle);
    EXPECT_EQ(entry.modes, expected->modes) << vehicleName(*entry.vehicle);
  }
}
