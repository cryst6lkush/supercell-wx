#include <scwx/qt/ui/storm_development_heat_map_widget.hpp>

#include <algorithm>
#include <array>
#include <cmath>

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

   if (grid_ == nullptr)
   {
      painter.setPen(palette().color(QPalette::WindowText));
      painter.drawText(rect(), Qt::AlignCenter, tr("Waiting for data..."));
      return;
   }

   const double w             = width();
   const double h             = height();
   const double latMin        = grid_->LatMin();
   const double latMax        = grid_->LatMax();
   const double lonMin        = grid_->LonMin();
   const double lonMax        = grid_->LonMax();
   const double spacingDeg    = grid_->SpacingDegrees();

   for (std::size_t row = 0; row < grid_->RowCount(); ++row)
   {
      for (std::size_t column = 0; column < grid_->ColumnCount(); ++column)
      {
         const double value = grid_->ValueAt(row, column);
         if (std::isnan(value))
         {
            continue;
         }

         const common::Coordinate coordinate =
            grid_->CoordinateAt(row, column);

         const double x0 =
            (coordinate.longitude_ - lonMin) / (lonMax - lonMin) * w;
         const double x1 = (coordinate.longitude_ + spacingDeg - lonMin) /
                           (lonMax - lonMin) * w;
         const double y0 =
            (latMax - coordinate.latitude_) / (latMax - latMin) * h;
         const double y1 =
            (latMax - (coordinate.latitude_ - spacingDeg)) /
            (latMax - latMin) * h;

         painter.fillRect(QRectF {x0, y0, x1 - x0, y1 - y0},
                          LikelihoodColor(value));
      }
   }
}

} // namespace scwx::qt::ui
