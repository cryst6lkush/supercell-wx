#include <scwx/provider/open_meteo_provider.hpp>

#include <gtest/gtest.h>

namespace scwx::provider
{

TEST(OpenMeteoProviderTest, FetchGrid)
{
   OpenMeteoProvider provider {};

   // CONUS corners plus a couple of interior points
   const std::vector<common::Coordinate> points {
      {24.0, -125.0}, {50.0, -125.0}, {24.0, -66.0}, {50.0, -66.0}};

   auto result = provider.FetchGrid(points);
   ASSERT_TRUE(result.has_value());

   const auto& responses = result.value();
   EXPECT_EQ(responses.size(), points.size());

   for (const auto& response : responses)
   {
      EXPECT_GE(response.current_.capeJPerKg_, 0.0);
      EXPECT_GE(response.current_.relativeHumidity2mPercent_, 0.0);
      EXPECT_LE(response.current_.relativeHumidity2mPercent_, 100.0);
   }
}

} // namespace scwx::provider
