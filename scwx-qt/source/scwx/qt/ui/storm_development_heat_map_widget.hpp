#pragma once

#include <scwx/common/grid.hpp>

#include <memory>

#include <QWidget>

namespace scwx
{
namespace qt
{
namespace ui
{

/**
 * @brief Paints a CONUS-wide storm development likelihood grid as a colored
 * heat map.
 */
class StormDevelopmentHeatMapWidget : public QWidget
{
   Q_OBJECT

public:
   explicit StormDevelopmentHeatMapWidget(QWidget* parent = nullptr);
   ~StormDevelopmentHeatMapWidget() override;

   void SetGrid(std::shared_ptr<const common::LatLonGrid> grid);

protected:
   void paintEvent(QPaintEvent* event) override;

private:
   std::shared_ptr<const common::LatLonGrid> grid_ {};
};

} // namespace ui
} // namespace qt
} // namespace scwx
