// SPDX-License-Identifier: MIT

#pragma once

#include <filesystem>
#include <optional>
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

/// Inconsistencies that do not stop an export: the declared (offset based) size differing from the assembled
/// size, and chunks that overlap. Empty when the file is consistent.
std::vector<std::string> embeddedFileWarnings(const EmbeddedFile& file);

/// Whether `name` (a path taken from the log, so untrusted) could be exported below any folder.
bool isSafeExportName(std::string_view name);

enum class FileStatus { kOk, kWarning, kCannotExport };

struct FileCheck {
  FileStatus status = FileStatus::kOk;
  std::vector<std::string> details;  // one line per finding, blocking ones first
};

/// What the file list shows for `file`: kCannotExport for an unsafe name or missing chunks (gaps), kWarning when
/// embeddedFileWarnings() has something to say, otherwise kOk.
FileCheck checkEmbeddedFile(const EmbeddedFile& file);

/// Where `name` (a path taken from the log, so untrusted) lands below `dir`, e.g. "@SYS/crash.bin" ->
/// <dir>/@SYS/crash.bin. Returns nullopt when the name is unsafe: absolute, drive-relative, empty or "."/".."
/// components, characters or device names that are invalid on common file systems, or a result that does not
/// stay inside `dir` once symbolic links are resolved.
std::optional<std::filesystem::path> resolveExportPath(const std::filesystem::path& dir, std::string_view name);

/// Names of `files` whose export target already exists below `dir`.
std::vector<std::string> findExportConflicts(const std::vector<const EmbeddedFile*>& files,
                                             const std::filesystem::path& dir);

enum class OverwritePolicy { kOverwrite, kSkipExisting };

struct ExportFailure {
  std::string name;
  std::string reason;
};

struct ExportResult {
  size_t exported = 0;
  std::vector<std::string> skipped;        // target existed and the policy was kSkipExisting
  std::vector<ExportFailure> failed;       // could not be exported
  std::vector<std::string> warnings;       // "<name>: <problem>" for exported files with size inconsistencies
};

/// Re-reads the log at `log_path` and writes each of `files` (as listed by the earlier scan) below `dir`, keeping
/// its relative path. The content is assembled while streaming to a temporary ".part" file and is only moved into
/// place when its SHA-256 equals the digest recorded at scan time, so a log that changed in between is refused.
ExportResult exportEmbeddedFiles(const std::filesystem::path& log_path, const std::vector<const EmbeddedFile*>& files,
                                 const std::filesystem::path& dir, OverwritePolicy policy);

}  // namespace pj_drone::ardupilot
