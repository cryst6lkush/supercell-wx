#pragma once

#include <scwx/qt/map/map_widget.hpp>

#include <optional>

namespace scwx::qt::ui
{

class Level3SettingsWidgetImpl;

class Level3SettingsWidget : public QWidget
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(Level3SettingsWidget)

public:
   explicit Level3SettingsWidget(QWidget* parent = nullptr);
   ~Level3SettingsWidget() override;

   bool UpdateThreshold(map::MapWidget* activeMap);

   /**
    * @brief Show the Mean radius (km) control only when the active product is the
    * Relative SRV (SRM, code 56) view, and sync it from settings.
    *
    * @return true if the radius control is visible, otherwise false.
    */
   bool UpdateMeanRadius(map::MapWidget* activeMap);

signals:
   void ThresholdChanged(std::optional<float> threshold);

private:
   std::shared_ptr<Level3SettingsWidgetImpl> p;
};

} // namespace scwx::qt::ui
