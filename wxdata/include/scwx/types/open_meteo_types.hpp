#pragma once

#include <boost/json/value.hpp>

namespace scwx::types::open_meteo
{

/**
 * @brief The "current" block of an Open-Meteo forecast API response.
 *
 * <https://open-meteo.com/en/docs>
 */
struct CurrentBlock
{
   double capeJPerKg_ {};
   double convectiveInhibitionJPerKg_ {};
   double relativeHumidity2mPercent_ {};
   double surfacePressureHpa_ {};
   double pressureMslHpa_ {};
   double windSpeed10mMs_ {};
   double dewPoint2mC_ {};
};

/**
 * @brief A single point's response object, as returned inside the JSON array
 * produced by a multi-coordinate Open-Meteo request.
 */
struct PointResponse
{
   double       latitude_ {};
   double       longitude_ {};
   CurrentBlock current_ {};
};

CurrentBlock  tag_invoke(boost::json::value_to_tag<CurrentBlock>,
                         const boost::json::value& jv);
PointResponse tag_invoke(boost::json::value_to_tag<PointResponse>,
                         const boost::json::value& jv);

} // namespace scwx::types::open_meteo
