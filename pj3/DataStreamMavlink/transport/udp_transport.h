/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "mavlink_transport.h"
#include <QHostAddress>
#include <QUdpSocket>

// UDP bidirectional transport.
// Binds to AnyIPv4 on the given port. The sender address/port of the most
// recently received datagram is stored and used as the write destination,
// enabling SET_MESSAGE_INTERVAL commands to be sent back to the vehicle/SITL.
class UdpTransport : public MavlinkTransport
{
  Q_OBJECT

public:
  explicit UdpTransport(quint16 port, QObject* parent = nullptr);

  bool open() override;
  void close() override;

  bool canWrite() const override;
  qint64 write(const QByteArray& data) override;

private slots:
  void onReadyRead();

private:
  quint16 _port;
  QUdpSocket* _socket = nullptr;
  QHostAddress _senderAddress;
  quint16 _senderPort = 0;
};
