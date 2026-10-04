// SPDX-License-Identifier: MIT

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
