// SPDX-License-Identifier: MIT

#pragma once

#include <QDialog>
#include <QList>
#include <QModelIndex>
#include <QString>
#include <vector>

#include "pj_drone_core/ardupilot/log_tools.hpp"
#include "pj_drone_core/ardupilot/parser.hpp"

namespace Ui {
class ArdupilotInfoDialog;
}

// Diagnostic timing/size figures shown in the Debug tab (debug builds only).
struct ApLoadStats {
  qint64 load_ms = 0;        // total wall-clock time to load (parse + write-through)
  qint64 file_size = 0;      // log file size in bytes
  size_t total_samples = 0;  // total number of data points emitted
  size_t series_count = 0;   // number of distinct numeric series
};

class ArdupilotInfoDialog : public QDialog {
  Q_OBJECT

 public:
  // `log_path` is the log the embedded files were indexed from: exporting reads their content from it again.
  explicit ArdupilotInfoDialog(
      const std::vector<pj_drone::ardupilot::Parameter>& params,
      const std::vector<pj_drone::ardupilot::EmbeddedFile>& files,
      const std::vector<pj_drone::ardupilot::LogMessage>& msgs, const QString& log_path, const ApLoadStats& stats = {},
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
  std::vector<pj_drone::ardupilot::Parameter> _params;
  std::vector<pj_drone::ardupilot::EmbeddedFile> _embeddedFiles;
  std::vector<pj_drone::ardupilot::FileCheck> _fileChecks;  // parallel to _embeddedFiles
  QString _logPath;
};
