/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <cstdint>
#include <QString>

// Typed replacement for the raw (sysid<<8)|compid uint16_t pattern.
struct MavlinkEndpoint
{
  uint8_t sysid = 0;
  uint8_t compid = 0;

  uint16_t packed() const
  {
    return (static_cast<uint16_t>(sysid) << 8) | compid;
  }

  bool operator==(const MavlinkEndpoint& o) const
  {
    return sysid == o.sysid && compid == o.compid;
  }

  QString label() const
  {
    return QString("SYS %1 / COMP %2").arg(sysid).arg(compid);
  }

  static MavlinkEndpoint fromPacked(uint16_t p)
  {
    return { static_cast<uint8_t>(p >> 8), static_cast<uint8_t>(p & 0xFF) };
  }
};

inline uint qHash(const MavlinkEndpoint& ep, uint seed = 0)
{
  return static_cast<uint>(ep.packed()) ^ seed;
}
