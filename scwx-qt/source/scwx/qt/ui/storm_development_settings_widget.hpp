#pragma once

#include <memory>

#include <QWidget>

namespace scwx
{
namespace qt
{
namespace ui
{

/**
 * @brief Toolbox controls for the Storm Development heat map: enable
 * checkbox + refresh interval.
 */
class StormDevelopmentSettingsWidget : public QWidget
{
   Q_OBJECT

public:
   explicit StormDevelopmentSettingsWidget(QWidget* parent = nullptr);
   ~StormDevelopmentSettingsWidget() override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace ui
} // namespace qt
} // namespace scwx
