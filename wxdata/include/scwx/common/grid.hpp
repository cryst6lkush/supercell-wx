#pragma once

#include <scwx/common/geographic.hpp>

#include <cstddef>
#include <vector>

namespace scwx
{
namespace common
{

/**
 * @brief A regular lat/lon grid of scalar values over a fixed bounding box.
 *
 * Cells are stored row-major, north to south (row 0 = latMax) and west to
 * east (column 0 = lonMin), so a flat response list from a batched
 * multi-point query can be mapped directly onto @c Points() order.
 */
class LatLonGrid
{
public:
   LatLonGrid(double latMin,
              double latMax,
              double lonMin,
              double lonMax,
              double spacingDegrees);
   ~LatLonGrid();

   LatLonGrid(const LatLonGrid&)            = default;
   LatLonGrid& operator=(const LatLonGrid&) = default;
   LatLonGrid(LatLonGrid&&)                 = default;
   LatLonGrid& operator=(LatLonGrid&&)      = default;

   std::size_t RowCount() const;
   std::size_t ColumnCount() const;

   double LatMin() const;
   double LatMax() const;
   double LonMin() const;
   double LonMax() const;
   double SpacingDegrees() const;

   Coordinate CoordinateAt(std::size_t row, std::size_t column) const;

   double ValueAt(std::size_t row, std::size_t column) const;
   void   SetValue(std::size_t row, std::size_t column, double value);

   /**
    * @brief Coordinates of every cell, in the same row-major order as the
    * underlying value storage.
    */
   std::vector<Coordinate> Points() const;

private:
   std::size_t Index(std::size_t row, std::size_t column) const;

   double latMin_;
   double latMax_;
   double lonMin_;
   double lonMax_;
   double spacingDegrees_;

   std::size_t rowCount_;
   std::size_t columnCount_;

   std::vector<double> values_;
};

} // namespace common
} // namespace scwx
