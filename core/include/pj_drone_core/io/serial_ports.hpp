/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <string>
#include <vector>

namespace pj_drone::io {

struct SerialPortInfo {
  std::string name;             // short name shown to the user, e.g. "COM3" or "ttyUSB0"
  std::string system_location;  // what to open, e.g. "\\\\.\\COM3" or "/dev/ttyUSB0"
};

/// Serial ports currently present on the machine, sorted by name.
std::vector<SerialPortInfo> availableSerialPorts();

}  // namespace pj_drone::io
