/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mavlink_controller.h"
#include "mavlink_constants.h"

MavlinkController::MavlinkController(QObject* parent) : QObject(parent)
{
}

void MavlinkController::setTransport(MavlinkTransport* transport)
{
  _transport = transport;
}

void MavlinkController::reset()
{
  _seenMsgs.clear();
  _msgPrevTime.clear();
  _msgSmoothedHz.clear();
  _seenEndpoints.clear();
}

void MavlinkController::processDecodedMessage(const mavlink_message_t& msg, double recvTimeSec)
{
  const mavlink_message_info_t* info = mavlink_get_message_info(&msg);

  // --- Rate tracking with EMA smoothing ---
  constexpr double kAlpha = 0.1;
  const bool isNew = !_seenMsgs.contains(msg.msgid);

  double& prev = _msgPrevTime[msg.msgid];
  double& smoothed = _msgSmoothedHz[msg.msgid];

  if (prev > 0.0 && (recvTimeSec - prev) > 1e-4)
  {
    const double inst = 1.0 / (recvTimeSec - prev);
    smoothed = (smoothed > 0.0) ? kAlpha * inst + (1.0 - kAlpha) * smoothed : inst;
  }
  prev = recvTimeSec;

  const QString name = info ? QString(info->name) : QString("ID_%1").arg(msg.msgid);
  _seenMsgs[msg.msgid] = { name, smoothed };

  if (isNew)
    emit historyList(QVector<uint16_t>{ static_cast<uint16_t>(msg.msgid) },
                     QStringList{ name },
                     QVector<double>{ smoothed });

  // --- Endpoint discovery ---
  const MavlinkEndpoint ep{ msg.sysid, msg.compid };
  if (!_seenEndpoints.contains(ep))
  {
    _seenEndpoints.insert(ep);
    emit endpointDiscovered(msg.sysid, msg.compid);
  }

  // --- MESSAGE_INTERVAL (#244) relay ---
  if (msg.msgid == Mav::MSGID_MESSAGE_INTERVAL)
  {
    mavlink_message_interval_t mi;
    mavlink_msg_message_interval_decode(&msg, &mi);
    emit messageIntervalReceived(mi.message_id, mi.interval_us);
  }
}

void MavlinkController::onRequestHistory()
{
  QVector<uint16_t> ids;
  QStringList names;
  QVector<double> rates;
  ids.reserve(_seenMsgs.size());
  names.reserve(_seenMsgs.size());
  rates.reserve(_seenMsgs.size());

  for (auto it = _seenMsgs.constBegin(); it != _seenMsgs.constEnd(); ++it)
  {
    ids.append(it.key());
    names.append(it.value().first);
    rates.append(it.value().second);
  }
  emit historyList(ids, names, rates);

  for (const MavlinkEndpoint& ep : _seenEndpoints)
    emit endpointDiscovered(ep.sysid, ep.compid);
}

void MavlinkController::onRequestGet(uint8_t sysid, uint8_t compid, QVector<uint16_t> msgIds)
{
  for (uint16_t id : msgIds)
    sendRequestMessageInterval(sysid, compid, id);
}

void MavlinkController::onRequestSet(uint8_t sysid, uint8_t compid,
                                      QVector<QPair<uint16_t, int32_t>> intervals)
{
  if (!_transport || !_transport->canWrite())
    return;
  for (const auto& p : intervals)
    sendSetMessageInterval(sysid, compid, p.first, p.second);
}

// --- Private helpers ---

void MavlinkController::sendMavlinkMessage(const mavlink_message_t& msg)
{
  if (!_transport || !_transport->canWrite())
    return;
  uint8_t buf[MAVLINK_MAX_PACKET_LEN];
  const int len = mavlink_msg_to_send_buffer(buf, &msg);
  _transport->write(QByteArray(reinterpret_cast<const char*>(buf), len));
}

void MavlinkController::sendSetMessageInterval(uint8_t sysid, uint8_t compid,
                                                uint16_t msgId, int32_t intervalUs)
{
  mavlink_message_t msg;
  mavlink_msg_command_long_pack(Mav::GCS_SYSID, Mav::GCS_COMPID, &msg,
                                sysid, compid,
                                Mav::CMD_SET_MESSAGE_INTERVAL, 0,
                                static_cast<float>(msgId),
                                static_cast<float>(intervalUs),
                                0, 0, 0, 0, 0);
  sendMavlinkMessage(msg);
}

void MavlinkController::sendRequestMessageInterval(uint8_t sysid, uint8_t compid,
                                                    uint16_t msgId)
{
  mavlink_message_t msg;
  mavlink_msg_command_long_pack(Mav::GCS_SYSID, Mav::GCS_COMPID, &msg,
                                sysid, compid,
                                Mav::CMD_REQUEST_MESSAGE, 0,
                                static_cast<float>(Mav::MSGID_MESSAGE_INTERVAL),
                                static_cast<float>(msgId),
                                0, 0, 0, 0, 0);
  sendMavlinkMessage(msg);
}
