/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <QDialog>
#include <QList>
#include <QModelIndex>
#include <QString>
#include <vector>
#include "ardupilot_parser.h"

namespace Ui { class ArdupilotInfoDialog; }

// Diagnostic timing/size figures shown in the Debug tab (debug builds only).
struct ApLoadStats
{
  qint64 parse_ms     = 0;   // time spent decoding the .bin file
  qint64 write_ms     = 0;   // time spent pushing samples into PlotJuggler
  qint64 file_size    = 0;   // log file size in bytes
  size_t total_samples = 0;  // total number of data points emitted
  size_t series_count  = 0;  // number of distinct numeric series
};

class ArdupilotInfoDialog : public QDialog
{
  Q_OBJECT

public:
  explicit ArdupilotInfoDialog(const std::vector<ApParameter>& params,
                               const std::vector<ApEmbeddedFile>& files,
                               const std::vector<ApLogMessage>& msgs,
                               const ApLoadStats& stats = {},
                               QWidget* parent = nullptr);
  ~ArdupilotInfoDialog() override;

  void saveSettings();
  void restoreSettings();

private slots:
  void onSearchChanged(const QString& text);
  void onExport();
  void onExportSelected();
  void onExportAll();

private:
  void exportFilesToFolder(const QList<QModelIndex>& rows);
#ifdef ARDUPILOT_DEBUG_TAB
  void setupDebugTab(const ApLoadStats& stats);
#endif

  Ui::ArdupilotInfoDialog* ui;
  std::vector<ApEmbeddedFile> _embeddedFiles;
};
