// SPDX-License-Identifier: MIT

#include "pj_drone_core/ardupilot/mode_xml.hpp"

#ifdef PJ_DRONE_HAS_MODE_XML

#include <charconv>
#include <pugixml.hpp>
#include <system_error>

namespace pj_drone::ardupilot {

namespace {

ModeOverridesResult fail(const std::string& why) {
  ModeOverridesResult result;
  result.error = "not usable ardupilotmega.xml: " + why;
  return result;
}

struct XmlModeEnum {
  std::string_view name;
  VehicleType vehicle;
};
constexpr XmlModeEnum kXmlModeEnums[] = {
    {"COPTER_MODE", VehicleType::kCopter}, {"PLANE_MODE", VehicleType::kPlane},     {"ROVER_MODE", VehicleType::kRover},
    {"SUB_MODE", VehicleType::kSub},       {"TRACKER_MODE", VehicleType::kTracker},
};

bool parseModeNumber(const char* text, int* out) {
  const std::string_view view(text);
  const auto parsed = std::from_chars(view.data(), view.data() + view.size(), *out);
  return parsed.ec == std::errc() && parsed.ptr == view.data() + view.size() && *out >= 0 && *out <= 255;
}

// The whitespace-collapsed text: runs of blanks become one space, none at either end.
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

}  // namespace

ModeOverridesResult parseMavlinkModeXml(std::string_view xml) {
  pugi::xml_document doc;
  const pugi::xml_parse_result parsed = doc.load_buffer(xml.data(), xml.size());
  if (!parsed) {
    return fail(std::string(parsed.description()) + " (byte " + std::to_string(parsed.offset) + ")");
  }

  ModeOverridesResult result;
  for (const pugi::xml_node enum_node : doc.child("mavlink").child("enums").children("enum")) {
    const std::string_view enum_name = enum_node.attribute("name").value();
    const XmlModeEnum* known = nullptr;
    for (const XmlModeEnum& candidate : kXmlModeEnums) {
      if (candidate.name == enum_name) {
        known = &candidate;
        break;
      }
    }
    if (known == nullptr) {
      continue;
    }

    ModeOverride entry;
    entry.vehicle = known->vehicle;
    const std::string prefix = std::string(known->name) + "_";
    for (const pugi::xml_node item : enum_node.children("entry")) {
      int number = 0;
      if (!parseModeNumber(item.attribute("value").value(), &number)) {
        return fail(std::string(known->name) + ": entry value must be an integer from 0 to 255");
      }
      std::string text = collapseSpaces(item.child("description").text().get());
      if (text.empty()) {  // the enum name without its prefix backs the description up
        const std::string_view name = item.attribute("name").value();
        if (name.substr(0, prefix.size()) == prefix) {
          text = std::string(name.substr(prefix.size()));
        }
      }
      if (text.empty()) {
        return fail(std::string(known->name) + ": entry " + std::to_string(number) + " has no name");
      }
      if (!entry.modes.emplace(number, std::move(text)).second) {
        return fail(std::string(known->name) + ": duplicate value " + std::to_string(number));
      }
    }
    if (entry.modes.empty()) {
      return fail(std::string(known->name) + " has no entries");
    }
    result.overrides.push_back(std::move(entry));
  }

  if (result.overrides.empty()) {
    return fail("none of COPTER_MODE, PLANE_MODE, ROVER_MODE, SUB_MODE, TRACKER_MODE was found");
  }
  return result;
}

}  // namespace pj_drone::ardupilot

#else  // no pugixml

namespace pj_drone::ardupilot {

ModeOverridesResult parseMavlinkModeXml(std::string_view /*xml*/) {
  ModeOverridesResult result;
  result.error = "this build cannot read MAVLink XML";
  return result;
}

}  // namespace pj_drone::ardupilot

#endif
