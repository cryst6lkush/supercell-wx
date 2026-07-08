#include <scwx/common/lambert_conformal.hpp>

#include <cmath>

#include <gtest/gtest.h>

namespace scwx
{
namespace common
{

TEST(LambertConformal, FirstGridPointIsOrigin)
{
   LambertConformalGrid grid {};

   const auto index = grid.ToGridIndex(grid.la1Deg_, grid.lo1Deg_);
   EXPECT_NEAR(index.i, 0.0, 1.0e-3);
   EXPECT_NEAR(index.j, 0.0, 1.0e-3);
}

TEST(LambertConformal, GridCenterIsInBounds)
{
   LambertConformalGrid grid {};

   // The orientation-longitude / reference-latitude point sits near the
   // interior of the CONUS domain.
   const auto index = grid.ToGridIndex(grid.ladDeg_, grid.lovDeg_);
   EXPECT_GT(index.i, 0.0);
   EXPECT_LT(index.i, static_cast<double>(grid.nx_));
   EXPECT_GT(index.j, 0.0);
   EXPECT_LT(index.j, static_cast<double>(grid.ny_));
}

TEST(LambertConformal, EastwardIncreasesColumn)
{
   LambertConformalGrid grid {};

   const auto west = grid.ToGridIndex(40.0, -100.0);
   const auto east = grid.ToGridIndex(40.0, -95.0);
   EXPECT_GT(east.i, west.i);
}

TEST(LambertConformal, NorthwardIncreasesRow)
{
   LambertConformalGrid grid {};

   const auto south = grid.ToGridIndex(35.0, -97.5);
   const auto north = grid.ToGridIndex(45.0, -97.5);
   EXPECT_GT(north.j, south.j);
}

} // namespace common
} // namespace scwx
