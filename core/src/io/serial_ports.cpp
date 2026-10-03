/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "pj_drone_core/io/serial_ports.hpp"

#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <filesystem>
#endif

namespace pj_drone::io {

std::vector<SerialPortInfo> availableSerialPorts() {
  std::vector<SerialPortInfo> ports;

#ifdef _WIN32
  HKEY key = nullptr;
  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &key) == ERROR_SUCCESS) {
    for (DWORD index = 0;; ++index) {
      char value_name[256];
      DWORD value_name_size = sizeof(value_name);
      char data[256];
      DWORD data_size = sizeof(data) - 1;
      DWORD type = 0;
      const LONG rc = RegEnumValueA(
          key, index, value_name, &value_name_size, nullptr, &type, reinterpret_cast<LPBYTE>(data), &data_size);
      if (rc == ERROR_NO_MORE_ITEMS) {
        break;
      }
      if (rc != ERROR_SUCCESS || type != REG_SZ) {
        continue;
      }
      data[data_size] = '\0';
      const std::string name = data;
      ports.push_back({name, "\\\\.\\" + name});
    }
    RegCloseKey(key);
  }
#else
  namespace fs = std::filesystem;
  std::error_code ec;
  for (const auto& entry : fs::directory_iterator("/dev", ec)) {
    const std::string name = entry.path().filename().string();
#ifdef __APPLE__
    const bool is_serial = name.rfind("cu.", 0) == 0;
#else
    const bool is_serial = name.rfind("ttyUSB", 0) == 0 || name.rfind("ttyACM", 0) == 0 ||
                           name.rfind("ttyAMA", 0) == 0 || name.rfind("rfcomm", 0) == 0;
#endif
    if (is_serial) {
      ports.push_back({name, entry.path().string()});
    }
  }
#endif

  std::sort(ports.begin(), ports.end(), [](const SerialPortInfo& a, const SerialPortInfo& b) { return a.name < b.name; });
  return ports;
}

}  // namespace pj_drone::io
