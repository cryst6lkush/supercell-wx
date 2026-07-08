#pragma once

#include <scwx/qt/map/draw_layer.hpp>

namespace scwx::qt::map
{

/**
 * @brief Draws the CONUS storm-development likelihood field as a
 * semi-transparent, geo-referenced, interpolated overlay on the map.
 */
class StormDevelopmentLayer : public DrawLayer
{
   Q_DISABLE_COPY_MOVE(StormDevelopmentLayer)

public:
   explicit StormDevelopmentLayer(const std::shared_ptr<gl::GlContext>& glContext);
   ~StormDevelopmentLayer();

   void Initialize(const std::shared_ptr<MapContext>& mapContext) override;
   void Render(const std::shared_ptr<MapContext>& mapContext,
               const QMapLibre::CustomLayerRenderParameters&) override;
   void Deinitialize() override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
