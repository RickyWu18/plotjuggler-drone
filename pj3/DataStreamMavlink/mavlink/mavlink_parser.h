/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <QByteArray>
#include <QObject>

#define MAVLINK_USE_MESSAGE_INFO
#include "all/mavlink.h"

// Stateful MAVLink byte-stream accumulator.
// Owns the mavlink_parse_char() state; emits messageDecoded for each complete frame.
class MavlinkParser : public QObject
{
  Q_OBJECT

public:
  explicit MavlinkParser(QObject* parent = nullptr);

  void reset();

public slots:
  void onBytesReceived(QByteArray data, double recvTimeSec);

signals:
  void messageDecoded(mavlink_message_t msg, double recvTimeSec);
  // Emitted once at the end of each onBytesReceived() call (i.e. per readyRead batch).
  void batchProcessed();

private:
  mavlink_status_t _status{};
  mavlink_message_t _msg{};
  uint8_t _channel = MAVLINK_COMM_0;
};
