// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>
#include <QtPlugin>
#include "PlotJuggler/dataloader_base.h"

class DataLoadArdupilot : public PJ::DataLoader
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.DataLoader")
  Q_INTERFACES(PJ::DataLoader)

public:
  DataLoadArdupilot()          = default;
  ~DataLoadArdupilot() override = default;

  const std::vector<const char*>& compatibleFileExtensions() const override;

  bool readDataFromFile(PJ::FileLoadInfo* fileload_info,
                        PJ::PlotDataMapRef& destination) override;

  const char* name() const override { return "DataLoad ArduPilot BIN"; }
};
