#include <scwx/qt/view/level3_mean_relative_velocity_view.hpp>
#include <scwx/qt/settings/product_settings.hpp>
#include <scwx/common/products.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/wsr88d/rpg/generic_radial_data_packet.hpp>
#include <scwx/wsr88d/rpg/product_description_block.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>

#include <boost/uuid/random_generator.hpp>

namespace scwx::qt::view
{

static const std::string logPrefix_ =
   "scwx::qt::view::level3_mean_relative_velocity_view";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

// The base velocity feed to read. N0G (super-resolution base velocity) is the
// app's default Velocity product and what actually flows on the AWS Level 3
// feed for most sites; legacy N0U is frequently absent.
// ponytail: hardcoded N0G; resolve the site's available velocity product
// (preferring N0G, then N0U) if a site lacks N0G.
static const std::string kSourceProduct_ {"N0G"};

static constexpr std::uint8_t RANGE_FOLDED      = 1u;
static constexpr std::uint8_t kFallbackZeroByte = 128u;

// Window-size clamps and the azimuth conversion constant.
static constexpr int   kMinHalfWidth        = 1;
static constexpr int   kMaxGateHalfWidth    = 600;
static constexpr int   kMaxRadialHalfWidth  = 120;
// ponytail: range-independent azimuth half-width (radials per km) — a small
// fixed constant, not a true physical width. Make it range-dependent if the
// azimuthal mean looks too wide near the radar or too narrow far out.
static constexpr double kRadialsPerKm = 1.0;

Level3MeanRelativeVelocityView::Level3MeanRelativeVelocityView(
   const std::string&                            product,
   std::shared_ptr<manager::RadarProductManager> radarProductManager) :
    Level3RadialView(product, radarProductManager),
    refreshUuid_ {boost::uuids::random_generator()()}
{
   // Poll the base velocity feed (not the selected SRM feed) for live updates
   auto manager = radar_product_manager();
   if (manager != nullptr)
   {
      manager->EnableRefresh(
         common::RadarProductGroup::Level3, kSourceProduct_, true, refreshUuid_);
   }

   // Recompute when the mean radius is changed (staged from the settings panel)
   radiusCallbackUuid_ =
      settings::ProductSettings::Instance()
         .srv_mean_radius_km()
         .RegisterValueStagedCallback(
            [this](const double&)
            {
               radiusDirty_.store(true, std::memory_order_relaxed);
               Update();
            });
}

Level3MeanRelativeVelocityView::~Level3MeanRelativeVelocityView()
{
   // Stop new recomputes before tearing down
   settings::ProductSettings::Instance()
      .srv_mean_radius_km()
      .UnregisterValueStagedCallback(radiusCallbackUuid_);

   auto manager = radar_product_manager();
   if (manager != nullptr)
   {
      manager->EnableRefresh(
         common::RadarProductGroup::Level3, kSourceProduct_, false, refreshUuid_);
   }

   // Wait for any in-flight ComputeSweep (which may be in TransformLevels and
   // access members of this object) to finish before they are destroyed.
   std::unique_lock sweepLock {sweep_mutex()};
}

std::string Level3MeanRelativeVelocityView::SourceProductName() const
{
   return kSourceProduct_;
}

bool Level3MeanRelativeVelocityView::MomentTransformDirty() const
{
   return radiusDirty_.load(std::memory_order_relaxed);
}

bool Level3MeanRelativeVelocityView::TransformLevels(
   const std::shared_ptr<wsr88d::rpg::GenericRadialDataPacket>& radialData,
   const std::shared_ptr<wsr88d::rpg::ProductDescriptionBlock>& descriptionBlock,
   std::vector<std::vector<std::uint8_t>>&                      outLevels)
{
   if (radialData == nullptr || descriptionBlock == nullptr)
   {
      return false;
   }

   const std::uint16_t radials = radialData->number_of_radials();
   const std::uint16_t gates   = radialData->number_of_range_bins();
   if (radials == 0 || gates == 0)
   {
      return false;
   }

   // Read the tunable radius and clear the dirty flag
   const double radiusKm = settings::ProductSettings::Instance()
                              .srv_mean_radius_km()
                              .GetStagedOrValue();
   radiusDirty_.store(false, std::memory_order_relaxed);

   const std::uint16_t threshold = descriptionBlock->threshold();

   // Zero-velocity byte to re-center on. descriptionBlock->offset() is a physical
   // offset for velocity (outside [2, 255]), so the fallback mid-scale value is
   // used in practice; honored here for products that encode it as a byte.
   const float        offsetF = descriptionBlock->offset();
   const std::uint8_t offsetByte =
      (offsetF >= 2.0f && offsetF <= 255.0f) ?
         static_cast<std::uint8_t>(std::lround(offsetF)) :
         kFallbackZeroByte;

   // Convert radius (km) to window half-widths
   const float gateLengthMeters =
      std::max(1.0f, static_cast<float>(descriptionBlock->x_resolution_raw()));
   const int gateHalfWidth = std::clamp(
      static_cast<int>(std::lround(radiusKm * 1000.0 / gateLengthMeters)),
      kMinHalfWidth,
      kMaxGateHalfWidth);
   const int radialHalfWidth =
      std::clamp(static_cast<int>(std::lround(radiusKm * kRadialsPerKm)),
                 kMinHalfWidth,
                 kMaxRadialHalfWidth);

   logger_->trace("Relative SRV: radius={} km, gateHalfWidth={}, "
                  "radialHalfWidth={}, offsetByte={}",
                  radiusKm,
                  gateHalfWidth,
                  radialHalfWidth,
                  static_cast<unsigned>(offsetByte));

   const std::size_t count = static_cast<std::size_t>(radials) * gates;

   // Decode raw bytes to a float grid; NaN marks no-data / range-folded
   std::vector<float> raw(count);
   for (std::uint16_t r = 0; r < radials; ++r)
   {
      const auto&       level = radialData->level(r);
      const std::size_t base  = static_cast<std::size_t>(r) * gates;
      for (std::uint16_t g = 0; g < gates; ++g)
      {
         const std::uint8_t b = (g < level.size()) ? level[g] : 0u;
         raw[base + g]        = (b < threshold || b == RANGE_FOLDED) ?
                                   std::numeric_limits<float>::quiet_NaN() :
                                   static_cast<float>(b);
      }
   }

   // Separable local mean. First: sliding-window sum/count along gates (range).
   std::vector<float> rowSum(count, 0.0f);
   std::vector<int>   rowCnt(count, 0);
   for (std::uint16_t r = 0; r < radials; ++r)
   {
      const std::size_t base = static_cast<std::size_t>(r) * gates;

      // Initialize window [0, gateHalfWidth] (the window for gate 0)
      float sum = 0.0f;
      int   cnt = 0;
      for (int g = 0; g <= gateHalfWidth && g < gates; ++g)
      {
         const float v = raw[base + g];
         if (!std::isnan(v))
         {
            sum += v;
            ++cnt;
         }
      }

      for (int g = 0; g < gates; ++g)
      {
         rowSum[base + g] = sum;
         rowCnt[base + g] = cnt;

         // Slide to gate g+1: drop (g - gateHalfWidth), add (g + gateHalfWidth+1)
         const int outIdx = g - gateHalfWidth;
         const int inIdx  = g + gateHalfWidth + 1;
         if (outIdx >= 0)
         {
            const float vo = raw[base + outIdx];
            if (!std::isnan(vo))
            {
               sum -= vo;
               --cnt;
            }
         }
         if (inIdx < gates)
         {
            const float vi = raw[base + inIdx];
            if (!std::isnan(vi))
            {
               sum += vi;
               ++cnt;
            }
         }
      }
   }

   // Second: sum the per-radial windows over +/- radialHalfWidth radials
   // (wrapping in azimuth), divide for the local mean, then encode the high-pass.
   // ponytail: O(radials * gates * radialHalfWidth); make the azimuth pass a
   // sliding window too if this shows up in sweep timings.
   outLevels.resize(radials);
   for (std::uint16_t r = 0; r < radials; ++r)
   {
      const auto&       level = radialData->level(r);
      const std::size_t base  = static_cast<std::size_t>(r) * gates;

      std::vector<std::uint8_t>& out = outLevels[r];
      out.resize(gates);

      for (std::uint16_t g = 0; g < gates; ++g)
      {
         const std::uint8_t origByte = (g < level.size()) ? level[g] : 0u;
         const float        rawVal   = raw[base + g];

         // Preserve no-data and range-folded bytes unchanged
         if (std::isnan(rawVal))
         {
            out[g] = origByte;
            continue;
         }

         float meanSum = 0.0f;
         int   meanCnt = 0;
         for (int dr = -radialHalfWidth; dr <= radialHalfWidth; ++dr)
         {
            int rr = static_cast<int>(r) + dr;
            rr %= radials;
            if (rr < 0)
            {
               rr += radials;
            }
            const std::size_t idx = static_cast<std::size_t>(rr) * gates + g;
            meanSum += rowSum[idx];
            meanCnt += rowCnt[idx];
         }

         if (meanCnt <= 0)
         {
            out[g] = origByte;
            continue;
         }

         const float mean = meanSum / static_cast<float>(meanCnt);
         const long  encoded =
            std::lround(rawVal - mean + static_cast<float>(offsetByte));
         out[g] = static_cast<std::uint8_t>(std::clamp<long>(encoded, 2, 255));
      }
   }

   return true;
}

std::shared_ptr<Level3MeanRelativeVelocityView>
Level3MeanRelativeVelocityView::Create(
   const std::string&                            product,
   std::shared_ptr<manager::RadarProductManager> radarProductManager)
{
   return std::make_shared<Level3MeanRelativeVelocityView>(product,
                                                           radarProductManager);
}

} // namespace scwx::qt::view
