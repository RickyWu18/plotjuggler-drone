/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <QDialog>
#include <QHash>
#include <QPair>
#include <QShowEvent>
#include <QStringList>
#include <QVector>
#include <cstdint>

namespace Ui
{
class IntervalSettingsDialog;
}

class IntervalSettingsDialog : public QDialog
{
  Q_OBJECT

public:
  explicit IntervalSettingsDialog(QWidget* parent = nullptr);
  ~IntervalSettingsDialog() override;

  void setConnected(bool connected);

public slots:
  void onMessageIntervalReceived(uint16_t msg_id, int32_t interval_us);
  void onHistoryList(QVector<uint16_t> ids, QStringList names, QVector<double> rates);
  void onEndpointDiscovered(uint8_t sysid, uint8_t compid);

signals:
  void requestHistory();
  void requestGet(uint8_t target_sysid, uint8_t target_compid, QVector<uint16_t> msg_ids);
  void requestSet(uint8_t target_sysid, uint8_t target_compid,
                  QVector<QPair<uint16_t, int32_t>> intervals);

protected:
  void showEvent(QShowEvent* event) override;

private slots:
  void onAddClicked();
  void onRefreshClicked();
  void onCellDoubleClicked(int row, int col);

private:
  void showRateEditDialog(int row);
  void addEntry(uint16_t msg_id, const QString& name, double hz);
  uint16_t rowMsgId(int row) const;
  int findRow(uint16_t msg_id) const;
  void setRowRate(int row, double hz);
  double rowRate(int row) const;
  QPair<uint8_t, uint8_t> selectedEndpoint() const;

  static double usToHz(int32_t us);
  static int32_t hzToUs(double hz);
  static QString formatHz(double hz);

  Ui::IntervalSettingsDialog* ui_;
  bool _connected = false;
  // cache of all received messages (updated via onHistoryList)
  QHash<uint16_t, QPair<QString, double>> _available_msgs;
};
