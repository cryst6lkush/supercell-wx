#pragma once

#include <scwx/qt/view/level3_product_view.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

namespace scwx::wsr88d::rpg
{
class GenericRadialDataPacket;
class ProductDescriptionBlock;
} // namespace scwx::wsr88d::rpg

namespace scwx::qt::view
{

class Level3RadialView : public Level3ProductView
{
   Q_OBJECT

public:
   explicit Level3RadialView(
      const std::string&                            product,
      std::shared_ptr<manager::RadarProductManager> radarProductManager);
   ~Level3RadialView() override;

   Level3RadialView(const Level3RadialView&)            = delete;
   Level3RadialView(Level3RadialView&&)                 = delete;
   Level3RadialView& operator=(const Level3RadialView&) = delete;
   Level3RadialView& operator=(Level3RadialView&&)      = delete;

   [[nodiscard]] std::optional<float> elevation() const override;
   [[nodiscard]] float                range() const override;
   [[nodiscard]] std::chrono::system_clock::time_point
                                           sweep_time() const override;
   [[nodiscard]] std::uint16_t             vcp() const override;
   [[nodiscard]] const std::vector<float>& vertices() const override;

   [[nodiscard]] std::tuple<const void*, std::size_t, std::size_t>
   GetMomentData() const override;

   [[nodiscard]] std::optional<std::uint16_t>
   GetBinLevel(const common::Coordinate& coordinate) const override;

   static std::shared_ptr<Level3RadialView>
   Create(const std::string&                            product,
          std::shared_ptr<manager::RadarProductManager> radarProductManager);

protected:
   boost::asio::thread_pool& thread_pool() override;

   /**
    * @brief Whether a moment transform parameter changed and a recompute is
    * required even though the underlying message is unchanged. Default false.
    */
   [[nodiscard]] virtual bool MomentTransformDirty() const;

   /**
    * @brief Optionally replace the per-radial level bytes used by ComputeSweep.
    * Default returns false (use the packet's own levels). When true, @p outLevels
    * is sized [radials][rangeBins] and read instead of the raw packet.
    */
   virtual bool TransformLevels(
      const std::shared_ptr<wsr88d::rpg::GenericRadialDataPacket>& radialData,
      const std::shared_ptr<wsr88d::rpg::ProductDescriptionBlock>&
                                              descriptionBlock,
      std::vector<std::vector<std::uint8_t>>& outLevels);

protected slots:
   void ComputeSweep() override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::view
