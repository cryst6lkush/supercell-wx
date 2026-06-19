#pragma once

#include <scwx/qt/view/level3_radial_view.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <boost/uuid/uuid.hpp>

namespace scwx::qt::view
{

/**
 * @brief Single-radar "Relative SRV" product.
 *
 * Selected as Storm Relative Velocity (SRM, code 56), but reads the base
 * velocity feed and renders a spatial high-pass of it: at every gate the local
 * mean radial velocity (within a tunable radius) is subtracted and the result
 * re-centered on the zero-velocity level. This removes the broad-scale flow and
 * highlights small-scale features. It is a radial-velocity high-pass, not true
 * storm motion.
 */
class Level3MeanRelativeVelocityView : public Level3RadialView
{
   Q_OBJECT

public:
   explicit Level3MeanRelativeVelocityView(
      const std::string&                            product,
      std::shared_ptr<manager::RadarProductManager> radarProductManager);
   ~Level3MeanRelativeVelocityView() override;

   Level3MeanRelativeVelocityView(const Level3MeanRelativeVelocityView&) =
      delete;
   Level3MeanRelativeVelocityView(Level3MeanRelativeVelocityView&&) = delete;
   Level3MeanRelativeVelocityView&
   operator=(const Level3MeanRelativeVelocityView&) = delete;
   Level3MeanRelativeVelocityView&
   operator=(Level3MeanRelativeVelocityView&&) = delete;

   static std::shared_ptr<Level3MeanRelativeVelocityView>
   Create(const std::string&                            product,
          std::shared_ptr<manager::RadarProductManager> radarProductManager);

protected:
   [[nodiscard]] std::string SourceProductName() const override;
   [[nodiscard]] bool        MomentTransformDirty() const override;

   bool TransformLevels(
      const std::shared_ptr<wsr88d::rpg::GenericRadialDataPacket>& radialData,
      const std::shared_ptr<wsr88d::rpg::ProductDescriptionBlock>&
                                              descriptionBlock,
      std::vector<std::vector<std::uint8_t>>& outLevels) override;

private:
   std::atomic<bool>  radiusDirty_ {false};
   boost::uuids::uuid radiusCallbackUuid_ {};
   boost::uuids::uuid refreshUuid_ {};
};

} // namespace scwx::qt::view
