// SPDX-License-Identifier: MIT

#pragma once

#include <filesystem>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "pj_drone_core/ardupilot/parser.hpp"

namespace pj_drone::ardupilot {

/// "%.8g" — the text shown for, and exported with, a parameter value.
std::string formatParamValue(double value);

/// "%.6f" — the text shown for a log-message timestamp.
std::string formatMessageTimestamp(double seconds);

/// Case-insensitive parameter-name filter. The text is a regular expression (searched, not anchored); when it is
/// not a valid expression it falls back to a case-insensitive substring match. Empty text matches everything.
class ParamFilter {
 public:
  explicit ParamFilter(std::string text);
  bool matches(std::string_view name) const;

 private:
  std::string text_;
  std::regex regex_;
  bool regex_valid_ = false;
};

/// The parameters accepted by `filter`, ordered by name (ascending).
std::vector<Parameter> filterParameters(const std::vector<Parameter>& params, const ParamFilter& filter);

/// One "name,value\n" line per parameter, in the given order.
std::string buildParamFileText(const std::vector<Parameter>& params);

bool writeTextFile(const std::filesystem::path& path, const std::string& text);

struct ExportResult {
  size_t exported = 0;
  std::vector<std::string> failed;  // embedded-file names that could not be written
};

/// Writes each file below `dir`, keeping its relative path (e.g. "@SYS/crash_dump.bin" ->
/// <dir>/@SYS/crash_dump.bin). A name that is absolute or climbs out of `dir` ("..") is reported as failed.
ExportResult exportEmbeddedFiles(const std::vector<const EmbeddedFile*>& files, const std::filesystem::path& dir);

}  // namespace pj_drone::ardupilot
