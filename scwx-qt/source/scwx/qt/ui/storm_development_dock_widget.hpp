#pragma once

#include <memory>

#include <QDockWidget>

namespace scwx
{
namespace qt
{
namespace ui
{

/**
 * @brief Standalone dock pane showing the CONUS storm development
 * likelihood heat map, independent of the currently loaded radar product.
 */
class StormDevelopmentDockWidget : public QDockWidget
{
   Q_OBJECT

public:
   explicit StormDevelopmentDockWidget(QWidget* parent = nullptr);
   ~StormDevelopmentDockWidget() override;

   StormDevelopmentDockWidget(const StormDevelopmentDockWidget&) = delete;
   StormDevelopmentDockWidget&
   operator=(const StormDevelopmentDockWidget&) = delete;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace ui
} // namespace qt
} // namespace scwx
