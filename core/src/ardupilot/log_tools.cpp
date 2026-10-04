// SPDX-License-Identifier: MIT

#include "pj_drone_core/ardupilot/log_tools.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>

namespace pj_drone::ardupilot {

namespace {

std::string toLower(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

bool isSafeRelativePath(const std::filesystem::path& rel) {
  if (rel.empty() || rel.is_absolute() || rel.has_root_name() || rel.has_root_directory()) {
    return false;
  }
  return std::none_of(rel.begin(), rel.end(), [](const std::filesystem::path& part) { return part == ".."; });
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

ExportResult exportEmbeddedFiles(const std::vector<const EmbeddedFile*>& files, const std::filesystem::path& dir) {
  ExportResult result;
  for (const EmbeddedFile* file : files) {
    if (file == nullptr) {
      continue;
    }
    const std::filesystem::path rel{std::u8string(file->name.begin(), file->name.end())};
    if (!isSafeRelativePath(rel)) {
      result.failed.push_back(file->name);
      continue;
    }

    const std::filesystem::path path = dir / rel;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream out(path, std::ios::binary);
    if (!out) {
      result.failed.push_back(file->name);
      continue;
    }
    out.write(reinterpret_cast<const char*>(file->data.data()), static_cast<std::streamsize>(file->data.size()));
    if (!out.good()) {
      result.failed.push_back(file->name);
      continue;
    }
    result.exported++;
  }
  return result;
}

}  // namespace pj_drone::ardupilot
