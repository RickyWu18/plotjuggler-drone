/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <map>
#include <QTimer>
#include <QtPlugin>
#include "PlotJuggler/toolbox_base.h"
#include "PlotJuggler/plotwidget_base.h"

namespace Ui
{
class toolbox_realtime_fft;
}

class ToolboxRealtimeFFT : public PJ::ToolboxPlugin
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.Toolbox")
  Q_INTERFACES(PJ::ToolboxPlugin)

public:
  ToolboxRealtimeFFT();
  ~ToolboxRealtimeFFT() override;

  const char* name() const override
  {
    return "Real-Time FFT";
  }

  void init(PJ::PlotDataMapRef& src_data, PJ::TransformsMap& transform_map) override;

  std::pair<QWidget*, WidgetType> providedWidget() const override;

public slots:
  bool onShowWidget() override;

private slots:
  void onTimerTick();
  void onDragEnterEvent(QDragEnterEvent* event);
  void onDropEvent(QDropEvent* event);
  void onClearCurves();

private:
  void computeAndDisplayFFT();

  QWidget* _widget;
  Ui::toolbox_realtime_fft* ui;

  PJ::PlotWidgetBase* _plot_time = nullptr;
  PJ::PlotWidgetBase* _plot_fft = nullptr;

  PJ::PlotDataMapRef* _plot_data = nullptr;
  PJ::TransformsMap* _transforms = nullptr;

  // std::map provides stable references even after further insertions.
  // QwtSeriesWrapper holds a raw pointer into these objects.
  std::map<std::string, PJ::PlotDataXY> _time_scatter;
  std::map<std::string, PJ::PlotDataXY> _fft_scatter;

  QTimer* _timer;
  QStringList _dragging_curves;
  std::vector<std::string> _curve_names;
  bool _fft_zoom_init = false;
};
