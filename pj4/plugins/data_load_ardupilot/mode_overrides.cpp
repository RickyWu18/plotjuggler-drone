// SPDX-License-Identifier: MIT

#include "mode_overrides.hpp"

#include <charconv>
#include <fstream>
#include <nlohmann/json.hpp>
#include <pugixml.hpp>
#include <sstream>
#include <string_view>
#include <system_error>

namespace ardupilot_detail {

namespace ap = pj_drone::ardupilot;

namespace {

ModeOverridesResult fail(std::string message) {
  ModeOverridesResult result;
  result.error = std::move(message);
  return result;
}

bool parseModeNumber(const std::string& key, int* out) {
  const char* end = key.data() + key.size();
  const auto parsed = std::from_chars(key.data(), end, *out);
  return parsed.ec == std::errc() && parsed.ptr == end && *out >= 0 && *out <= 255;
}

// ---- MAVLink ardupilotmega.xml -------------------------------------------------------------------------------------

struct XmlModeEnum {
  std::string_view name;
  ap::VehicleType vehicle;
};
constexpr XmlModeEnum kXmlModeEnums[] = {
    {"COPTER_MODE", ap::VehicleType::kCopter},   {"PLANE_MODE", ap::VehicleType::kPlane},
    {"ROVER_MODE", ap::VehicleType::kRover},     {"SUB_MODE", ap::VehicleType::kSub},
    {"TRACKER_MODE", ap::VehicleType::kTracker},
};

// Same text rule as core/scripts/gen_mode_tables.py: the whitespace-collapsed <description>.
std::string collapseSpaces(std::string_view text) {
  std::string out;
  bool pending_space = false;
  for (const char c : text) {
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      pending_space = !out.empty();
    } else {
      if (pending_space) {
        out += ' ';
        pending_space = false;
      }
      out += c;
    }
  }
  return out;
}

ModeOverridesResult parseModeXml(const std::string& xml) {
  const auto bad = [](const std::string& why) { return fail("not usable ardupilotmega.xml: " + why); };

  pugi::xml_document doc;
  const pugi::xml_parse_result parsed = doc.load_buffer(xml.data(), xml.size());
  if (!parsed) {
    return bad(std::string(parsed.description()) + " (byte " + std::to_string(parsed.offset) + ")");
  }

  ModeOverridesResult result;
  for (const pugi::xml_node enum_node : doc.child("mavlink").child("enums").children("enum")) {
    const std::string_view enum_name = enum_node.attribute("name").value();
    const XmlModeEnum* known = nullptr;
    for (const auto& candidate : kXmlModeEnums) {
      if (candidate.name == enum_name) {
        known = &candidate;
      }
    }
    if (known == nullptr) {
      continue;
    }

    ap::ModeOverride entry;
    entry.vehicle = known->vehicle;
    const std::string prefix = std::string(known->name) + "_";
    for (const pugi::xml_node item : enum_node.children("entry")) {
      int number = 0;
      if (!parseModeNumber(item.attribute("value").value(), &number)) {
        return bad(std::string(known->name) + ": entry value must be an integer from 0 to 255");
      }
      std::string text = collapseSpaces(item.child("description").text().get());
      if (text.empty()) {  // the enum name without its prefix backs the description up
        const std::string_view name = item.attribute("name").value();
        if (name.substr(0, prefix.size()) == prefix) {
          text = std::string(name.substr(prefix.size()));
        }
      }
      if (text.empty()) {
        return bad(std::string(known->name) + ": entry " + std::to_string(number) + " has no name");
      }
      if (!entry.modes.emplace(number, std::move(text)).second) {
        return bad(std::string(known->name) + ": duplicate value " + std::to_string(number));
      }
    }
    if (entry.modes.empty()) {
      return bad(std::string(known->name) + " has no entries");
    }
    result.overrides.push_back(std::move(entry));
  }

  if (result.overrides.empty()) {
    return bad("none of COPTER_MODE, PLANE_MODE, ROVER_MODE, SUB_MODE, TRACKER_MODE was found");
  }
  return result;
}

// XML if the first character after an optional UTF-8 BOM and whitespace is '<'.
bool looksLikeXml(const std::string& text) {
  const size_t start = text.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
  const size_t first = text.find_first_not_of(" \t\r\n", start);
  return first != std::string::npos && text[first] == '<';
}

}  // namespace

ModeOverridesResult parseModeOverrides(const std::string& text) {
  return looksLikeXml(text) ? parseModeXml(text) : parseModeJson(text);
}

ModeOverridesResult parseModeJson(const std::string& json_text) {
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

  ModeOverridesResult result;
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

ModeOverridesResult loadModeOverrides(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return fail("cannot open the file");
  }
  std::ostringstream text;
  text << file.rdbuf();
  if (file.bad()) {
    return fail("cannot read the file");
  }
  return parseModeOverrides(text.str());
}

}  // namespace ardupilot_detail
