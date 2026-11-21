// SPDX-License-Identifier: MIT

#include "ardupilot_info_dialog.h"
#include "ui_ardupilot_info_dialog.h"
#include "qt_path.h"

#include "pj_drone_core/ardupilot/log_tools.hpp"

#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QPair>
#include <QPushButton>
#include <QSettings>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QVector>
#include <algorithm>

namespace ap = pj_drone::ardupilot;

namespace
{

QString statusText(ap::FileStatus status)
{
  switch (status)
  {
    case ap::FileStatus::kWarning:      return "Warning";
    case ap::FileStatus::kCannotExport: return "Cannot export";
    case ap::FileStatus::kOk:           break;
  }
  return "OK";
}

QString joinLines(const std::vector<std::string>& lines)
{
  QStringList out;
  for (const auto& line : lines) out << QString::fromStdString(line);
  return out.join("\n");
}

}  // namespace

ArdupilotInfoDialog::ArdupilotInfoDialog(const std::vector<ap::Parameter>& params,
                                         const std::vector<ap::EmbeddedFile>& files,
                                         const std::vector<ap::LogMessage>& msgs,
                                         const QString& log_path,
                                         const ApLoadStats& stats,
                                         QWidget* parent)
  : QDialog(parent), ui(new Ui::ArdupilotInfoDialog), _params(params), _embeddedFiles(files),
    _logPath(log_path)
{
  ui->setupUi(this);

  _fileChecks.reserve(files.size());
  for (const auto& f : files) _fileChecks.push_back(ap::checkEmbeddedFile(f));

  // --- Parameters tab ---
  auto* table = ui->tableParams;
  table->setUpdatesEnabled(false);
  table->setSortingEnabled(false);
  table->setRowCount(static_cast<int>(params.size()));
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->verticalHeader()->setVisible(false);
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

  for (int row = 0; row < static_cast<int>(params.size()); row++)
  {
    auto* nameItem = new QTableWidgetItem(QString::fromStdString(params[row].name));
    // Store the _params index so the mapping survives column sorting
    nameItem->setData(Qt::UserRole, row);
    table->setItem(row, 0, nameItem);
    table->setItem(row, 1,
        new QTableWidgetItem(QString::fromStdString(ap::formatParamValue(params[row].value))));
  }
  // Set ResizeToContents after all items are inserted — avoids O(n²) rescans
  table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);
  table->setSortingEnabled(true);
  table->setUpdatesEnabled(true);

  // Match search box font and height to the parameter name rows
  ui->searchEdit->setFont(table->font());
  ui->searchEdit->setMinimumHeight(table->verticalHeader()->defaultSectionSize());

  connect(ui->searchEdit, &QLineEdit::textChanged, this, &ArdupilotInfoDialog::onSearchChanged);
  connect(ui->btnExport,  &QPushButton::clicked,   this, &ArdupilotInfoDialog::onExport);
  connect(ui->buttonBox,  &QDialogButtonBox::rejected, this, &QDialog::reject);

  // --- Embedded Files tab ---
  auto* ftable = ui->tableFiles;
  ftable->setUpdatesEnabled(false);
  ftable->setSortingEnabled(false);
  ftable->setRowCount(static_cast<int>(files.size()));
  ftable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  ftable->verticalHeader()->setVisible(false);
  ftable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

  for (int row = 0; row < static_cast<int>(files.size()); row++)
  {
    auto* nameItem = new QTableWidgetItem(QString::fromStdString(files[row].name));
    // Store the original _embeddedFiles index so the mapping survives column sorting
    nameItem->setData(Qt::UserRole, row);
    ftable->setItem(row, 0, nameItem);
    ftable->setItem(row, 1,
        new QTableWidgetItem(
            QString::number(static_cast<qulonglong>(files[row].size)) + " bytes"));

    // Bytes actually backed by chunk data (Size minus the gaps no chunk provides)
    ftable->setItem(row, 2,
        new QTableWidgetItem(
            QString::number(static_cast<qulonglong>(files[row].covered_bytes)) + " bytes"));

    const auto& check = _fileChecks[static_cast<size_t>(row)];
    auto* statusItem = new QTableWidgetItem(statusText(check.status));
    statusItem->setToolTip(joinLines(check.details));
    ftable->setItem(row, 3, statusItem);
  }
  for (int col = 1; col <= 3; col++)
    ftable->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
  ftable->setSortingEnabled(true);
  ftable->setUpdatesEnabled(true);

  // "Export All" is available when at least one file can be exported
  ui->btnExportAll->setEnabled(
      std::any_of(_fileChecks.begin(), _fileChecks.end(), [](const ap::FileCheck& c) {
        return c.status != ap::FileStatus::kCannotExport;
      }));

  connect(ftable, &QTableWidget::itemSelectionChanged, this, [this](){
    ui->btnExportSelected->setEnabled(!ui->tableFiles->selectedItems().isEmpty());
  });
  connect(ui->btnExportSelected, &QPushButton::clicked,
          this, &ArdupilotInfoDialog::onExportSelected);
  connect(ui->btnExportAll, &QPushButton::clicked,
          this, &ArdupilotInfoDialog::onExportAll);

  // --- Messages tab ---
  auto* mtable = ui->tableMsgs;
  mtable->setUpdatesEnabled(false);
  mtable->setRowCount(static_cast<int>(msgs.size()));
  mtable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  mtable->verticalHeader()->setVisible(false);
  mtable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

  for (int row = 0; row < static_cast<int>(msgs.size()); row++)
  {
    auto* tsItem = new QTableWidgetItem(
        QString::fromStdString(ap::formatMessageTimestamp(msgs[row].timestamp)));
    tsItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mtable->setItem(row, 0, tsItem);
    mtable->setItem(row, 1,
        new QTableWidgetItem(QString::fromStdString(msgs[row].message)));
  }
  mtable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  mtable->setUpdatesEnabled(true);

#ifdef ARDUPILOT_DEBUG_TAB
  // --- Debug tab (RelWithDebInfo builds only) ---
  setupDebugTab(stats);
#else
  Q_UNUSED(stats);
#endif
}

ArdupilotInfoDialog::~ArdupilotInfoDialog()
{
  saveSettings();
  delete ui;
}

void ArdupilotInfoDialog::onSearchChanged(const QString& text)
{
  auto* table = ui->tableParams;
  const ap::ParamFilter filter(text.toStdString());
  table->setUpdatesEnabled(false);
  for (int row = 0; row < table->rowCount(); row++)
  {
    auto* item = table->item(row, 0);
    table->setRowHidden(row, !item || !filter.matches(item->text().toStdString()));
  }
  table->setUpdatesEnabled(true);
}

void ArdupilotInfoDialog::onExport()
{
  const QString path = QFileDialog::getSaveFileName(
      this, "Export Parameters", QString(), "Parameter files (*.param)");
  if (path.isEmpty()) return;

  // The visible rows, in the order the table currently shows them
  std::vector<ap::Parameter> visible;
  auto* table = ui->tableParams;
  for (int row = 0; row < table->rowCount(); row++)
  {
    if (table->isRowHidden(row)) continue;
    const int idx = table->item(row, 0)->data(Qt::UserRole).toInt();
    visible.push_back(_params[static_cast<size_t>(idx)]);
  }

  if (!ap::writeTextFile(pathFromQString(path), ap::buildParamFileText(visible)))
  {
    QMessageBox::warning(this, "Export Failed",
                         "Could not open file for writing:\n" + path);
  }
}

void ArdupilotInfoDialog::onExportSelected()
{
  const QList<QModelIndex> rows =
      ui->tableFiles->selectionModel()->selectedRows();
  if (rows.isEmpty()) return;
  exportFilesToFolder(rows);
}

void ArdupilotInfoDialog::onExportAll()
{
  QList<QModelIndex> all;
  for (int row = 0; row < ui->tableFiles->rowCount(); row++)
    all << ui->tableFiles->model()->index(row, 0);
  exportFilesToFolder(all);
}

void ArdupilotInfoDialog::exportFilesToFolder(const QList<QModelIndex>& rows)
{
  // Files flagged "Cannot export" are already explained in the table; skip them.
  std::vector<const ap::EmbeddedFile*> toExport;
  for (const auto& idx : rows)
  {
    auto* nameItem = ui->tableFiles->item(idx.row(), 0);
    if (!nameItem) continue;

    const int ef_idx = nameItem->data(Qt::UserRole).toInt();
    if (ef_idx < 0 || ef_idx >= static_cast<int>(_embeddedFiles.size())) continue;

    if (_fileChecks[static_cast<size_t>(ef_idx)].status == ap::FileStatus::kCannotExport) continue;
    toExport.push_back(&_embeddedFiles[static_cast<size_t>(ef_idx)]);
  }
  if (toExport.empty())
  {
    QMessageBox::information(this, "Export", "None of the selected files can be exported.");
    return;
  }

  const QString dir = QFileDialog::getExistingDirectory(
      this, "Select Export Folder");
  if (dir.isEmpty()) return;
  const auto dirPath = pathFromQString(dir);

  ap::OverwritePolicy policy = ap::OverwritePolicy::kOverwrite;
  const auto conflicts = ap::findExportConflicts(toExport, dirPath);
  if (!conflicts.empty())
  {
    constexpr size_t kShown = 8;
    QStringList shown;
    for (size_t i = 0; i < conflicts.size() && i < kShown; i++)
      shown << QString::fromStdString(conflicts[i]);
    if (conflicts.size() > kShown)
      shown << QString("... and %1 more").arg(conflicts.size() - kShown);

    QMessageBox box(QMessageBox::Question, "Files already exist",
                    "These files already exist in the export folder:\n\n" + shown.join("\n"),
                    QMessageBox::NoButton, this);
    auto* overwrite = box.addButton("Overwrite", QMessageBox::AcceptRole);
    auto* skip      = box.addButton("Skip existing", QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() == overwrite)  policy = ap::OverwritePolicy::kOverwrite;
    else if (box.clickedButton() == skip)  policy = ap::OverwritePolicy::kSkipExisting;
    else                                   return;
  }

  const auto result =
      ap::exportEmbeddedFiles(pathFromQString(_logPath), toExport, dirPath, policy);

  QString msg = QString("Exported %1 file(s) to:\n%2").arg(result.exported).arg(dir);
  if (!result.skipped.empty())
  {
    msg += "\n\nSkipped (already exist):";
    for (const auto& name : result.skipped) msg += "\n" + QString::fromStdString(name);
  }
  if (!result.failed.empty())
  {
    msg += "\n\nFailed:";
    for (const auto& f : result.failed)
      msg += "\n" + QString::fromStdString(f.name) + " - " + QString::fromStdString(f.reason);
  }
  if (!result.warnings.empty())
  {
    msg += "\n\nWarnings:";
    for (const auto& w : result.warnings) msg += "\n" + QString::fromStdString(w);
  }

  QMessageBox::information(this, "Export Complete", msg);
}

#ifdef ARDUPILOT_DEBUG_TAB
void ArdupilotInfoDialog::setupDebugTab(const ApLoadStats& stats)
{
  const double file_mb  = stats.file_size / (1024.0 * 1024.0);

  const double load_mb_s     = stats.load_ms > 0 ? file_mb / (stats.load_ms / 1000.0) : 0.0;
  const double samples_per_s = stats.load_ms > 0
      ? static_cast<double>(stats.total_samples) / (stats.load_ms / 1000.0) : 0.0;
  const double avg_per_series = stats.series_count > 0
      ? static_cast<double>(stats.total_samples) / stats.series_count : 0.0;

  const QVector<QPair<QString, QString>> rows = {
    { "File size",          QString::number(file_mb, 'f', 2) + " MB" },
    { "Load time",          QString::number(stats.load_ms) + " ms" },
    { "Total samples",      QString::number(static_cast<qulonglong>(stats.total_samples)) },
    { "Numeric series",     QString::number(static_cast<qulonglong>(stats.series_count)) },
    { "Avg samples/series", QString::number(avg_per_series, 'f', 0) },
    { "Throughput",         QString::number(load_mb_s, 'f', 1) + " MB/s" },
    { "Sample rate",        QString::number(samples_per_s / 1e6, 'f', 2) + " M samples/s" },
  };

  auto* tab    = new QWidget();
  auto* layout = new QVBoxLayout(tab);
  auto* table  = new QTableWidget(rows.size(), 2, tab);

  table->setHorizontalHeaderLabels({ "Metric", "Value" });
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->verticalHeader()->setVisible(false);
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

  for (int row = 0; row < rows.size(); row++)
  {
    table->setItem(row, 0, new QTableWidgetItem(rows[row].first));
    auto* valItem = new QTableWidgetItem(rows[row].second);
    valItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->setItem(row, 1, valItem);
  }

  layout->addWidget(table);
  ui->tabWidget->addTab(tab, "Debug");
}
#endif

void ArdupilotInfoDialog::saveSettings()
{
  QSettings settings;
  settings.setValue("ArdupilotParamsDialog/geometry", saveGeometry());
  settings.setValue("ArdupilotParamsDialog/tableState",
                    ui->tableParams->horizontalHeader()->saveState());
  settings.setValue("ArdupilotParamsDialog/filesTableState",
                    ui->tableFiles->horizontalHeader()->saveState());
}

void ArdupilotInfoDialog::restoreSettings()
{
  QSettings settings;
  restoreGeometry(settings.value("ArdupilotParamsDialog/geometry").toByteArray());
  ui->tableParams->horizontalHeader()->restoreState(
      settings.value("ArdupilotParamsDialog/tableState").toByteArray());
  // Re-apply resize modes — restoreState() serializes them and would overwrite
  // the deferred-resize setup in the constructor with any previously saved mode.
  ui->tableParams->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  ui->tableParams->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  ui->tableFiles->horizontalHeader()->restoreState(
      settings.value("ArdupilotParamsDialog/filesTableState").toByteArray());
  ui->tableFiles->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  for (int col = 1; col <= 3; col++)
    ui->tableFiles->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
}
