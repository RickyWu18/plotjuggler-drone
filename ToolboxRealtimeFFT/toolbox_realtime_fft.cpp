/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "toolbox_realtime_fft.h"
#include "ui_toolbox_realtime_fft.h"

#include <QDialogButtonBox>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QHBoxLayout>
#include <algorithm>
#include <cmath>

#include "KissFFT/kiss_fftr.h"

static constexpr double PI = 3.14159265358979323846;

ToolboxRealtimeFFT::ToolboxRealtimeFFT()
{
  _widget = new QWidget(nullptr);
  ui = new Ui::toolbox_realtime_fft;
  ui->setupUi(_widget);

  _timer = new QTimer(_widget);
  _timer->setInterval(100);

  connect(_timer, &QTimer::timeout, this, &ToolboxRealtimeFFT::onTimerTick);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ToolboxPlugin::closed);
  connect(ui->pushButtonClear, &QPushButton::clicked, this, &ToolboxRealtimeFFT::onClearCurves);
  connect(ui->spinBoxUpdateRate, QOverload<int>::of(&QSpinBox::valueChanged),
          [this](int ms) { _timer->setInterval(ms); });
}

ToolboxRealtimeFFT::~ToolboxRealtimeFFT()
{
  _timer->stop();
  delete ui;
}

void ToolboxRealtimeFFT::init(PJ::PlotDataMapRef& src_data, PJ::TransformsMap& transform_map)
{
  _plot_data = &src_data;
  _transforms = &transform_map;

  _plot_time = new PJ::PlotWidgetBase(ui->frameDropZone);
  _plot_fft = new PJ::PlotWidgetBase(ui->framePlotFFT);

  auto layout_time = new QHBoxLayout(ui->frameDropZone);
  layout_time->setMargin(6);
  layout_time->addWidget(_plot_time);

  auto layout_fft = new QHBoxLayout(ui->framePlotFFT);
  layout_fft->setMargin(6);
  layout_fft->addWidget(_plot_fft);

  _plot_time->setAcceptDrops(true);
  connect(_plot_time, &PJ::PlotWidgetBase::dragEnterSignal, this,
          &ToolboxRealtimeFFT::onDragEnterEvent);
  connect(_plot_time, &PJ::PlotWidgetBase::dropSignal, this, &ToolboxRealtimeFFT::onDropEvent);
}

std::pair<QWidget*, PJ::ToolboxPlugin::WidgetType> ToolboxRealtimeFFT::providedWidget() const
{
  return { _widget, PJ::ToolboxPlugin::FIXED };
}

bool ToolboxRealtimeFFT::onShowWidget()
{
  return true;
}

void ToolboxRealtimeFFT::onDragEnterEvent(QDragEnterEvent* event)
{
  const QMimeData* mimeData = event->mimeData();
  for (const QString& format : mimeData->formats())
  {
    if (format != "curveslist/add_curve")
      continue;

    QByteArray encoded = mimeData->data(format);
    QDataStream stream(&encoded, QIODevice::ReadOnly);

    QStringList curves;
    while (!stream.atEnd())
    {
      QString name;
      stream >> name;
      if (!name.isEmpty())
        curves.push_back(name);
    }
    _dragging_curves = curves;
    event->accept();
    return;
  }
}

void ToolboxRealtimeFFT::onDropEvent(QDropEvent*)
{
  for (const QString& curve_q : _dragging_curves)
  {
    std::string curve_id = curve_q.toStdString();
    if (std::find(_curve_names.begin(), _curve_names.end(), curve_id) != _curve_names.end())
      continue;

    // Insert into std::map before calling addCurve; std::map never invalidates
    // existing iterators/references when new elements are inserted.
    _time_scatter.emplace(std::piecewise_construct, std::forward_as_tuple(curve_id),
                          std::forward_as_tuple(curve_id, PJ::PlotGroup::Ptr{}));
    _fft_scatter.emplace(std::piecewise_construct, std::forward_as_tuple(curve_id),
                         std::forward_as_tuple(curve_id, PJ::PlotGroup::Ptr{}));

    // Let PlotWidgetBase auto-assign a color and store it in the scatter's COLOR_HINT.
    _plot_time->addCurve(curve_id, _time_scatter.at(curve_id));

    // Read back the assigned color so both plots use the same one for this curve.
    QColor color = Qt::transparent;
    {
      QVariant hint = _time_scatter.at(curve_id).attribute(PJ::COLOR_HINT);
      if (hint.isValid())
        color = hint.value<QColor>();
    }
    _fft_scatter.at(curve_id).setAttribute(PJ::COLOR_HINT, color);
    _plot_fft->addCurve(curve_id + "_RTFFT", _fft_scatter.at(curve_id));

    _curve_names.push_back(curve_id);
  }

  if (!_curve_names.empty())
  {
    _fft_zoom_init = false;
    _timer->start();
  }

  _dragging_curves.clear();
}

void ToolboxRealtimeFFT::onClearCurves()
{
  _timer->stop();
  _curve_names.clear();

  _plot_time->removeAllCurves();
  _plot_time->resetZoom();

  _plot_fft->removeAllCurves();
  _plot_fft->resetZoom();

  // Safe to clear after removeAllCurves has deleted all QwtSeriesWrapper objects.
  _time_scatter.clear();
  _fft_scatter.clear();
  _fft_zoom_init = false;
}

void ToolboxRealtimeFFT::onTimerTick()
{
  computeAndDisplayFFT();
}

void ToolboxRealtimeFFT::computeAndDisplayFFT()
{
  const int window_size = ui->comboBoxWindowSize->currentText().toInt();
  const int window_fn = ui->comboBoxWindowFn->currentIndex();  // 0=Rect, 1=Hann, 2=Hamming
  const bool remove_dc = ui->checkBoxRemoveDC->isChecked();

  bool any_data = false;
  double y_peak = 0.0;

  for (const auto& curve_id : _curve_names)
  {
    auto src_it = _plot_data->numeric.find(curve_id);
    if (src_it == _plot_data->numeric.end())
      continue;
    PlotData& curve_data = src_it->second;

    auto time_it = _time_scatter.find(curve_id);
    auto fft_it = _fft_scatter.find(curve_id);
    if (time_it == _time_scatter.end() || fft_it == _fft_scatter.end())
      continue;

    PJ::PlotDataXY& time_data = time_it->second;
    PJ::PlotDataXY& fft_data = fft_it->second;

    // Determine how many samples fit
    size_t N = static_cast<size_t>(window_size);
    if (N & 1)
      N--;  // KissFFT real FFT requires even N
    if (N < 8 || curve_data.size() < N)
    {
      time_data.clear();
      fft_data.clear();
      continue;
    }

    size_t start = curve_data.size() - N;
    double t0 = curve_data.at(start).x;
    double t1 = curve_data.at(start + N - 1).x;
    double dT = (t1 - t0) / static_cast<double>(N - 1);
    if (dT <= 0)
      continue;

    // --- Update time-domain sliding window ---
    time_data.clear();
    for (size_t i = 0; i < N; i++)
    {
      const auto& p = curve_data[start + i];
      time_data.pushBack({ p.x, p.y });
    }

    // --- Build FFT input ---
    double mean = 0;
    if (remove_dc)
    {
      for (size_t i = 0; i < N; i++)
        mean += curve_data[start + i].y;
      mean /= static_cast<double>(N);
    }

    std::vector<kiss_fft_scalar> input(N);
    for (size_t i = 0; i < N; i++)
    {
      double val = curve_data[start + i].y - mean;
      if (window_fn == 1)  // Hann
        val *= 0.5 * (1.0 - std::cos(2.0 * PI * i / static_cast<double>(N - 1)));
      else if (window_fn == 2)  // Hamming
        val *= 0.54 - 0.46 * std::cos(2.0 * PI * i / static_cast<double>(N - 1));
      input[i] = static_cast<kiss_fft_scalar>(val);
    }

    std::vector<kiss_fft_cpx> out(N / 2 + 1);
    kiss_fftr_cfg cfg = kiss_fftr_alloc(static_cast<int>(N), false, nullptr, nullptr);
    kiss_fftr(cfg, input.data(), out.data());
    free(cfg);

    // --- Update FFT scatter in-place ---
    fft_data.clear();
    const double scale = 1.0 / static_cast<double>(N);
    for (size_t i = 1; i < N / 2; i++)
    {
      double hz = static_cast<double>(i) * (1.0 / dT) / static_cast<double>(N);
      double amp = std::hypot(out[i].r, out[i].i) * scale;
      y_peak = std::max(y_peak, amp);
      fft_data.pushBack({ hz, amp });
    }

    any_data = true;
  }

  // Time domain always auto-fits: the window is fixed-size so this isn't disorienting.
  _plot_time->resetZoom();

  if (any_data)
  {
    if (!_fft_zoom_init)
    {
      // First data: establish initial axes and zoom limits.
      _plot_fft->resetZoom();
      _fft_zoom_init = true;
    }
    else
    {
      // Check whether the current peak exceeds the visible Y range.
      // currentBoundingRect() reads from the live axis scale, before any repaint.
      const QRectF view = _plot_fft->currentBoundingRect();
      const double view_y_max = std::max(view.top(), view.bottom());

      if (y_peak > view_y_max)
      {
        // New peak is outside the visible area: reset both axes so the data fits
        // and the zoom boundary (maxZoomRect) is updated to reflect the new range.
        _plot_fft->resetZoom();
      }
      else
      {
        // Data fits in the current view — just repaint without touching axes,
        // preserving whatever zoom/pan the user has applied.
        _plot_fft->replot();
      }
    }
  }
}
