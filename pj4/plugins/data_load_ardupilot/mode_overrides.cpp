// SPDX-License-Identifier: MIT

#include "mode_overrides.hpp"

#include <charconv>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <system_error>

#include "pj_drone_core/ardupilot/mode_xml.hpp"

namespace ardupilot_detail {

namespace ap = pj_drone::ardupilot;

namespace {

ap::ModeOverridesResult fail(std::string message) {
  ap::ModeOverridesResult result;
  result.error = std::move(message);
  return result;
}

bool parseModeNumber(const std::string& key, int* out) {
  const char* end = key.data() + key.size();
  const auto parsed = std::from_chars(key.data(), end, *out);
  return parsed.ec == std::errc() && parsed.ptr == end && *out >= 0 && *out <= 255;
}

// XML if the first character after an optional UTF-8 BOM and whitespace is '<'.
bool looksLikeXml(const std::string& text) {
  const size_t start = text.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
  const size_t first = text.find_first_not_of(" \t\r\n", start);
  return first != std::string::npos && text[first] == '<';
}

ap::ModeOverridesResult parseModeJson(const std::string& json_text) {
  const nlohmann::json root = nlohmann::json::parse(json_text, nullptr, false);
  if (root.is_discarded()) {
    return fail("not valid JSON");
  }
  if (!root.is_object()) {
    return fail("the top level must be an object");
  }
  const auto firmwares = root.find("firmwares");
  if (firmwares == root.end() || !firmwares->is_array()) {
    return fail("\"firmwares\" must be an array");
  }

  ap::ModeOverridesResult result;
  size_t index = 0;
  for (const auto& item : *firmwares) {
    const std::string where = "firmwares[" + std::to_string(index++) + "]: ";
    if (!item.is_object()) {
      return fail(where + "must be an object");
    }
    ap::ModeOverride entry;

    if (const auto it = item.find("match"); it != item.end()) {
      if (!it->is_string()) {
        return fail(where + "\"match\" must be a string");
      }
      entry.match = it->get<std::string>();
    }

    const auto modes = item.find("modes");
    if (modes == item.end() || !modes->is_object() || modes->empty()) {
      return fail(where + "\"modes\" must be a non-empty object");
    }
    for (const auto& [key, value] : modes->items()) {
      int mode = 0;
      if (!parseModeNumber(key, &mode)) {
        return fail(where + "mode number \"" + key + "\" must be an integer from 0 to 255");
      }
      if (!value.is_string() || value.get<std::string>().empty()) {
        return fail(where + "the name of mode " + key + " must be a non-empty string");
      }
      entry.modes[mode] = value.get<std::string>();
    }
    result.overrides.push_back(std::move(entry));
  }
  return result;
}

}  // namespace

ap::ModeOverridesResult loadModeOverrides(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return fail("cannot open the file");
  }
  std::ostringstream text;
  text << file.rdbuf();
  if (file.bad()) {
    return fail("cannot read the file");
  }
  const std::string content = text.str();
  return looksLikeXml(content) ? ap::parseMavlinkModeXml(content) : parseModeJson(content);
}

}  // namespace ardupilot_detail
