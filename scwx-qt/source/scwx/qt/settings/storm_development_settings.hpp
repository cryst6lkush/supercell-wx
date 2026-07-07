#pragma once

#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_variable.hpp>

#include <cstdint>
#include <memory>

namespace scwx::qt::settings
{

class StormDevelopmentSettings : public SettingsCategory
{
public:
   explicit StormDevelopmentSettings();
   ~StormDevelopmentSettings() override;

   StormDevelopmentSettings(const StormDevelopmentSettings&)            = delete;
   StormDevelopmentSettings& operator=(const StormDevelopmentSettings&) = delete;

   StormDevelopmentSettings(StormDevelopmentSettings&&) noexcept;
   StormDevelopmentSettings& operator=(StormDevelopmentSettings&&) noexcept;

   [[nodiscard]] SettingsVariable<bool>& enabled() const;
   [[nodiscard]] SettingsVariable<std::int64_t>&
   refresh_interval_minutes() const;

   static StormDevelopmentSettings& Instance();

   friend bool operator==(const StormDevelopmentSettings& lhs,
                          const StormDevelopmentSettings& rhs);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::settings
