#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "pj_drone_core/ardupilot/log_tools.hpp"

using namespace pj_drone::ardupilot;

TEST(LogTools, FormatsValuesLikeTheOriginalPlugin) {
  EXPECT_EQ(formatParamValue(0.5), "0.5");
  EXPECT_EQ(formatParamValue(1234567.891), "1234567.9");
  EXPECT_EQ(formatMessageTimestamp(1.5), "1.500000");
}

TEST(LogTools, ParamFilterIsCaseInsensitiveRegexWithSubstringFallback) {
  EXPECT_TRUE(ParamFilter("").matches("ATC_RAT_P"));
  EXPECT_TRUE(ParamFilter("^atc").matches("ATC_RAT_P"));
  EXPECT_FALSE(ParamFilter("^RAT").matches("ATC_RAT_P"));
  EXPECT_TRUE(ParamFilter("RAT_.*P").matches("ATC_RAT_P"));
  // "(" is not a valid regex: falls back to a plain substring match.
  EXPECT_TRUE(ParamFilter("(").matches("A(B"));
  EXPECT_FALSE(ParamFilter("(").matches("AB"));
}

TEST(LogTools, FilterSortsByNameAndBuildsParamFile) {
  const std::vector<Parameter> params = {{"B_PARAM", 2.0}, {"A_PARAM", 0.25}, {"C_OTHER", 3.0}};
  const auto filtered = filterParameters(params, ParamFilter("PARAM"));
  ASSERT_EQ(filtered.size(), 2U);
  EXPECT_EQ(filtered[0].name, "A_PARAM");
  EXPECT_EQ(buildParamFileText(filtered), "A_PARAM,0.25\nB_PARAM,2\n");
}

TEST(LogTools, ExportKeepsSubdirectoriesAndRejectsEscapes) {
  const auto dir = std::filesystem::temp_directory_path() / "pj_drone_core_export_test";
  std::error_code ec_cleanup;
  std::filesystem::remove_all(dir, ec_cleanup);

  EmbeddedFile ok{"@SYS/crash.bin", {1, 2, 3}};
  EmbeddedFile escape{"../evil.bin", {9}};
  EmbeddedFile absolute{std::filesystem::absolute("evil_abs.bin").string(), {9}};

  const auto result = exportEmbeddedFiles({&ok, &escape, &absolute}, dir);
  EXPECT_EQ(result.exported, 1U);
  EXPECT_EQ(result.failed.size(), 2U);

  {
    std::ifstream in(dir / "@SYS" / "crash.bin", std::ios::binary);
    ASSERT_TRUE(in.good());
    std::ostringstream content;
    content << in.rdbuf();
    EXPECT_EQ(content.str(), std::string("\x01\x02\x03", 3));
  }
  EXPECT_FALSE(std::filesystem::exists(dir.parent_path() / "evil.bin"));

  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}
