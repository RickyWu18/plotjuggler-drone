/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <QByteArray>
#include <QObject>

// Abstract I/O interface for MAVLink transports (UDP, serial, …).
// Each concrete implementation owns its socket/port and emits bytesReceived
// with a wall-clock receive timestamp (seconds since epoch).
class MavlinkTransport : public QObject
{
  Q_OBJECT

public:
  explicit MavlinkTransport(QObject* parent = nullptr) : QObject(parent)
  {
  }
  ~MavlinkTransport() override = default;

  virtual bool open() = 0;
  virtual void close() = 0;

  virtual bool canWrite() const = 0;
  virtual qint64 write(const QByteArray& data) = 0;

signals:
  void bytesReceived(QByteArray data, double recvTimeSec);
  void errorOccurred(QString message);
};
