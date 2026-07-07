#pragma once

namespace scwx
{
namespace common
{

/**
 * @brief Inputs to the storm development likelihood heuristic.
 */
struct StormDevelopmentInputs
{
   double capeJPerKg_ {};
   double convectiveInhibitionJPerKg_ {};
   double relativeHumidity2mPercent_ {};
   double windSpeed10mMs_ {};
};

/**
 * @brief Compute a 0-100 storm development likelihood score from a simple
 * weighted heuristic (CAPE, humidity, CIN penalty, wind-speed shear proxy).
 */
double ComputeStormDevelopmentScore(const StormDevelopmentInputs& inputs);

} // namespace common
} // namespace scwx
