#include <scwx/common/storm_development_index.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace common
{

TEST(StormDevelopmentIndex, AllZeroInputsIsLow)
{
   StormDevelopmentInputs inputs {};

   EXPECT_NEAR(ComputeStormDevelopmentScore(inputs), 0.0, 0.001);
}

TEST(StormDevelopmentIndex, MaxFavorableInputsIsNearMax)
{
   StormDevelopmentInputs inputs {};
   inputs.capeJPerKg_                 = 4000.0;
   inputs.relativeHumidity2mPercent_  = 100.0;
   inputs.windSpeed10mMs_             = 25.0;
   inputs.convectiveInhibitionJPerKg_ = 0.0;

   EXPECT_NEAR(ComputeStormDevelopmentScore(inputs), 100.0, 0.001);
}

TEST(StormDevelopmentIndex, LargeCinNeverGoesBelowZero)
{
   StormDevelopmentInputs inputs {};
   inputs.convectiveInhibitionJPerKg_ = -10000.0;

   EXPECT_GE(ComputeStormDevelopmentScore(inputs), 0.0);
}

TEST(StormDevelopmentIndex, ResultAlwaysInRange)
{
   StormDevelopmentInputs inputs {};
   inputs.capeJPerKg_                 = 8000.0;
   inputs.relativeHumidity2mPercent_  = 250.0;
   inputs.windSpeed10mMs_             = 90.0;
   inputs.convectiveInhibitionJPerKg_ = -500.0;

   const double score = ComputeStormDevelopmentScore(inputs);
   EXPECT_GE(score, 0.0);
   EXPECT_LE(score, 100.0);
}

TEST(StormDevelopmentIndex, CinPenalizesScore)
{
   StormDevelopmentInputs favorable {};
   favorable.capeJPerKg_                = 2000.0;
   favorable.relativeHumidity2mPercent_ = 60.0;
   favorable.windSpeed10mMs_            = 10.0;

   StormDevelopmentInputs withCin        = favorable;
   withCin.convectiveInhibitionJPerKg_   = 150.0;

   EXPECT_LT(ComputeStormDevelopmentScore(withCin),
             ComputeStormDevelopmentScore(favorable));
}

} // namespace common
} // namespace scwx
