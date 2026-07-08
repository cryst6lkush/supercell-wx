#include <scwx/qt/ui/storm_development_heat_map_widget.hpp>

#include <algorithm>
#include <array>
#include <cmath>

#include <QImage>
#include <QPainter>

namespace scwx::qt::ui
{

namespace
{

struct ColorStop
{
   double score;
   int    r;
   int    g;
   int    b;
};

// Deep blue -> green -> yellow -> orange -> red, low to high likelihood.
constexpr std::array<ColorStop, 5> kColorStops {
   {{0.0, 20, 20, 120},
    {25.0, 40, 140, 220},
    {50.0, 240, 220, 60},
    {75.0, 240, 140, 30},
    {100.0, 200, 20, 20}}};

QColor LikelihoodColor(double score)
{
   const double clamped = std::clamp(score, 0.0, 100.0);

   for (std::size_t i = 0; i + 1 < kColorStops.size(); ++i)
   {
      const ColorStop& a = kColorStops[i];
      const ColorStop& b = kColorStops[i + 1];

      if (clamped <= b.score)
      {
         const double t = (clamped - a.score) / (b.score - a.score);
         return QColor {static_cast<int>(std::lround(a.r + t * (b.r - a.r))),
                        static_cast<int>(std::lround(a.g + t * (b.g - a.g))),
                        static_cast<int>(std::lround(a.b + t * (b.b - a.b)))};
      }
   }

   const ColorStop& last = kColorStops.back();
   return QColor {last.r, last.g, last.b};
}

} // namespace

StormDevelopmentHeatMapWidget::StormDevelopmentHeatMapWidget(QWidget* parent) :
    QWidget(parent)
{
   setMinimumSize(320, 200);
}

StormDevelopmentHeatMapWidget::~StormDevelopmentHeatMapWidget() = default;

void StormDevelopmentHeatMapWidget::SetGrid(
   std::shared_ptr<const common::LatLonGrid> grid)
{
   grid_ = std::move(grid);
   update();
}

void StormDevelopmentHeatMapWidget::paintEvent(QPaintEvent* /* event */)
{
   QPainter painter(this);
   painter.fillRect(rect(), palette().color(QPalette::Base));

   if (grid_ == nullptr || grid_->RowCount() == 0 || grid_->ColumnCount() == 0)
   {
      painter.setPen(palette().color(QPalette::WindowText));
      painter.drawText(rect(), Qt::AlignCenter, tr("Waiting for data..."));
      return;
   }

   // Build a 1-pixel-per-cell image (row 0 = north, col 0 = west, matching the
   // grid's storage order and the widget's top-left origin), then let Qt
   // bilinearly upscale it to the widget. Cheaper and smoother than drawing one
   // rectangle per cell, and independent of the data resolution.
   const int cols = static_cast<int>(grid_->ColumnCount());
   const int rows = static_cast<int>(grid_->RowCount());

   QImage image {cols, rows, QImage::Format_ARGB32};

   for (int row = 0; row < rows; ++row)
   {
      for (int column = 0; column < cols; ++column)
      {
         const double value = grid_->ValueAt(row, column);
         image.setPixelColor(
            column,
            row,
            std::isnan(value) ? QColor {Qt::transparent} : LikelihoodColor(value));
      }
   }

   painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
   painter.drawImage(rect(), image);
}

} // namespace scwx::qt::ui
