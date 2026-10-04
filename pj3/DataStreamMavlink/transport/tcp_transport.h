// SPDX-License-Identifier: MIT

#pragma once

#include "mavlink_transport.h"
#include <QTcpSocket>
#include <QString>

// TCP client transport (PlotJuggler connects to SITL/vehicle).
// Bidirectional: reads incoming MAVLink bytes, writes outbound commands.
class TcpTransport : public MavlinkTransport
{
  Q_OBJECT

public:
  explicit TcpTransport(QString host, quint16 port, QObject* parent = nullptr);

  bool open() override;
  void close() override;

  bool canWrite() const override;
  qint64 write(const QByteArray& data) override;

private slots:
  void onReadyRead();

private:
  QString _host;
  quint16 _port;
  QTcpSocket* _socket = nullptr;
};
