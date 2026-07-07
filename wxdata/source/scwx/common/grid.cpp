#include <scwx/common/grid.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace scwx
{
namespace common
{

static std::size_t SpanCount(double min, double max, double spacingDegrees)
{
   return static_cast<std::size_t>(std::round((max - min) / spacingDegrees)) +
          1;
}

LatLonGrid::LatLonGrid(double latMin,
                       double latMax,
                       double lonMin,
                       double lonMax,
                       double spacingDegrees) :
    latMin_ {latMin},
    latMax_ {latMax},
    lonMin_ {lonMin},
    lonMax_ {lonMax},
    spacingDegrees_ {spacingDegrees},
    rowCount_ {SpanCount(latMin, latMax, spacingDegrees)},
    columnCount_ {SpanCount(lonMin, lonMax, spacingDegrees)},
    values_(rowCount_ * columnCount_,
            std::numeric_limits<double>::quiet_NaN())
{
}

LatLonGrid::~LatLonGrid() = default;

std::size_t LatLonGrid::RowCount() const
{
   return rowCount_;
}

std::size_t LatLonGrid::ColumnCount() const
{
   return columnCount_;
}

double LatLonGrid::LatMin() const
{
   return latMin_;
}

double LatLonGrid::LatMax() const
{
   return latMax_;
}

double LatLonGrid::LonMin() const
{
   return lonMin_;
}

double LatLonGrid::LonMax() const
{
   return lonMax_;
}

double LatLonGrid::SpacingDegrees() const
{
   return spacingDegrees_;
}

std::size_t LatLonGrid::Index(std::size_t row, std::size_t column) const
{
   if (row >= rowCount_ || column >= columnCount_)
   {
      throw std::out_of_range("LatLonGrid index out of range");
   }

   return row * columnCount_ + column;
}

Coordinate LatLonGrid::CoordinateAt(std::size_t row, std::size_t column) const
{
   if (row >= rowCount_ || column >= columnCount_)
   {
      throw std::out_of_range("LatLonGrid index out of range");
   }

   return Coordinate {latMax_ - static_cast<double>(row) * spacingDegrees_,
                       lonMin_ + static_cast<double>(column) * spacingDegrees_};
}

double LatLonGrid::ValueAt(std::size_t row, std::size_t column) const
{
   return values_[Index(row, column)];
}

void LatLonGrid::SetValue(std::size_t row, std::size_t column, double value)
{
   values_[Index(row, column)] = value;
}

std::vector<Coordinate> LatLonGrid::Points() const
{
   std::vector<Coordinate> points {};
   points.reserve(rowCount_ * columnCount_);

   for (std::size_t row = 0; row < rowCount_; ++row)
   {
      for (std::size_t column = 0; column < columnCount_; ++column)
      {
         points.push_back(CoordinateAt(row, column));
      }
   }

   return points;
}

} // namespace common
} // namespace scwx
