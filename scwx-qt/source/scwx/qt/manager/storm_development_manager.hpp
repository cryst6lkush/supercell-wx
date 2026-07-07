#pragma once

#include <scwx/common/grid.hpp>

#include <memory>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

/**
 * @brief Periodically fetches CONUS atmospheric data from Open-Meteo and
 * computes a storm development likelihood grid.
 */
class StormDevelopmentManager : public QObject
{
   Q_OBJECT

public:
   explicit StormDevelopmentManager();
   ~StormDevelopmentManager();

   StormDevelopmentManager(const StormDevelopmentManager&)            = delete;
   StormDevelopmentManager& operator=(const StormDevelopmentManager&) = delete;

   /**
    * @brief Thread-safe snapshot of the current CONUS storm development
    * grid. Returns nullptr before the first successful refresh.
    */
   std::shared_ptr<const common::LatLonGrid> Grid() const;

   /**
    * @brief Triggers an immediate refresh, outside the normal schedule.
    */
   void Refresh();

   static std::shared_ptr<StormDevelopmentManager> Instance();

signals:
   void GridUpdated();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
