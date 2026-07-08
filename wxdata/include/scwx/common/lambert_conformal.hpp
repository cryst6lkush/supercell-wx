#pragma once

#include <cmath>
#include <numbers>

namespace scwx
{
namespace common
{

/**
 * @brief Spherical Lambert Conformal Conic projection for a GRIB2 grid.
 *
 * Maps (latitude, longitude) to fractional grid indices (i, j), where i runs
 * west-to-east and j runs south-to-north (GRIB2 scanning mode 0b01000000).
 * Defaults are the fixed NOAA HRRR CONUS 3 km grid.
 */
struct LambertConformalGrid
{
   double earthRadiusM_ = 6371229.0; // HRRR spherical earth (GRIB shape 6)
   double latin1Deg_    = 38.5;      // first standard parallel
   double latin2Deg_    = 38.5;      // second standard parallel
   double lovDeg_       = -97.5;     // orientation longitude (grid +y up)
   double ladDeg_       = 38.5;      // latitude of Dx/Dy
   double la1Deg_       = 21.138123; // latitude of first grid point (i=j=0)
   double lo1Deg_       = -122.719528; // longitude of first grid point
   double dxM_          = 3000.0;
   double dyM_          = 3000.0;
   std::size_t nx_      = 1799;
   std::size_t ny_      = 1059;

   struct GridIndex
   {
      double i;
      double j;
   };

   [[nodiscard]] double ConeConstant() const
   {
      const double phi1 = latin1Deg_ * kDegToRad;
      const double phi2 = latin2Deg_ * kDegToRad;
      if (std::abs(phi1 - phi2) < 1.0e-9)
      {
         return std::sin(phi1);
      }
      return std::log(std::cos(phi1) / std::cos(phi2)) /
             std::log(std::tan(kQuarterPi + phi2 * 0.5) /
                      std::tan(kQuarterPi + phi1 * 0.5));
   }

   [[nodiscard]] GridIndex ToGridIndex(double latDeg, double lonDeg) const
   {
      const double n = ConeConstant();
      const double phi1 = latin1Deg_ * kDegToRad;
      const double f =
         std::cos(phi1) * std::pow(std::tan(kQuarterPi + phi1 * 0.5), n) / n;

      const auto rho = [&](double latRad)
      { return earthRadiusM_ * f / std::pow(std::tan(kQuarterPi + latRad * 0.5), n); };

      const double rho0 = rho(ladDeg_ * kDegToRad);

      const auto project = [&](double lat, double lon, double& x, double& y)
      {
         double dLon = lon - lovDeg_;
         while (dLon > 180.0) dLon -= 360.0;
         while (dLon < -180.0) dLon += 360.0;
         const double theta = n * dLon * kDegToRad;
         const double r     = rho(lat * kDegToRad);
         x                  = r * std::sin(theta);
         y                  = rho0 - r * std::cos(theta);
      };

      double x0 = 0.0;
      double y0 = 0.0;
      project(la1Deg_, lo1Deg_, x0, y0);

      double x = 0.0;
      double y = 0.0;
      project(latDeg, lonDeg, x, y);

      return {(x - x0) / dxM_, (y - y0) / dyM_};
   }

private:
   static constexpr double kDegToRad   = std::numbers::pi / 180.0;
   static constexpr double kQuarterPi  = std::numbers::pi / 4.0;
};

} // namespace common
} // namespace scwx
