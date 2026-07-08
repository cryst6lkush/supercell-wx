#include <scwx/qt/manager/storm_development_manager.hpp>
#include <scwx/qt/settings/storm_development_settings.hpp>
#include <scwx/common/storm_development_index.hpp>
#include <scwx/provider/hrrr_provider.hpp>
#include <scwx/util/logger.hpp>

#include <mutex>

#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>

namespace scwx::qt::manager
{

static const std::string logPrefix_ =
   "scwx::qt::manager::storm_development_manager";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

// CONUS bounding box at 1-degree spacing (~1600 points). The HRRR provider is
// unmetered (one hourly GRIB2 download, not per-point requests), so a finer
// grid costs nothing extra. The heat-map widget interpolates for display.
static constexpr double kConusLatMin_     = 24.0;
static constexpr double kConusLatMax_     = 50.0;
static constexpr double kConusLonMin_     = -125.0;
static constexpr double kConusLonMax_     = -66.0;
static constexpr double kConusSpacingDeg_ = 1.0;

class StormDevelopmentManager::Impl
{
public:
   explicit Impl(StormDevelopmentManager* self) : self_ {self} {}
   ~Impl()
   {
      auto& settings = settings::StormDevelopmentSettings::Instance();
      settings.enabled().UnregisterValueChangedCallback(enabledCallbackUuid_);
      settings.refresh_interval_minutes().UnregisterValueChangedCallback(
         refreshIntervalCallbackUuid_);

      std::unique_lock lock {timerMutex_};
      refreshTimer_.cancel();
      lock.unlock();

      threadPool_.join();
   }
   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void ArmTimer(std::chrono::seconds delay);
   void OnTimerFired(const boost::system::error_code& e);
   void RefreshAsync();
   void Refresh();

   StormDevelopmentManager* self_;

   boost::asio::thread_pool  threadPool_ {1u};
   boost::asio::steady_timer refreshTimer_ {threadPool_};
   std::mutex                timerMutex_ {};

   provider::HrrrProvider hrrrProvider_ {};

   boost::uuids::uuid enabledCallbackUuid_ {};
   boost::uuids::uuid refreshIntervalCallbackUuid_ {};

   mutable std::mutex                        gridMutex_ {};
   std::shared_ptr<const common::LatLonGrid> grid_ {};
};

StormDevelopmentManager::StormDevelopmentManager() :
    p(std::make_unique<Impl>(this))
{
   auto& settings = settings::StormDevelopmentSettings::Instance();

   p->enabledCallbackUuid_ = settings.enabled().RegisterValueChangedCallback(
      [this](const bool& enabled)
      {
         if (enabled)
         {
            p->RefreshAsync();
         }
         else
         {
            std::unique_lock lock {p->timerMutex_};
            p->refreshTimer_.cancel();
         }
      });
   p->refreshIntervalCallbackUuid_ =
      settings.refresh_interval_minutes().RegisterValueChangedCallback(
         [this](const std::int64_t&)
         {
            if (settings::StormDevelopmentSettings::Instance()
                   .enabled()
                   .GetValue())
            {
               p->RefreshAsync();
            }
         });

   if (settings.enabled().GetValue())
   {
      p->RefreshAsync();
   }
}

StormDevelopmentManager::~StormDevelopmentManager() = default;

std::shared_ptr<const common::LatLonGrid> StormDevelopmentManager::Grid() const
{
   std::unique_lock lock {p->gridMutex_};
   return p->grid_;
}

void StormDevelopmentManager::Refresh()
{
   p->RefreshAsync();
}

void StormDevelopmentManager::Impl::ArmTimer(std::chrono::seconds delay)
{
   std::unique_lock lock {timerMutex_};
   refreshTimer_.expires_after(delay);
   refreshTimer_.async_wait([this](const boost::system::error_code& e)
                            { OnTimerFired(e); });
}

void StormDevelopmentManager::Impl::OnTimerFired(
   const boost::system::error_code& e)
{
   if (e == boost::asio::error::operation_aborted)
   {
      return;
   }

   if (settings::StormDevelopmentSettings::Instance().enabled().GetValue())
   {
      RefreshAsync();
   }
}

void StormDevelopmentManager::Impl::RefreshAsync()
{
   boost::asio::post(threadPool_,
                     [this]()
                     {
                        try
                        {
                           Refresh();
                        }
                        catch (const std::exception& ex)
                        {
                           logger_->error(ex.what());
                        }
                     });
}

void StormDevelopmentManager::Impl::Refresh()
{
   logger_->debug("Refreshing storm development grid");

   common::LatLonGrid grid {kConusLatMin_,
                            kConusLatMax_,
                            kConusLonMin_,
                            kConusLonMax_,
                            kConusSpacingDeg_};

   auto points = grid.Points();
   auto result = hrrrProvider_.FetchInputs(points);

   if (!result.has_value())
   {
      logger_->warn("Storm development fetch failed: {}",
                    result.error().message());
   }
   else if (result.value().size() != points.size())
   {
      logger_->warn("Storm development response count mismatch: {} != {}",
                    result.value().size(),
                    points.size());
   }
   else
   {
      const auto& inputs = result.value();

      for (std::size_t i = 0; i < inputs.size(); ++i)
      {
         const std::size_t row    = i / grid.ColumnCount();
         const std::size_t column = i % grid.ColumnCount();
         grid.SetValue(
            row, column, common::ComputeStormDevelopmentScore(inputs[i]));
      }

      {
         std::unique_lock lock {gridMutex_};
         grid_ = std::make_shared<const common::LatLonGrid>(std::move(grid));
      }

      Q_EMIT self_->GridUpdated();
   }

   ArmTimer(std::chrono::minutes(settings::StormDevelopmentSettings::Instance()
                                    .refresh_interval_minutes()
                                    .GetValue()));
}

std::shared_ptr<StormDevelopmentManager> StormDevelopmentManager::Instance()
{
   static std::weak_ptr<StormDevelopmentManager> stormDevelopmentManagerRef_ {};
   static std::mutex                             instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::shared_ptr<StormDevelopmentManager> stormDevelopmentManager =
      stormDevelopmentManagerRef_.lock();

   if (stormDevelopmentManager == nullptr)
   {
      stormDevelopmentManager     = std::make_shared<StormDevelopmentManager>();
      stormDevelopmentManagerRef_ = stormDevelopmentManager;
   }

   return stormDevelopmentManager;
}

} // namespace scwx::qt::manager
