#include <scwx/provider/hrrr_provider.hpp>

#include <gtest/gtest.h>

namespace scwx::provider
{

// Live-network test: fetches the latest HRRR analysis and samples a few CONUS
// points. Exercises the .idx parse, byte-range fetch, GRIB2/JPEG2000 decode,
// and Lambert-Conformal sampling end to end.
TEST(HrrrProviderTest, FetchInputs)
{
   HrrrProvider provider {};

   const std::vector<common::Coordinate> points {
      {35.0, -97.5}, {41.0, -88.4}, {32.0, -95.0}, {44.0, -103.0}};

   auto result = provider.FetchInputs(points);
   ASSERT_TRUE(result.has_value());

   const auto& inputs = result.value();
   EXPECT_EQ(inputs.size(), points.size());

   for (const auto& in : inputs)
   {
      // CAPE is non-negative and physically bounded; CIN is non-positive-ish
      // but we store magnitude-agnostic. Just sanity-check ranges.
      EXPECT_GE(in.capeJPerKg_, 0.0);
      EXPECT_LT(in.capeJPerKg_, 10000.0);
      EXPECT_GE(in.relativeHumidity2mPercent_, 0.0);
      EXPECT_LE(in.relativeHumidity2mPercent_, 100.0);
      EXPECT_GE(in.windSpeed10mMs_, 0.0);
      EXPECT_LT(in.windSpeed10mMs_, 150.0);
   }
}

} // namespace scwx::provider
