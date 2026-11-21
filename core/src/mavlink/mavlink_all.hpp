// SPDX-License-Identifier: MIT

#pragma once

// The one place that includes the MAVLink C headers (private to core and its tests).
#define MAVLINK_USE_MESSAGE_INFO
#ifdef _MSC_VER
#pragma warning(push, 0)  // third-party header: MSVC (VS generators) does not honor SYSTEM
#endif
#include "all/mavlink.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
