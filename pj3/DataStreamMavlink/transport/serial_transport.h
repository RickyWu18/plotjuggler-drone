// SPDX-License-Identifier: MIT

#pragma once

#include "mavlink_transport.h"
#include <QSerialPort>
#include <QString>

struct SerialConfig
{
  QString systemPath;    // e.g. "COM3" or "/dev/ttyUSB0"
  QString displayName;   // shown in error messages
  int baudRate = 57600;
};

// Bidirectional serial transport (8N1, no flow control).
class SerialTransport : public MavlinkTransport
{
  Q_OBJECT

public:
  explicit SerialTransport(SerialConfig cfg, QObject* parent = nullptr);

  bool open() override;
  void close() override;

  bool canWrite() const override;
  qint64 write(const QByteArray& data) override;

private slots:
  void onReadyRead();

private:
  SerialConfig _cfg;
  QSerialPort* _port = nullptr;
};
