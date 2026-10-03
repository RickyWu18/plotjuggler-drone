/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "serial_transport.h"

#include <chrono>

SerialTransport::SerialTransport(SerialConfig cfg, QObject* parent)
  : MavlinkTransport(parent), _cfg(std::move(cfg))
{
}

bool SerialTransport::open()
{
  _port = new QSerialPort(this);
  _port->setPortName(_cfg.systemPath);
  _port->setBaudRate(_cfg.baudRate);
  _port->setDataBits(QSerialPort::Data8);
  _port->setParity(QSerialPort::NoParity);
  _port->setStopBits(QSerialPort::OneStop);
  _port->setFlowControl(QSerialPort::NoFlowControl);

  if (!_port->open(QIODevice::ReadWrite))
  {
    emit errorOccurred(
        QString("Cannot open %1: %2").arg(_cfg.displayName, _port->errorString()));
    _port->deleteLater();
    _port = nullptr;
    return false;
  }
  connect(_port, &QSerialPort::readyRead, this, &SerialTransport::onReadyRead);
  return true;
}

void SerialTransport::close()
{
  if (_port)
  {
    _port->close();
    _port->deleteLater();
    _port = nullptr;
  }
}

bool SerialTransport::canWrite() const
{
  return _port && _port->isOpen();
}

qint64 SerialTransport::write(const QByteArray& data)
{
  return _port ? _port->write(data) : -1;
}

void SerialTransport::onReadyRead()
{
  using namespace std::chrono;
  const double ts =
      duration_cast<duration<double>>(system_clock::now().time_since_epoch()).count();

  while (_port && _port->bytesAvailable() > 0)
    emit bytesReceived(_port->read(4096), ts);
}
