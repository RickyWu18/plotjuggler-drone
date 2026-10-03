/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <cstdint>

// All MAVLink magic values in one place.
// Indices for TYPE_SIZE match MAVLINK_TYPE_* enum (0=CHAR, 1=UINT8, 2=INT8,
// 3=UINT16, 4=INT16, 5=UINT32, 6=INT32, 7=UINT64, 8=INT64, 9=FLOAT, 10=DOUBLE).
namespace Mav
{
constexpr uint8_t GCS_SYSID = 255;
constexpr uint8_t GCS_COMPID = 0;

constexpr uint16_t CMD_SET_MESSAGE_INTERVAL = 511;
constexpr uint16_t CMD_REQUEST_MESSAGE = 512;

constexpr uint32_t MSGID_MESSAGE_INTERVAL = 244;

constexpr const char* FIELD_TIME_USEC = "time_usec";
constexpr const char* FIELD_TIME_BOOT_MS = "time_boot_ms";

constexpr uint8_t TYPE_SIZE[11] = { 1, 1, 1, 2, 2, 4, 4, 8, 8, 4, 8 };
}  // namespace Mav
