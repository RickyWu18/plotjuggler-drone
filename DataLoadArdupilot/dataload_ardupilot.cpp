/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "dataload_ardupilot.h"
#include "ardupilot_parser.h"
#include "ardupilot_info_dialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProgressBar>
#include <QProgressDialog>
#include <QSettings>
#include <QVBoxLayout>
#include <QtCore/qscopeguard.h>
#include <stdexcept>

const std::vector<const char*>& DataLoadArdupilot::compatibleFileExtensions() const
{
  static std::vector<const char*> exts = { "BIN" };
  return exts;
}

bool DataLoadArdupilot::readDataFromFile(PJ::FileLoadInfo* info,
                                         PJ::PlotDataMapRef& dest)
{
  QSettings pref("DataLoadArdupilot", "settings");
  bool show_units        = pref.value("show_units", false).toBool();
  bool load_files        = pref.value("load_files", true).toBool();
  bool official_compat   = pref.value("official_compat", false).toBool();

  {
    QDialog dlg;
    dlg.setWindowTitle("ArduPilot Load Settings");
    auto* layout      = new QVBoxLayout(&dlg);
    auto* cb_units    = new QCheckBox("Append units to series names (e.g. Roll \xe2\x86\x92 Roll(deg))", &dlg);
    auto* cb_files    = new QCheckBox("Load embedded FILE messages (may be slow for large logs)", &dlg);
    auto* cb_official = new QCheckBox("Use official ArduPilot plugin naming format\n"
                                      "(leading slash and # before instance: ATT/0/Roll \xe2\x86\x92 /ATT/#0/Roll)", &dlg);
    auto* buttons     = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    cb_units->setChecked(show_units);
    cb_files->setChecked(load_files);
    cb_official->setChecked(official_compat);
    layout->addWidget(cb_units);
    layout->addWidget(cb_files);
    layout->addWidget(cb_official);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return false;
    show_units      = cb_units->isChecked();
    load_files      = cb_files->isChecked();
    official_compat = cb_official->isChecked();
    pref.setValue("show_units", show_units);
    pref.setValue("load_files", load_files);
    pref.setValue("official_compat", official_compat);
  }

  QFile file(info->filename);
  if (!file.open(QIODevice::ReadOnly))
  {
    throw std::runtime_error("ArduPilot: failed to open file: " +
                             info->filename.toStdString());
  }

  const qint64 file_size = file.size();
  if (file_size == 0) return false;

  const uchar* mapped = file.map(0, file_size);
  if (!mapped)
  {
    throw std::runtime_error("ArduPilot: failed to memory-map file");
  }
  auto unmap_guard = qScopeGuard([&]{ file.unmap(const_cast<uchar*>(mapped)); });  // D4

  QWidget* main_window = QApplication::activeWindow();

  QProgressDialog progress_dialog;
  progress_dialog.setWindowTitle("Loading ArduPilot log");
  progress_dialog.setLabelText("Decoding log file...");
  progress_dialog.setWindowModality(Qt::ApplicationModal);
  progress_dialog.setRange(0, 100);
  progress_dialog.setAutoClose(false);
  progress_dialog.setAutoReset(false);
  progress_dialog.show();

  if (auto* bar = progress_dialog.findChild<QProgressBar*>())
  {
    bar->setTextVisible(true);
    bar->setAlignment(Qt::AlignCenter);
  }

  QElapsedTimer parse_timer;   // throttle timer for progress callback
  QElapsedTimer load_timer;    // total load time for Debug tab
  parse_timer.start();
  load_timer.start();
  ArdupilotParser parser(
      reinterpret_cast<const uint8_t*>(mapped),
      static_cast<size_t>(file_size),
      load_files,
      official_compat,
      [&](size_t pos, size_t total) -> bool {
        if (parse_timer.elapsed() < 50) return true;
        parse_timer.restart();
        progress_dialog.setValue(static_cast<int>(100.0 * pos / total));
        QApplication::processEvents();
        return !progress_dialog.wasCanceled();
      },
      // Write-through sink: build display key and obtain PlotData* once per series (⑤)
      [&](const std::string& key, const std::string& unit) -> PJ::PlotData* {
        std::string disp_unit;
        disp_unit.reserve(unit.size() + 4);
        for (char ch : unit)
          ch == '/' ? disp_unit += "\xe2\x88\x95" : disp_unit += ch;

        std::string display_key;
        if (official_compat) display_key = "/" + key;
        else                 display_key = key;
        if (show_units && !disp_unit.empty())
          display_key.append("(").append(disp_unit).append(")");

        return &dest.getOrCreateNumeric(display_key);
      },
      // String series sink
      [&](const std::string& key) -> PJ::StringSeries* {
        std::string display_key = official_compat ? "/" + key : key;
        return &dest.getOrCreateStringSeries(display_key);
      });

  if (progress_dialog.wasCanceled())
  {
    dest.clear();  // write-through: partial data may already be in dest
    return false;
  }

  const auto& series_map     = parser.getSeriesMap();
  const size_t total_samples = parser.getTotalSamples();

  ApLoadStats stats;
  stats.load_ms       = load_timer.elapsed();
  stats.file_size     = file_size;
  stats.total_samples = total_samples;
  stats.series_count  = series_map.size();

  auto* dlg = new ArdupilotInfoDialog(parser.getParameters(),
                                      parser.getEmbeddedFiles(),
                                      parser.getLogMessages(),
                                      stats,
                                      main_window);
  dlg->setWindowTitle(
      QString("ArduPilot log: %1")
          .arg(QFileInfo(info->filename).fileName()));
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->restoreSettings();
  dlg->show();

  progress_dialog.setValue(100);
  progress_dialog.close();

  return true;
}
