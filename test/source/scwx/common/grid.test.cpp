#include <scwx/common/grid.hpp>

#include <cmath>
#include <stdexcept>

#include <gtest/gtest.h>

namespace scwx
{
namespace common
{

TEST(Grid, Dimensions)
{
   // Bbox width isn't an exact multiple of spacing (59 / 2 = 29.5), so the
   // rounded column count (31) overshoots lonMax by one cell (-65 vs -66).
   LatLonGrid grid {24.0, 50.0, -125.0, -66.0, 2.0};

   EXPECT_EQ(grid.RowCount(), 14u);
   EXPECT_EQ(grid.ColumnCount(), 31u);
}

TEST(Grid, CoordinateAtCorners)
{
   LatLonGrid grid {24.0, 50.0, -125.0, -66.0, 2.0};

   const Coordinate northwest = grid.CoordinateAt(0, 0);
   EXPECT_DOUBLE_EQ(northwest.latitude_, 50.0);
   EXPECT_DOUBLE_EQ(northwest.longitude_, -125.0);

   const Coordinate southeast =
      grid.CoordinateAt(grid.RowCount() - 1, grid.ColumnCount() - 1);
   EXPECT_DOUBLE_EQ(southeast.latitude_, 24.0);
   EXPECT_DOUBLE_EQ(southeast.longitude_, -65.0);
}

TEST(Grid, SetAndGetValue)
{
   LatLonGrid grid {24.0, 50.0, -125.0, -66.0, 2.0};

   EXPECT_TRUE(std::isnan(grid.ValueAt(1, 2)));

   grid.SetValue(1, 2, 42.0);
   EXPECT_DOUBLE_EQ(grid.ValueAt(1, 2), 42.0);
}

TEST(Grid, PointsMatchRowMajorOrder)
{
   LatLonGrid grid {24.0, 50.0, -125.0, -66.0, 2.0};

   const auto points = grid.Points();
   EXPECT_EQ(points.size(), grid.RowCount() * grid.ColumnCount());
   EXPECT_EQ(points.front(), grid.CoordinateAt(0, 0));
   EXPECT_EQ(points.back(),
             grid.CoordinateAt(grid.RowCount() - 1, grid.ColumnCount() - 1));
}

TEST(Grid, BboxAccessors)
{
   LatLonGrid grid {24.0, 50.0, -125.0, -66.0, 2.0};

   EXPECT_DOUBLE_EQ(grid.LatMin(), 24.0);
   EXPECT_DOUBLE_EQ(grid.LatMax(), 50.0);
   EXPECT_DOUBLE_EQ(grid.LonMin(), -125.0);
   EXPECT_DOUBLE_EQ(grid.LonMax(), -66.0);
   EXPECT_DOUBLE_EQ(grid.SpacingDegrees(), 2.0);
}

TEST(Grid, OutOfRangeThrows)
{
   LatLonGrid grid {24.0, 50.0, -125.0, -66.0, 2.0};

   EXPECT_THROW(grid.ValueAt(grid.RowCount(), 0), std::out_of_range);
   EXPECT_THROW(grid.CoordinateAt(0, grid.ColumnCount()), std::out_of_range);
}

} // namespace common
} // namespace scwx
