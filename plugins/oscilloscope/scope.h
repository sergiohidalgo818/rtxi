/*
         The Real-Time eXperiment Interface (RTXI)
         Copyright (C) 2011 Georgia Institute of Technology, University of Utah,
   Weill Cornell Medical College

         This program is free software: you can redistribute it and/or modify
         it under the terms of the GNU General Public License as published by
         the Free Software Foundation, either version 3 of the License, or
         (at your option) any later version.

         This program is distributed in the hope that it will be useful,
         but WITHOUT ANY WARRANTY; without even the implied warranty of
         MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
         GNU General Public License for more details.

         You should have received a copy of the GNU General Public License
         along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

/*
         This class instantiates a single scope instance.
         This includes the painting director, the canvass,
         and the functions necessary for modifying those
         scopes. Multiple scope objects can be instantiated,
         and each instance will have it's own set of settings
         that can be edited with the Panel class.
         */

#ifndef SCOPE_H
#define SCOPE_H

#include <QtWidgets>
#include <vector>

#include <qwt.h>
#include <qwt_curve_fitter.h>
#include <qwt_interval.h>
#include <qwt_painter.h>
#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_directpainter.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_legenditem.h>
#include <qwt_plot_marker.h>
#include <qwt_scale_engine.h>
#include <qwt_system_clock.h>

#include "io.hpp"

namespace Oscilloscope {

// values meant to be used with qt timer for redrawing the screen
// values are in milliseconds 
namespace FrameRates {
constexpr size_t HZ60 = 17;
constexpr size_t HZ120 = 8;
constexpr size_t HZ240 = 4;
}; // namespace FrameRates 

typedef struct sample {
  double value;
  int64_t time;
} sample;

typedef struct scope_channel
{
  QString label;
  double scale=1;
  double offset=0;
  std::vector<double> xbuffer;
  std::vector<double> ybuffer;
  size_t data_indx=0;
  QwtPlotCurve* curve = nullptr;
  IO::Block* block = nullptr;
  size_t port = 0;
  IO::flags_t direction;
  IO::channel_t info {};
}scope_channel;

constexpr std::array<QColor, 7> penColors = 
  {
    QColor(255, 0, 16, 255),
    QColor(255, 164, 5, 255),
    QColor(43, 206, 72, 255),
    QColor(0, 117, 220, 255),
    QColor(178, 102, 255, 255),
    QColor(0, 153, 143, 255),
    QColor(83, 81, 84, 255)
  };

class LegendItem : public QwtPlotLegendItem
{
public:
  LegendItem()
  {
    setRenderHint(QwtPlotItem::RenderAntialiased);
    QColor color(Qt::black);
    setTextPen(color);
  }
};  // LegendItem

class Canvas : public QwtPlotCanvas
{
public:
  Canvas(QwtPlot* plot = nullptr) : QwtPlotCanvas(plot)
  {
    setPaintAttribute(QwtPlotCanvas::BackingStore, false);
    if (QwtPainter::isX11GraphicsSystem()) {
      if (testPaintAttribute(QwtPlotCanvas::BackingStore)) {
        setAttribute(Qt::WA_PaintOnScreen, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
      }
    }
    setupPalette();
  }

private:
  void setupPalette()
  {
    QPalette pal = palette();
    QLinearGradient gradient;
    gradient.setCoordinateMode(QGradient::StretchToDeviceMode);
    gradient.setColorAt(1.0, QColor(Qt::white));
    pal.setBrush(QPalette::Window, QBrush(gradient));
    pal.setColor(QPalette::WindowText, Qt::green);
    setPalette(pal);
  }
};  // Canvas


class Scope : public QwtPlot
{
public:
  explicit Scope(QWidget* = nullptr);
  ~Scope();

  bool paused() const;
  void insertChannel(const scope_channel& channel);
  void removeChannel(IO::Block* block, size_t port, IO::flags_t type);
  size_t getChannelCount() const;
  //scope_channel getChannel(IO::Block* block, size_t port);

  void clearData();
  void setData(IO::Block* block, size_t port, std::vector<sample> data);
  size_t getDataSize() const;
  void setDataSize(size_t);

  //Trigger::trig_t getTriggerDirection();
  //double getTriggerThreshold();

  double getDivT() const;
  void setDivT(double);

  void setPeriod(double);
  size_t getDivX() const;
  size_t getDivY() const;

  size_t getRefresh() const;
  void setRefresh(size_t);


  void setChannelScale(IO::Block* block, size_t port, IO::flags_t direction, double scale);
  double getChannelScale(IO::Block* block, size_t port, IO::flags_t direction);
  void setChannelOffset(IO::Block* block, size_t port, IO::flags_t direction, double offset);
  double getChannelOffset(IO::Block* block, size_t port, IO::flags_t direction);
  void setChannelPen(IO::Block* block, size_t port, IO::flags_t direction, const QPen& pen);
  QPen* getChannelPen(IO::Block* block, size_t port, IO::flags_t direction);
  void setChannelLabel(IO::Block* block, size_t port, IO::flags_t direction, const QString& label);
  //Trigger::Info capture_trigger;

  int64_t getWindowTimewidth();
protected:
  void resizeEvent(QResizeEvent* event);

private slots:
  void timeoutEvent();

private:
  void drawCurves();
  double window_timewidth;

  bool isPaused = false;
  int divX=10;
  int divY=10;
  size_t refresh=Oscilloscope::FrameRates::HZ60;
  double hScl=1.0;  // horizontal scale for time (ms)
  bool triggering=false;

  // Scope primary paint element
  QwtPlotDirectPainter* d_directPainter=nullptr;

  // Scope painter elements
  QwtPlotGrid* grid;
  QwtPlotMarker* origin;

  // Scaling engine
  QwtScaleMap* scaleMapY;
  QwtScaleMap* scaleMapX;

  // Legend
  LegendItem* legendItem;

  QTimer* timer;
  QString dtLabel;
  std::list<scope_channel> channels;
};  // Scope

}; // namespace Oscilloscope

#endif  // SCOPE_H
