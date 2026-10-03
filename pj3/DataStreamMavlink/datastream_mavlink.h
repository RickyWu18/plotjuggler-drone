/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <PlotJuggler/datastreamer_base.h>
#include <QAction>

#define MAVLINK_USE_MESSAGE_INFO
#include "all/mavlink.h"

class MavlinkTransport;
class MavlinkParser;
class MavlinkController;
class IntervalSettingsDialog;

// Plugin facade: wires Transport → Parser → (Controller + dataMap injection).
// All MAVLink state is owned by dedicated classes; this class only holds
// the PlotJuggler mutex and dataMap contract.
class DataStreamMavlink : public PJ::DataStreamer
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.DataStreamer")
  Q_INTERFACES(PJ::DataStreamer)

public:
  DataStreamMavlink() = default;
  ~DataStreamMavlink() override;

  bool start(QStringList* selected) override;
  void shutdown() override;
  bool isRunning() const override
  {
    return _running;
  }
  const char* name() const override
  {
    return "MAVLink Streamer";
  }
  bool isDebugPlugin() override
  {
    return false;
  }
  const std::vector<QAction*>& availableActions() override;

private slots:
  void onMessageDecoded(mavlink_message_t msg, double recvTimeSec);

private:
  MavlinkTransport* createTransport(int mode, quint16 udpPort,
                                    const QString& tcpHost, quint16 tcpPort,
                                    const QString& serialPath,
                                    const QString& serialDisplay, int baud);
  void buildActions();

  bool _running = false;

  MavlinkTransport* _transport = nullptr;
  MavlinkParser* _parser = nullptr;
  MavlinkController* _controller = nullptr;
  IntervalSettingsDialog* _intervalDialog = nullptr;
  std::vector<QAction*> _actions;
};
