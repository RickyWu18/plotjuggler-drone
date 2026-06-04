/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ardupilot_info_dialog.h"
#include "ui_ardupilot_info_dialog.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QHeaderView>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSettings>
#include <QTableWidgetItem>
#include <QTextStream>

ArdupilotInfoDialog::ArdupilotInfoDialog(const std::vector<ApParameter>& params,
                                         const std::vector<ApEmbeddedFile>& files,
                                         const std::vector<ApLogMessage>& msgs,
                                         QWidget* parent)
  : QDialog(parent), ui(new Ui::ArdupilotInfoDialog), _embeddedFiles(files)
{
  ui->setupUi(this);

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
    table->setItem(row, 0,
        new QTableWidgetItem(QString::fromStdString(params[row].name)));
    table->setItem(row, 1,
        new QTableWidgetItem(QString::number(params[row].value, 'g', 8)));
  }
  table->sortItems(0);
  // Set ResizeToContents after all items are inserted — avoids O(n²) rescans
  table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
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
            QString::number(static_cast<qulonglong>(files[row].data.size())) + " bytes"));
  }
  ftable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  ftable->setUpdatesEnabled(true);

  // "Export All" is always available when there are files
  ui->btnExportAll->setEnabled(!files.empty());

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
    auto* tsItem = new QTableWidgetItem(QString::number(msgs[row].timestamp, 'f', 6));
    tsItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mtable->setItem(row, 0, tsItem);
    mtable->setItem(row, 1,
        new QTableWidgetItem(QString::fromStdString(msgs[row].message)));
  }
  mtable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  mtable->setUpdatesEnabled(true);
}

ArdupilotInfoDialog::~ArdupilotInfoDialog()
{
  saveSettings();
  delete ui;
}

void ArdupilotInfoDialog::onSearchChanged(const QString& text)
{
  auto* table = ui->tableParams;
  QRegularExpression re(text, QRegularExpression::CaseInsensitiveOption);
  const bool valid = re.isValid();
  table->setUpdatesEnabled(false);
  for (int row = 0; row < table->rowCount(); row++)
  {
    auto* item = table->item(row, 0);
    const bool match = !item ? false
                     : text.isEmpty() ? true
                     : valid && re.match(item->text()).hasMatch();
    table->setRowHidden(row, !match);
  }
  table->setUpdatesEnabled(true);
}

void ArdupilotInfoDialog::onExport()
{
  const QString path = QFileDialog::getSaveFileName(
      this, "Export Parameters", QString(), "Parameter files (*.param)");
  if (path.isEmpty()) return;

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    QMessageBox::warning(this, "Export Failed",
                         "Could not open file for writing:\n" + path);
    return;
  }

  QTextStream out(&file);
  auto* table = ui->tableParams;
  for (int row = 0; row < table->rowCount(); row++)
  {
    if (table->isRowHidden(row)) continue;
    const QString name  = table->item(row, 0)->text();
    const QString value = table->item(row, 1)->text();
    out << name << "," << value << "\n";
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
  const QString dir = QFileDialog::getExistingDirectory(
      this, "Select Export Folder");
  if (dir.isEmpty()) return;

  int exported = 0;
  QStringList failed;

  for (const auto& idx : rows)
  {
    auto* nameItem = ui->tableFiles->item(idx.row(), 0);
    if (!nameItem) continue;

    const int ef_idx = nameItem->data(Qt::UserRole).toInt();
    if (ef_idx < 0 || ef_idx >= static_cast<int>(_embeddedFiles.size())) continue;

    const auto& ef = _embeddedFiles[static_cast<size_t>(ef_idx)];

    // Preserve the full path structure (e.g. @SYS/crash_dump.bin → <dir>/@SYS/crash_dump.bin)
    const QString relPath = QString::fromStdString(ef.name);
    const QString path = dir + "/" + relPath;
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile out(path);
    if (!out.open(QIODevice::WriteOnly))
    {
      failed << relPath;
      continue;
    }
    out.write(reinterpret_cast<const char*>(ef.data.data()),
              static_cast<qint64>(ef.data.size()));
    exported++;
  }

  QString msg = QString("Exported %1 file(s) to:\n%2").arg(exported).arg(dir);
  if (!failed.isEmpty())
    msg += "\n\nFailed to write:\n" + failed.join("\n");

  QMessageBox::information(this, "Export Complete", msg);
}

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
  ui->tableFiles->horizontalHeader()->restoreState(
      settings.value("ArdupilotParamsDialog/filesTableState").toByteArray());
}
