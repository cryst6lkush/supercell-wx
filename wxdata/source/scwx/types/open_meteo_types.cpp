#include <scwx/types/open_meteo_types.hpp>

#include <boost/json/value_to.hpp>

namespace scwx::types::open_meteo
{

// Open-Meteo returns whole-number fields (e.g. "cape": 0) as JSON integers,
// so as_double() alone would throw -- normalize across kinds here.
static double AsDouble(const boost::json::value& jv)
{
   if (jv.is_double())
   {
      return jv.as_double();
   }
   if (jv.is_int64())
   {
      return static_cast<double>(jv.as_int64());
   }
   if (jv.is_uint64())
   {
      return static_cast<double>(jv.as_uint64());
   }

   return 0.0;
}

CurrentBlock tag_invoke(boost::json::value_to_tag<CurrentBlock>,
                        const boost::json::value& jv)
{
   auto& jo = jv.as_object();

   CurrentBlock block {};

   block.capeJPerKg_                 = AsDouble(jo.at("cape"));
   block.convectiveInhibitionJPerKg_ = AsDouble(jo.at("convective_inhibition"));
   block.relativeHumidity2mPercent_  = AsDouble(jo.at("relative_humidity_2m"));
   block.surfacePressureHpa_         = AsDouble(jo.at("surface_pressure"));
   block.pressureMslHpa_             = AsDouble(jo.at("pressure_msl"));
   block.windSpeed10mMs_             = AsDouble(jo.at("wind_speed_10m"));
   block.dewPoint2mC_                = AsDouble(jo.at("dew_point_2m"));

   return block;
}

PointResponse tag_invoke(boost::json::value_to_tag<PointResponse>,
                         const boost::json::value& jv)
{
   auto& jo = jv.as_object();

   PointResponse response {};

   response.latitude_  = AsDouble(jo.at("latitude"));
   response.longitude_ = AsDouble(jo.at("longitude"));
   response.current_ =
      boost::json::value_to<CurrentBlock>(jo.at("current"));

   return response;
}

} // namespace scwx::types::open_meteo
