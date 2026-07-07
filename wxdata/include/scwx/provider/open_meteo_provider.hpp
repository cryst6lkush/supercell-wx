#pragma once

#include <scwx/common/geographic.hpp>
#include <scwx/types/open_meteo_types.hpp>

#include <memory>
#include <vector>

#include <boost/outcome/result.hpp>

namespace scwx::provider
{

/**
 * @brief Open-Meteo Forecast API Provider
 *
 * <https://open-meteo.com/en/docs>
 */
class OpenMeteoProvider
{
public:
   explicit OpenMeteoProvider();
   ~OpenMeteoProvider();

   OpenMeteoProvider(const OpenMeteoProvider&)            = delete;
   OpenMeteoProvider& operator=(const OpenMeteoProvider&) = delete;

   OpenMeteoProvider(OpenMeteoProvider&&) noexcept;
   OpenMeteoProvider& operator=(OpenMeteoProvider&&) noexcept;

   /**
    * @brief Fetch current-conditions data for a list of points, in the same
    * order as requested. Batches requests internally to stay within a
    * reasonable per-request point count.
    */
   boost::outcome_v2::result<std::vector<types::open_meteo::PointResponse>>
   FetchGrid(const std::vector<common::Coordinate>& points);

   /**
    * @brief Shuts down the provider and stops any in-progress network
    * requests.
    */
   void Shutdown() noexcept;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
