/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "mavlink_endpoint.h"
#include "mavlink_transport.h"

#include <QHash>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QStringList>
#include <QVector>

#define MAVLINK_USE_MESSAGE_INFO
#include "all/mavlink.h"

// Owns message-rate tracking, endpoint discovery, and outbound MAVLink commands.
// Decouples IntervalSettingsDialog from DataStreamMavlink: the dialog only
// sees this controller's signals/slots, not the plugin facade.
class MavlinkController : public QObject
{
  Q_OBJECT

public:
  explicit MavlinkController(QObject* parent = nullptr);

  void setTransport(MavlinkTransport* transport);
  void reset();

  // Called by DataStreamMavlink for every decoded frame (tracking only — no dataMap access).
  void processDecodedMessage(const mavlink_message_t& msg, double recvTimeSec);

public slots:
  void onRequestHistory();
  void onRequestGet(uint8_t sysid, uint8_t compid, QVector<uint16_t> msgIds);
  void onRequestSet(uint8_t sysid, uint8_t compid,
                    QVector<QPair<uint16_t, int32_t>> intervals);

signals:
  void messageIntervalReceived(uint16_t msgId, int32_t intervalUs);
  void historyList(QVector<uint16_t> ids, QStringList names, QVector<double> rates);
  void endpointDiscovered(uint8_t sysid, uint8_t compid);

private:
  void sendMavlinkMessage(const mavlink_message_t& msg);
  void sendSetMessageInterval(uint8_t sysid, uint8_t compid,
                               uint16_t msgId, int32_t intervalUs);
  void sendRequestMessageInterval(uint8_t sysid, uint8_t compid, uint16_t msgId);

  MavlinkTransport* _transport = nullptr;

  // msgid -> {name, smoothed_hz}
  QHash<uint16_t, QPair<QString, double>> _seenMsgs;
  QHash<uint16_t, double> _msgPrevTime;
  QHash<uint16_t, double> _msgSmoothedHz;
  QSet<MavlinkEndpoint> _seenEndpoints;
};
