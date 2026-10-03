/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "interval_settings_dialog.h"
#include "ui_interval_settings_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QSet>
#include <QSpinBox>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <cmath>

namespace
{
class NumericTableItem : public QTableWidgetItem
{
public:
  using QTableWidgetItem::QTableWidgetItem;
  bool operator<(const QTableWidgetItem& other) const override
  {
    return data(Qt::UserRole).toDouble() < other.data(Qt::UserRole).toDouble();
  }
};
}  // namespace

// ----- unit helpers -------------------------------------------------------

double IntervalSettingsDialog::usToHz(int32_t us)
{
  if (us < 0)
    return -1.0;
  if (us == 0)
    return 0.0;
  return 1000000.0 / static_cast<double>(us);
}

int32_t IntervalSettingsDialog::hzToUs(double hz)
{
  if (hz < 0.0)
    return -1;
  if (hz == 0.0)
    return 0;
  return static_cast<int32_t>(std::round(1000000.0 / hz));
}

QString IntervalSettingsDialog::formatHz(double hz)
{
  if (hz < 0.0)
    return QStringLiteral("Disabled");
  return QString::number(static_cast<int>(std::round(hz)));
}

// ----- construction -------------------------------------------------------

IntervalSettingsDialog::IntervalSettingsDialog(QWidget* parent)
  : QDialog(parent), ui_(new Ui::IntervalSettingsDialog)
{
  ui_->setupUi(this);

  ui_->intervalsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  ui_->intervalsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
  ui_->intervalsTable->setColumnWidth(1, 110);
  ui_->intervalsTable->verticalHeader()->setVisible(false);

  connect(ui_->addBtn, &QPushButton::clicked, this, &IntervalSettingsDialog::onAddClicked);
  connect(ui_->refreshBtn, &QPushButton::clicked, this, &IntervalSettingsDialog::onRefreshClicked);
  connect(ui_->intervalsTable, &QTableWidget::cellDoubleClicked,
          this, &IntervalSettingsDialog::onCellDoubleClicked);
  connect(ui_->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

IntervalSettingsDialog::~IntervalSettingsDialog()
{
  delete ui_;
}

// ----- public API ---------------------------------------------------------

void IntervalSettingsDialog::setConnected(bool connected)
{
  _connected = connected;
  ui_->refreshBtn->setEnabled(connected);
}

void IntervalSettingsDialog::showEvent(QShowEvent* event)
{
  QDialog::showEvent(event);
  emit requestHistory();
}

// ----- row helpers --------------------------------------------------------

QPair<uint8_t, uint8_t> IntervalSettingsDialog::selectedEndpoint() const
{
  uint32_t v = ui_->targetCombo->currentData().toUInt();
  return { static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v & 0xFF) };
}

uint16_t IntervalSettingsDialog::rowMsgId(int row) const
{
  auto* item = ui_->intervalsTable->item(row, 0);
  return item ? static_cast<uint16_t>(item->data(Qt::UserRole).toUInt()) : 0;
}

void IntervalSettingsDialog::setRowRate(int row, double hz)
{
  auto* item = dynamic_cast<NumericTableItem*>(ui_->intervalsTable->item(row, 1));
  if (!item)
  {
    item = new NumericTableItem();
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    ui_->intervalsTable->setItem(row, 1, item);
  }
  item->setText(formatHz(hz));
  item->setData(Qt::UserRole, hz);
}

double IntervalSettingsDialog::rowRate(int row) const
{
  auto* item = ui_->intervalsTable->item(row, 1);
  return item ? item->data(Qt::UserRole).toDouble() : 0.0;
}

int IntervalSettingsDialog::findRow(uint16_t msg_id) const
{
  for (int row = 0; row < ui_->intervalsTable->rowCount(); ++row)
    if (rowMsgId(row) == msg_id)
      return row;
  return -1;
}

// ----- slots --------------------------------------------------------------

void IntervalSettingsDialog::onHistoryList(QVector<uint16_t> ids, QStringList names,
                                           QVector<double> rates)
{
  for (int i = 0; i < ids.size(); ++i)
  {
    const uint16_t id = ids[i];
    const QString name = i < names.size() ? names[i] : QString();
    const double hz = i < rates.size() ? rates[i] : 0.0;

    _available_msgs[id] = { name, hz };

    int row = findRow(id);
    if (row < 0)
    {
      addEntry(id, name, hz);
      continue;
    }

    auto* nameItem = ui_->intervalsTable->item(row, 0);
    if (nameItem && !name.isEmpty() &&
        (nameItem->text().isEmpty() || nameItem->text() == QString("ID_%1").arg(id)))
      nameItem->setText(name);

    setRowRate(row, hz);
  }
}

void IntervalSettingsDialog::onMessageIntervalReceived(uint16_t msg_id, int32_t interval_us)
{
  for (int row = 0; row < ui_->intervalsTable->rowCount(); ++row)
  {
    if (rowMsgId(row) == msg_id)
    {
      setRowRate(row, usToHz(interval_us));
      return;
    }
  }
}

void IntervalSettingsDialog::onEndpointDiscovered(uint8_t sysid, uint8_t compid)
{
  uint32_t packed = (uint32_t(sysid) << 8) | compid;
  for (int i = 0; i < ui_->targetCombo->count(); ++i)
    if (ui_->targetCombo->itemData(i).toUInt() == packed)
      return;
  ui_->targetCombo->addItem(QString("SYS %1 / COMP %2").arg(sysid).arg(compid), packed);
}

void IntervalSettingsDialog::onAddClicked()
{
  // Collect IDs already tracked
  QSet<uint16_t> existing;
  for (int row = 0; row < ui_->intervalsTable->rowCount(); ++row)
    existing.insert(rowMsgId(row));

  // Build menu from received messages not yet tracked
  QMenu menu(this);
  for (auto it = _available_msgs.constBegin(); it != _available_msgs.constEnd(); ++it)
  {
    if (existing.contains(it.key()))
      continue;
    const QString label =
        QString("%1 (ID %2)  –  %3 Hz")
            .arg(it.value().first.isEmpty() ? QString("ID_%1").arg(it.key()) : it.value().first)
            .arg(it.key())
            .arg(static_cast<int>(std::round(it.value().second)));
    auto* action = menu.addAction(label);
    const uint16_t id = it.key();
    connect(action, &QAction::triggered, this, [this, id]() {
      const auto& entry = _available_msgs[id];
      addEntry(id, entry.first, entry.second);
      int row = findRow(id);
      if (row >= 0)
        showRateEditDialog(row);
    });
  }

  if (menu.isEmpty())
  {
    // Fallback: manual ID entry
    bool ok = false;
    int id = QInputDialog::getInt(this, tr("Add Message"), tr("Message ID:"), 0, 0, 65535, 1, &ok);
    if (ok)
    {
      addEntry(static_cast<uint16_t>(id), QString(), 0.0);
      int row = findRow(static_cast<uint16_t>(id));
      if (row >= 0)
        showRateEditDialog(row);
    }
    return;
  }

  menu.exec(ui_->addBtn->mapToGlobal(ui_->addBtn->rect().bottomLeft()));
}

void IntervalSettingsDialog::onRefreshClicked()
{
  emit requestHistory();

  QVector<uint16_t> ids;
  ids.reserve(ui_->intervalsTable->rowCount());
  for (int row = 0; row < ui_->intervalsTable->rowCount(); ++row)
    ids.append(rowMsgId(row));

  if (!ids.isEmpty())
  {
    auto ep = selectedEndpoint();
    emit requestGet(ep.first, ep.second, ids);
  }
}

void IntervalSettingsDialog::onCellDoubleClicked(int row, int /*col*/)
{
  showRateEditDialog(row);
}

// ----- private helpers ----------------------------------------------------

void IntervalSettingsDialog::showRateEditDialog(int row)
{
  auto* nameItem = ui_->intervalsTable->item(row, 0);
  const QString msgName = nameItem ? nameItem->text() : QString("Message");
  const double currentHz = rowRate(row);

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Set Rate - %1").arg(msgName));

  auto* layout = new QVBoxLayout(&dlg);
  auto* form = new QFormLayout();

  auto* spinHz = new QSpinBox(&dlg);
  spinHz->setRange(-1, 2000);
  spinHz->setSingleStep(1);
  spinHz->setSpecialValueText(tr("Disabled"));
  spinHz->setValue(static_cast<int>(std::round(currentHz)));

  form->addRow(tr("Rate (Hz):"), spinHz);
  layout->addLayout(form);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted)
    return;

  const double newHz = static_cast<double>(spinHz->value());
  setRowRate(row, newHz);

  if (_connected)
  {
    auto ep = selectedEndpoint();
    emit requestSet(ep.first, ep.second, { { rowMsgId(row), hzToUs(newHz) } });
  }
}

void IntervalSettingsDialog::addEntry(uint16_t msg_id, const QString& name, double hz)
{
  // Prevent duplicates
  for (int row = 0; row < ui_->intervalsTable->rowCount(); ++row)
    if (rowMsgId(row) == msg_id)
      return;

  bool wasSorted = ui_->intervalsTable->isSortingEnabled();
  ui_->intervalsTable->setSortingEnabled(false);

  int row = ui_->intervalsTable->rowCount();
  ui_->intervalsTable->insertRow(row);

  auto* nameItem = new QTableWidgetItem(name.isEmpty() ? QString("ID_%1").arg(msg_id) : name);
  nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
  nameItem->setData(Qt::UserRole, static_cast<uint>(msg_id));
  ui_->intervalsTable->setItem(row, 0, nameItem);

  auto* rateItem = new NumericTableItem(formatHz(hz));
  rateItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
  rateItem->setData(Qt::UserRole, hz);
  ui_->intervalsTable->setItem(row, 1, rateItem);

  ui_->intervalsTable->setSortingEnabled(wasSorted);
}
