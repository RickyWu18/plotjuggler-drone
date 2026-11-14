// SPDX-License-Identifier: MIT

#include "pj_drone_core/ardupilot/log_tools.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>

#include "pj_drone_core/io/mapped_file.hpp"

namespace pj_drone::ardupilot {

namespace {

std::string toLower(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

constexpr size_t kMaxNameLength = 1024;
constexpr size_t kMaxComponentLength = 255;

bool isReservedDeviceName(const std::string& component) {
  // Windows treats "CON", "con.txt", ... as devices whatever the extension.
  std::string stem = toLower(component.substr(0, component.find('.')));
  while (!stem.empty() && stem.back() == ' ') {
    stem.pop_back();
  }
  if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul") {
    return true;
  }
  return stem.size() == 4 && (stem.compare(0, 3, "com") == 0 || stem.compare(0, 3, "lpt") == 0) && stem[3] >= '0' &&
         stem[3] <= '9';
}

bool isSafeComponent(const std::string& c) {
  if (c.empty() || c == "." || c == ".." || c.size() > kMaxComponentLength) {
    return false;
  }
  // A trailing dot or space is silently dropped by Windows, so "x." would alias "x".
  if (c.back() == '.' || c.back() == ' ') {
    return false;
  }
  for (const char ch : c) {
    const auto u = static_cast<unsigned char>(ch);
    if (u < 0x20 || u == 0x7F || std::string_view("<>:\"|?*").find(ch) != std::string_view::npos) {
      return false;
    }
  }
  return !isReservedDeviceName(c);
}

std::filesystem::path pathFromUtf8(std::string_view s) {
  return std::filesystem::path(std::u8string(s.begin(), s.end()));
}

// Key under which two export targets count as the same file.
std::string collisionKey(const std::filesystem::path& path) {
  std::string key = path.generic_string();
#ifdef _WIN32
  key = toLower(key);
#endif
  return key;
}

}  // namespace

std::string formatParamValue(double value) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.8g", value);
  return buf;
}

std::string formatMessageTimestamp(double seconds) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.6f", seconds);
  return buf;
}

ParamFilter::ParamFilter(std::string text) : text_(std::move(text)) {
  try {
    regex_ = std::regex(text_, std::regex::ECMAScript | std::regex::icase);
    regex_valid_ = true;
  } catch (const std::regex_error&) {
    regex_valid_ = false;
  }
}

bool ParamFilter::matches(std::string_view name) const {
  if (text_.empty()) {
    return true;
  }
  if (regex_valid_) {
    return std::regex_search(name.begin(), name.end(), regex_);
  }
  return toLower(name).find(toLower(text_)) != std::string::npos;
}

std::vector<Parameter> filterParameters(const std::vector<Parameter>& params, const ParamFilter& filter) {
  std::vector<Parameter> out;
  for (const auto& p : params) {
    if (filter.matches(p.name)) {
      out.push_back(p);
    }
  }
  std::sort(out.begin(), out.end(), [](const Parameter& a, const Parameter& b) { return a.name < b.name; });
  return out;
}

std::string buildParamFileText(const std::vector<Parameter>& params) {
  std::string text;
  for (const auto& p : params) {
    text += p.name;
    text += ',';
    text += formatParamValue(p.value);
    text += '\n';
  }
  return text;
}

bool writeTextFile(const std::filesystem::path& path, const std::string& text) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  return out.good();
}

std::vector<std::string> embeddedFileWarnings(const EmbeddedFile& file) {
  std::vector<std::string> out;
  if (file.estimated_size != file.size) {
    out.push_back("declared size " + std::to_string(file.estimated_size) + " bytes (from FILE offsets) but " +
                  std::to_string(file.size) + " bytes assembled");
  }
  if (file.overlap_bytes > 0) {
    out.push_back(std::to_string(file.overlap_bytes) + " byte(s) provided by several chunks, the later one is used");
  }
  return out;
}

FileCheck checkEmbeddedFile(const EmbeddedFile& file) {
  FileCheck check;
  if (!isSafeExportName(file.name)) {
    check.details.push_back("unsafe file name");
  }
  if (file.gap_bytes > 0) {
    check.details.push_back(std::to_string(file.gap_bytes) + " byte(s) missing between chunks, file is incomplete");
  }
  if (!check.details.empty()) {
    check.status = FileStatus::kCannotExport;
  }
  const auto warnings = embeddedFileWarnings(file);
  if (check.status == FileStatus::kOk && !warnings.empty()) {
    check.status = FileStatus::kWarning;
  }
  check.details.insert(check.details.end(), warnings.begin(), warnings.end());
  return check;
}

namespace {

// The relative path `name` stands for, or nullopt when any part of it is unsafe.
std::optional<std::filesystem::path> safeRelativePath(std::string_view name) {
  if (name.empty() || name.size() > kMaxNameLength || name.front() == '/' || name.front() == '\\') {
    return std::nullopt;
  }

  // Both separators split components on every platform, so a name cannot mean different things per OS.
  std::filesystem::path relative;
  size_t start = 0;
  while (start <= name.size()) {
    size_t end = name.find_first_of("/\\", start);
    if (end == std::string_view::npos) {
      end = name.size();
    }
    const std::string component(name.substr(start, end - start));
    if (!isSafeComponent(component)) {
      return std::nullopt;
    }
    relative /= pathFromUtf8(component);
    start = end + 1;
  }
  return relative;
}

}  // namespace

bool isSafeExportName(std::string_view name) {
  return safeRelativePath(name).has_value();
}

std::optional<std::filesystem::path> resolveExportPath(const std::filesystem::path& dir, std::string_view name) {
  const auto relative = safeRelativePath(name);
  if (!relative) {
    return std::nullopt;
  }

  std::error_code ec;
  const auto canonical_dir = std::filesystem::weakly_canonical(dir, ec);
  if (ec) {
    return std::nullopt;
  }
  const auto target = dir / *relative;
  const auto canonical_target = std::filesystem::weakly_canonical(target, ec);
  if (ec) {
    return std::nullopt;
  }
  // Symbolic links / junctions are resolved by weakly_canonical, so a link pointing out of `dir` shows up here.
  const auto rel_to_dir = canonical_target.lexically_relative(canonical_dir);
  if (rel_to_dir.empty() || rel_to_dir.is_absolute() || *rel_to_dir.begin() == "..") {
    return std::nullopt;
  }
  return target;
}

std::vector<std::string> findExportConflicts(const std::vector<const EmbeddedFile*>& files,
                                             const std::filesystem::path& dir) {
  std::vector<std::string> conflicts;
  for (const EmbeddedFile* file : files) {
    if (file == nullptr) {
      continue;
    }
    const auto path = resolveExportPath(dir, file->name);
    std::error_code ec;
    if (path && std::filesystem::exists(std::filesystem::symlink_status(*path, ec))) {
      conflicts.push_back(file->name);
    }
  }
  return conflicts;
}

ExportResult exportEmbeddedFiles(const std::filesystem::path& log_path, const std::vector<const EmbeddedFile*>& files,
                                 const std::filesystem::path& dir, OverwritePolicy policy) {
  ExportResult result;
  const auto fail = [&result](const EmbeddedFile& file, std::string reason) {
    result.failed.push_back({file.name, std::move(reason)});
  };

  // Several files resolving to one target would overwrite each other ("a/b" and "a\b" are the same path), so none
  // of them is written.
  std::map<std::string, size_t> target_uses;
  for (const EmbeddedFile* file : files) {
    if (const auto target = file ? resolveExportPath(dir, file->name) : std::nullopt) {
      target_uses[collisionKey(*target)]++;
    }
  }

  // Second read of the log: it may differ from the one that was scanned, so everything is re-checked below.
  io::MappedFile log;
  std::string open_error;
  const bool opened = log.open(log_path, &open_error);
  std::unique_ptr<Parser> parser;
  if (opened) {
    parser = std::make_unique<Parser>(log.data(), log.size());
  }

  for (const EmbeddedFile* requested : files) {
    if (requested == nullptr) {
      continue;
    }
    if (!opened) {
      fail(*requested, "cannot read log: " + open_error);
      continue;
    }
    const auto& fresh_files = parser->embeddedFiles();
    const auto fresh = std::find_if(fresh_files.begin(), fresh_files.end(),
                                    [&](const EmbeddedFile& f) { return f.name == requested->name; });
    if (fresh == fresh_files.end() || fresh->sha256 != requested->sha256) {
      fail(*requested, "log changed since it was scanned");
      continue;
    }

    if (fresh->gap_bytes > 0) {
      fail(*requested, std::to_string(fresh->gap_bytes) + " byte(s) missing between chunks, file is incomplete");
      continue;
    }

    const auto path = resolveExportPath(dir, requested->name);
    if (!path) {
      fail(*requested, "unsafe file name");
      continue;
    }
    if (target_uses[collisionKey(*path)] > 1) {
      fail(*requested, "another file in this export has the same target path");
      continue;
    }
    std::error_code ec;
    const auto existing = std::filesystem::symlink_status(*path, ec);
    const bool exists = std::filesystem::exists(existing);
    if (exists && !std::filesystem::is_regular_file(existing)) {
      fail(*requested, "target exists and is not a regular file");
      continue;
    }
    if (exists && policy == OverwritePolicy::kSkipExisting) {
      result.skipped.push_back(requested->name);
      continue;
    }

    std::filesystem::create_directories(path->parent_path(), ec);
    if (ec) {
      fail(*requested, "cannot create directory");
      continue;
    }

    // The temporary name must not belong to anything the user already has: pick the first unused ".part" name.
    auto part = *path;
    part += ".part";
    for (int n = 1; std::filesystem::exists(std::filesystem::symlink_status(part, ec)) && n < 1000; n++) {
      part = *path;
      part += "." + std::to_string(n) + ".part";
    }
    if (std::filesystem::exists(std::filesystem::symlink_status(part, ec))) {
      fail(*requested, "no free temporary file name");
      continue;
    }
    bool wrote = false;
    {
      std::ofstream out(part, std::ios::binary | std::ios::trunc);
      wrote = out.good() && streamEmbeddedFile(*fresh, log.data(), log.size(), [&out](const uint8_t* p, size_t n) {
        out.write(reinterpret_cast<const char*>(p), static_cast<std::streamsize>(n));
        return out.good();
      });
      out.close();
      wrote = wrote && !out.fail();
    }
    if (!wrote) {
      std::filesystem::remove(part, ec);
      fail(*requested, "could not write file");
      continue;
    }
    // The bytes were read from the mapping while writing; if the log was modified underneath it, its records no
    // longer match what was scanned.
    if (embeddedFileDigest(*fresh, log.data(), log.size()) != requested->sha256) {
      std::filesystem::remove(part, ec);
      fail(*requested, "log changed while exporting");
      continue;
    }
    // A file may have appeared since the check above; never replace it when the user chose to skip.
    if (policy == OverwritePolicy::kSkipExisting && std::filesystem::exists(std::filesystem::symlink_status(*path, ec))) {
      std::filesystem::remove(part, ec);
      result.skipped.push_back(requested->name);
      continue;
    }
    std::filesystem::rename(part, *path, ec);
    if (ec) {
      std::filesystem::remove(part, ec);
      fail(*requested, "could not move file into place");
      continue;
    }

    result.exported++;
    for (const auto& warning : embeddedFileWarnings(*requested)) {
      result.warnings.push_back(requested->name + ": " + warning);
    }
  }
  return result;
}

}  // namespace pj_drone::ardupilot
