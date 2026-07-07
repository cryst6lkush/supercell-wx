#include <scwx/common/storm_development_index.hpp>

#include <algorithm>
#include <cmath>

namespace scwx
{
namespace common
{

static constexpr double kCapeWeight = 0.50;
static constexpr double kRhWeight   = 0.20;
static constexpr double kWindWeight = 0.30;
static constexpr double kCinWeight  = 0.30;

static constexpr double kCapeMaxJPerKg = 4000.0;
static constexpr double kWindMaxMs     = 25.0;
static constexpr double kCinMaxJPerKg  = 200.0;

static double NormalizeToPercent(double value, double max)
{
   return std::clamp(value, 0.0, max) / max * 100.0;
}

double ComputeStormDevelopmentScore(const StormDevelopmentInputs& inputs)
{
   const double capeScore = NormalizeToPercent(inputs.capeJPerKg_, kCapeMaxJPerKg);
   const double rhScore = std::clamp(inputs.relativeHumidity2mPercent_, 0.0, 100.0);

   // ponytail: wind_speed_10m is a crude "shear proxy", not shear. Real
   // SCP/STP need 0-6km bulk shear + 0-1km SRH from a full wind profile.
   // Open-Meteo already exposes pressure-level wind fields
   // (wind_speed_850hPa/700hPa/500hPa) that could supply this without
   // switching providers -- swap this term out first when upgrading past v1.
   const double windScore = NormalizeToPercent(inputs.windSpeed10mMs_, kWindMaxMs);

   const double cinPenalty = NormalizeToPercent(
      std::abs(inputs.convectiveInhibitionJPerKg_), kCinMaxJPerKg);

   const double score = kCapeWeight * capeScore + kRhWeight * rhScore +
                         kWindWeight * windScore - kCinWeight * cinPenalty;

   return std::clamp(score, 0.0, 100.0);
}

} // namespace common
} // namespace scwx
