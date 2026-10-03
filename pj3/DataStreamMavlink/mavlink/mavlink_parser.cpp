/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mavlink_parser.h"

MavlinkParser::MavlinkParser(QObject* parent) : QObject(parent)
{
}

void MavlinkParser::reset()
{
  _status = {};
  _msg = {};
}

void MavlinkParser::onBytesReceived(QByteArray data, double recvTimeSec)
{
  const auto* bytes = reinterpret_cast<const uint8_t*>(data.constData());
  for (int i = 0; i < data.size(); ++i)
  {
    if (mavlink_parse_char(_channel, bytes[i], &_msg, &_status))
      emit messageDecoded(_msg, recvTimeSec);
  }
  emit batchProcessed();
}
