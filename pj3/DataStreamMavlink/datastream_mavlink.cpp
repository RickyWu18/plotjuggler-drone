/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "datastream_mavlink.h"
#include "interval_settings_dialog.h"
#include "mavlink_constants.h"
#include "mavlink_controller.h"
#include "mavlink_parser.h"
#include "mavlink_transport.h"
#include "serial_transport.h"
#include "tcp_transport.h"
#include "udp_transport.h"
#include "ui_datastream_mavlink.h"

#include <QDialog>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QSettings>
#include <cstring>
#include <string>

using namespace PJ;

DataStreamMavlink::~DataStreamMavlink()
{
  shutdown();
  delete _intervalDialog;
  delete _controller;
}

bool DataStreamMavlink::start(QStringList*)
{
  QSettings settings;

  // One-time migration of old QSettings keys (:: separator → / separator).
  auto migrate = [&](const QString& oldKey, const QString& newKey) {
    if (settings.contains(oldKey) && !settings.contains(newKey))
    {
      settings.setValue(newKey, settings.value(oldKey));
      settings.remove(oldKey);
    }
  };
  migrate("DataStreamMavlink::transport", "DataStreamMavlink/transport");
  migrate("DataStreamMavlink::UDP::port", "DataStreamMavlink/UDP/port");
  migrate("DataStreamMavlink::TCP::host", "DataStreamMavlink/TCP/host");
  migrate("DataStreamMavlink::TCP::port", "DataStreamMavlink/TCP/port");
  migrate("DataStreamMavlink::Serial::port", "DataStreamMavlink/Serial/port");
  migrate("DataStreamMavlink::Serial::baudrate", "DataStreamMavlink/Serial/baudrate");

  // --- Config dialog ---
  QDialog dialog;
  Ui::MavlinkDialog ui;
  ui.setupUi(&dialog);

  for (qint32 baud : QSerialPortInfo::standardBaudRates())
    ui.baudCombo->addItem(QString::number(baud));

  auto refreshPorts = [&]() {
    ui.portCombo->clear();
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts())
      ui.portCombo->addItem(info.portName(), info.systemLocation());
  };
  refreshPorts();
  QObject::connect(ui.refreshBtn, &QPushButton::clicked, refreshPorts);

  // mode: 0=UDP, 1=Serial, 2=TCP
  auto toggleGroups = [&](int mode) {
    ui.udpGroup->setVisible(mode == 0);
    ui.tcpGroup->setVisible(mode == 2);
    ui.serialGroup->setVisible(mode == 1);
    dialog.adjustSize();
  };
  QObject::connect(ui.radioUdp, &QRadioButton::toggled, [&](bool checked) {
    if (checked)
      toggleGroups(0);
  });
  QObject::connect(ui.radioTcp, &QRadioButton::toggled, [&](bool checked) {
    if (checked)
      toggleGroups(2);
  });
  QObject::connect(ui.radioSerial, &QRadioButton::toggled, [&](bool checked) {
    if (checked)
      toggleGroups(1);
  });

  ui.portSpin->setValue(settings.value("DataStreamMavlink/UDP/port", 14550).toInt());
  ui.tcpHostEdit->setText(
      settings.value("DataStreamMavlink/TCP/host", "localhost").toString());
  ui.tcpPortSpin->setValue(settings.value("DataStreamMavlink/TCP/port", 5760).toInt());

  const int savedTransport = settings.value("DataStreamMavlink/transport", 0).toInt();
  ui.radioUdp->setChecked(savedTransport == 0);
  ui.radioTcp->setChecked(savedTransport == 2);
  ui.radioSerial->setChecked(savedTransport == 1);
  toggleGroups(savedTransport);

  const QString savedSerialPort =
      settings.value("DataStreamMavlink/Serial/port", "").toString();
  if (!savedSerialPort.isEmpty())
    ui.portCombo->setCurrentText(savedSerialPort);
  ui.baudCombo->setCurrentText(
      settings.value("DataStreamMavlink/Serial/baudrate", "57600").toString());

  QObject::connect(ui.buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(ui.buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() == QDialog::Rejected)
    return false;

  const int mode = ui.radioUdp->isChecked() ? 0 : (ui.radioTcp->isChecked() ? 2 : 1);
  settings.setValue("DataStreamMavlink/transport", mode);
  settings.setValue("DataStreamMavlink/UDP/port", ui.portSpin->value());
  settings.setValue("DataStreamMavlink/TCP/host", ui.tcpHostEdit->text());
  settings.setValue("DataStreamMavlink/TCP/port", ui.tcpPortSpin->value());
  settings.setValue("DataStreamMavlink/Serial/port", ui.portCombo->currentText());
  settings.setValue("DataStreamMavlink/Serial/baudrate",
                    ui.baudCombo->currentText().toInt());

  // --- Reset state ---
  {
    std::lock_guard<std::mutex> lock(mutex());
    dataMap().clear();
  }

  // --- Create transport ---
  _transport = createTransport(mode,
                               static_cast<quint16>(ui.portSpin->value()),
                               ui.tcpHostEdit->text(),
                               static_cast<quint16>(ui.tcpPortSpin->value()),
                               ui.portCombo->currentData().toString(),
                               ui.portCombo->currentText(),
                               ui.baudCombo->currentText().toInt());
  if (!_transport)
    return false;

  // --- Create parser + controller ---
  _parser = new MavlinkParser(this);
  _parser->reset();

  if (!_controller)
    _controller = new MavlinkController(this);
  _controller->setTransport(_transport);
  _controller->reset();

  // Transport → Parser → dataMap injection + Controller tracking
  connect(_transport, &MavlinkTransport::bytesReceived,
          _parser, &MavlinkParser::onBytesReceived);
  connect(_parser, &MavlinkParser::messageDecoded,
          this, &DataStreamMavlink::onMessageDecoded);
  connect(_parser, &MavlinkParser::messageDecoded,
          _controller, &MavlinkController::processDecodedMessage);
  // Emit dataReceived() once per read batch (after all messages in a batch are processed)
  connect(_parser, &MavlinkParser::batchProcessed,
          this, [this]() { emit dataReceived(); });

  // Wire the interval dialog now that the controller exists (if already built)
  if (_intervalDialog)
    _intervalDialog->setConnected(true);

  _running = true;
  return true;
}

void DataStreamMavlink::shutdown()
{
  if (!_running)
    return;
  _running = false;

  if (_intervalDialog)
    _intervalDialog->setConnected(false);

  if (_transport)
  {
    _transport->close();
    _transport->deleteLater();
    _transport = nullptr;
  }

  if (_parser)
  {
    _parser->deleteLater();
    _parser = nullptr;
  }

  // _controller is kept alive across start/shutdown cycles so the dialog
  // connections remain valid; reset clears its tracking tables.
  if (_controller)
    _controller->reset();

  emit closed();
}

const std::vector<QAction*>& DataStreamMavlink::availableActions()
{
  if (_actions.empty())
    buildActions();
  return _actions;
}

// --- Private ---

MavlinkTransport* DataStreamMavlink::createTransport(int mode, quint16 udpPort,
                                                      const QString& tcpHost, quint16 tcpPort,
                                                      const QString& serialPath,
                                                      const QString& serialDisplay, int baud)
{
  if (mode == 0)  // UDP
  {
    auto* t = new UdpTransport(udpPort, this);
    if (!t->open())
    {
      t->deleteLater();
      return nullptr;
    }
    return t;
  }
  else if (mode == 2)  // TCP
  {
    auto* t = new TcpTransport(tcpHost, tcpPort, this);
    connect(t, &MavlinkTransport::errorOccurred,
            [](const QString& msg) { QMessageBox::warning(nullptr, "MAVLink Streamer", msg); });
    if (!t->open())
    {
      t->deleteLater();
      return nullptr;
    }
    return t;
  }
  else  // Serial (mode == 1)
  {
    SerialConfig cfg{ serialPath, serialDisplay, baud };
    auto* t = new SerialTransport(cfg, this);
    connect(t, &MavlinkTransport::errorOccurred,
            [](const QString& msg) { QMessageBox::warning(nullptr, "MAVLink Streamer", msg); });
    if (!t->open())
    {
      t->deleteLater();
      return nullptr;
    }
    return t;
  }
}

void DataStreamMavlink::buildActions()
{
  // Lazily create the controller if start() hasn't been called yet.
  if (!_controller)
    _controller = new MavlinkController(this);

  _intervalDialog = new IntervalSettingsDialog(nullptr);
  _intervalDialog->setConnected(_running);
  connect(qApp, &QCoreApplication::aboutToQuit, _intervalDialog, &QWidget::close);

  // Controller ↔ Dialog
  connect(_controller, &MavlinkController::messageIntervalReceived,
          _intervalDialog, &IntervalSettingsDialog::onMessageIntervalReceived);
  connect(_controller, &MavlinkController::historyList,
          _intervalDialog, &IntervalSettingsDialog::onHistoryList);
  connect(_controller, &MavlinkController::endpointDiscovered,
          _intervalDialog, &IntervalSettingsDialog::onEndpointDiscovered);

  connect(_intervalDialog, &IntervalSettingsDialog::requestHistory,
          _controller, &MavlinkController::onRequestHistory);
  connect(_intervalDialog, &IntervalSettingsDialog::requestGet,
          _controller, &MavlinkController::onRequestGet);
  connect(_intervalDialog, &IntervalSettingsDialog::requestSet,
          _controller, &MavlinkController::onRequestSet);

  auto* act = new QAction("Message Intervals...", this);
  connect(act, &QAction::triggered, _intervalDialog, &QWidget::show);

  _actions = { act };
}

void DataStreamMavlink::onMessageDecoded(mavlink_message_t msg, double recvTimeSec)
{
  const mavlink_message_info_t* info = mavlink_get_message_info(&msg);
  if (!info)
    return;

  const char* payload = _MAV_PAYLOAD(&msg);
  const std::string prefix = std::string("mav/") + std::to_string(msg.sysid) + "." +
                              std::to_string(msg.compid) + "/" + info->name;

  // First pass: detect embedded hardware timestamp
  double timestamp = recvTimeSec;
  for (unsigned i = 0; i < info->num_fields; ++i)
  {
    const mavlink_field_info_t& f = info->fields[i];
    if (f.array_length != 0)
      continue;
    if (std::strcmp(f.name, Mav::FIELD_TIME_USEC) == 0 &&
        f.type == MAVLINK_TYPE_UINT64_T)
    {
      uint64_t v;
      std::memcpy(&v, payload + f.wire_offset, 8);
      if (v > 0)
        timestamp = static_cast<double>(v) * 1e-6;
      break;
    }
    if (std::strcmp(f.name, Mav::FIELD_TIME_BOOT_MS) == 0 &&
        f.type == MAVLINK_TYPE_UINT32_T)
    {
      uint32_t v;
      std::memcpy(&v, payload + f.wire_offset, 4);
      if (v > 0)
        timestamp = static_cast<double>(v) * 1e-3;
      break;
    }
  }

  // Second pass: push all numeric fields into dataMap (under mutex).
  // The lock is released before this function returns so that batchProcessed
  // (which emits dataReceived) is never called while the mutex is held.
  std::lock_guard<std::mutex> lock(mutex());
  for (unsigned i = 0; i < info->num_fields; ++i)
  {
    const mavlink_field_info_t& f = info->fields[i];
    if (f.type == MAVLINK_TYPE_CHAR)
      continue;

    const unsigned count = (f.array_length > 0) ? f.array_length : 1;
    const unsigned elemSize = Mav::TYPE_SIZE[f.type];

    for (unsigned j = 0; j < count; ++j)
    {
      std::string key = prefix + "/" + f.name;
      if (f.array_length > 0)
        key += "." + std::to_string(j);

      double value = 0.0;
      const unsigned offset = f.wire_offset + j * elemSize;
      switch (f.type)
      {
        case MAVLINK_TYPE_UINT8_T:
        case MAVLINK_TYPE_CHAR: {
          uint8_t v;
          std::memcpy(&v, payload + offset, 1);
          value = v;
          break;
        }
        case MAVLINK_TYPE_INT8_T: {
          int8_t v;
          std::memcpy(&v, payload + offset, 1);
          value = v;
          break;
        }
        case MAVLINK_TYPE_UINT16_T: {
          uint16_t v;
          std::memcpy(&v, payload + offset, 2);
          value = v;
          break;
        }
        case MAVLINK_TYPE_INT16_T: {
          int16_t v;
          std::memcpy(&v, payload + offset, 2);
          value = v;
          break;
        }
        case MAVLINK_TYPE_UINT32_T: {
          uint32_t v;
          std::memcpy(&v, payload + offset, 4);
          value = v;
          break;
        }
        case MAVLINK_TYPE_INT32_T: {
          int32_t v;
          std::memcpy(&v, payload + offset, 4);
          value = v;
          break;
        }
        case MAVLINK_TYPE_UINT64_T: {
          uint64_t v;
          std::memcpy(&v, payload + offset, 8);
          value = static_cast<double>(v);
          break;
        }
        case MAVLINK_TYPE_INT64_T: {
          int64_t v;
          std::memcpy(&v, payload + offset, 8);
          value = static_cast<double>(v);
          break;
        }
        case MAVLINK_TYPE_FLOAT: {
          float v;
          std::memcpy(&v, payload + offset, 4);
          value = v;
          break;
        }
        case MAVLINK_TYPE_DOUBLE: {
          std::memcpy(&value, payload + offset, 8);
          break;
        }
      }
      dataMap().getOrCreateNumeric(key).pushBack({ timestamp, value });
    }
  }
}
