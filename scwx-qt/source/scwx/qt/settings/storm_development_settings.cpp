#include <scwx/qt/settings/storm_development_settings.hpp>

namespace scwx::qt::settings
{

class StormDevelopmentSettings::Impl
{
public:
   explicit Impl()
   {
      // SetDefault, SetMinimum, and SetMaximum are descriptive
      // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
      enabled_.SetDefault(true);
      // HRRR posts hourly, so refreshing faster only re-fetches the same run.
      refreshIntervalMinutes_.SetDefault(60);
      refreshIntervalMinutes_.SetMinimum(15);
      refreshIntervalMinutes_.SetMaximum(120);
      // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
   }

   ~Impl()                       = default;
   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   SettingsVariable<bool> enabled_ {"enabled"};
   SettingsVariable<std::int64_t> refreshIntervalMinutes_ {
      "refresh_interval_minutes"};
};

StormDevelopmentSettings::StormDevelopmentSettings() :
    SettingsCategory("storm_development"), p(std::make_unique<Impl>())
{
   RegisterVariables({&p->enabled_, &p->refreshIntervalMinutes_});
   SetDefaults();
}
StormDevelopmentSettings::~StormDevelopmentSettings() = default;

StormDevelopmentSettings::StormDevelopmentSettings(
   StormDevelopmentSettings&&) noexcept = default;
StormDevelopmentSettings& StormDevelopmentSettings::operator=(
   StormDevelopmentSettings&&) noexcept = default;

SettingsVariable<bool>& StormDevelopmentSettings::enabled() const
{
   return p->enabled_;
}

SettingsVariable<std::int64_t>&
StormDevelopmentSettings::refresh_interval_minutes() const
{
   return p->refreshIntervalMinutes_;
}

StormDevelopmentSettings& StormDevelopmentSettings::Instance()
{
   static StormDevelopmentSettings stormDevelopmentSettings_;
   return stormDevelopmentSettings_;
}

bool operator==(const StormDevelopmentSettings& lhs,
                const StormDevelopmentSettings& rhs)
{
   return (lhs.p->enabled_ == rhs.p->enabled_ &&
           lhs.p->refreshIntervalMinutes_ == rhs.p->refreshIntervalMinutes_);
}

} // namespace scwx::qt::settings
