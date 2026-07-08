#pragma once

#include <scwx/common/geographic.hpp>
#include <scwx/common/storm_development_index.hpp>

#include <memory>
#include <vector>

#include <boost/outcome/result.hpp>

namespace scwx::provider
{

/**
 * @brief NOAA HRRR provider.
 *
 * Fetches the latest HRRR analysis (surface CAPE/CIN, 2 m RH, 10 m wind)
 * directly from the free public GRIB2 archive on AWS, using the .idx sidecar
 * to byte-range only the needed fields, and samples them onto the requested
 * coordinates via the HRRR Lambert-Conformal grid. No API key or per-point
 * quota, native 3 km resolution.
 */
class HrrrProvider
{
public:
   explicit HrrrProvider();
   ~HrrrProvider();

   HrrrProvider(const HrrrProvider&)            = delete;
   HrrrProvider& operator=(const HrrrProvider&) = delete;

   HrrrProvider(HrrrProvider&&) noexcept;
   HrrrProvider& operator=(HrrrProvider&&) noexcept;

   /**
    * @brief Fetch storm-development inputs for each point, in request order.
    */
   boost::outcome_v2::result<std::vector<common::StormDevelopmentInputs>>
   FetchInputs(const std::vector<common::Coordinate>& points);

   void Shutdown() noexcept;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
