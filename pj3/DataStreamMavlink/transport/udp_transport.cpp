/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "udp_transport.h"

#include <QHostAddress>
#include <QNetworkDatagram>
#include <chrono>

UdpTransport::UdpTransport(quint16 port, QObject* parent)
  : MavlinkTransport(parent), _port(port)
{
}

bool UdpTransport::open()
{
  _socket = new QUdpSocket(this);
  if (!_socket->bind(QHostAddress::AnyIPv4, _port))
  {
    emit errorOccurred(_socket->errorString());
    _socket->deleteLater();
    _socket = nullptr;
    return false;
  }
  connect(_socket, &QUdpSocket::readyRead, this, &UdpTransport::onReadyRead);
  return true;
}

void UdpTransport::close()
{
  if (_socket)
  {
    _socket->close();
    _socket->deleteLater();
    _socket = nullptr;
  }
  _senderAddress = QHostAddress();
  _senderPort = 0;
}

bool UdpTransport::canWrite() const
{
  return _socket && !_senderAddress.isNull() && _senderPort != 0;
}

qint64 UdpTransport::write(const QByteArray& data)
{
  if (!canWrite())
    return -1;
  return _socket->writeDatagram(data, _senderAddress, _senderPort);
}

void UdpTransport::onReadyRead()
{
  using namespace std::chrono;
  const double ts =
      duration_cast<duration<double>>(system_clock::now().time_since_epoch()).count();

  while (_socket && _socket->hasPendingDatagrams())
  {
    QNetworkDatagram dg = _socket->receiveDatagram();
    if (!dg.senderAddress().isNull() && dg.senderPort() > 0)
    {
      _senderAddress = dg.senderAddress();
      _senderPort = static_cast<quint16>(dg.senderPort());
    }
    emit bytesReceived(dg.data(), ts);
  }
}
