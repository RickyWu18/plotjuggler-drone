/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "tcp_transport.h"

#include <chrono>

TcpTransport::TcpTransport(QString host, quint16 port, QObject* parent)
  : MavlinkTransport(parent), _host(std::move(host)), _port(port)
{
}

bool TcpTransport::open()
{
  _socket = new QTcpSocket(this);
  _socket->connectToHost(_host, _port);
  if (!_socket->waitForConnected(5000))
  {
    emit errorOccurred(
        QString("Cannot connect to %1:%2: %3").arg(_host).arg(_port).arg(_socket->errorString()));
    _socket->deleteLater();
    _socket = nullptr;
    return false;
  }
  connect(_socket, &QTcpSocket::readyRead, this, &TcpTransport::onReadyRead);
  return true;
}

void TcpTransport::close()
{
  if (_socket)
  {
    _socket->disconnectFromHost();
    _socket->deleteLater();
    _socket = nullptr;
  }
}

bool TcpTransport::canWrite() const
{
  return _socket && _socket->state() == QAbstractSocket::ConnectedState;
}

qint64 TcpTransport::write(const QByteArray& data)
{
  return _socket ? _socket->write(data) : -1;
}

void TcpTransport::onReadyRead()
{
  using namespace std::chrono;
  const double ts =
      duration_cast<duration<double>>(system_clock::now().time_since_epoch()).count();

  while (_socket && _socket->bytesAvailable() > 0)
    emit bytesReceived(_socket->read(4096), ts);
}
