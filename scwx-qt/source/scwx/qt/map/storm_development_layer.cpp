#include <scwx/qt/map/storm_development_layer.hpp>
#include <scwx/qt/gl/draw/placefile_triangles.hpp>
#include <scwx/qt/manager/storm_development_manager.hpp>
#include <scwx/qt/settings/storm_development_settings.hpp>
#include <scwx/common/grid.hpp>
#include <scwx/util/logger.hpp>

#include <algorithm>
#include <array>
#include <cmath>

#include <boost/uuid/uuid.hpp>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::storm_development_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

// Overlay opacity (80%).
static constexpr std::uint8_t kAlpha_ = 204;

namespace
{
struct ColorStop
{
   double        score;
   std::uint8_t  r;
   std::uint8_t  g;
   std::uint8_t  b;
};

constexpr std::array<ColorStop, 5> kColorStops {
   {{0.0, 20, 20, 120},
    {25.0, 40, 140, 220},
    {50.0, 240, 220, 60},
    {75.0, 240, 140, 30},
    {100.0, 200, 20, 20}}};

boost::gil::rgba8_pixel_t LikelihoodColor(double score)
{
   const double clamped = std::clamp(score, 0.0, 100.0);
   for (std::size_t i = 0; i + 1 < kColorStops.size(); ++i)
   {
      const ColorStop& a = kColorStops[i];
      const ColorStop& b = kColorStops[i + 1];
      if (clamped <= b.score)
      {
         const double t = (clamped - a.score) / (b.score - a.score);
         return boost::gil::rgba8_pixel_t {
            static_cast<std::uint8_t>(std::lround(a.r + t * (b.r - a.r))),
            static_cast<std::uint8_t>(std::lround(a.g + t * (b.g - a.g))),
            static_cast<std::uint8_t>(std::lround(a.b + t * (b.b - a.b))),
            kAlpha_};
      }
   }
   const ColorStop& last = kColorStops.back();
   return boost::gil::rgba8_pixel_t {last.r, last.g, last.b, kAlpha_};
}
} // namespace

class StormDevelopmentLayer::Impl
{
public:
   explicit Impl(StormDevelopmentLayer*                self,
                 const std::shared_ptr<gl::GlContext>& glContext) :
       self_ {self},
       triangles_ {std::make_shared<gl::draw::PlacefileTriangles>(glContext)},
       manager_ {manager::StormDevelopmentManager::Instance()}
   {
      triangles_->set_thresholded(false);

      QObject::connect(manager_.get(),
                       &manager::StormDevelopmentManager::GridUpdated,
                       self_,
                       [this]() { RebuildMesh(); },
                       Qt::QueuedConnection);

      // The toolbox "Enabled" checkbox is the master on/off switch: hide the
      // overlay immediately when it is unchecked, show it when re-checked.
      enabledCallbackUuid_ =
         settings::StormDevelopmentSettings::Instance()
            .enabled()
            .RegisterValueChangedCallback([this](const bool&)
                                          { RebuildMesh(); });
   }
   ~Impl()
   {
      settings::StormDevelopmentSettings::Instance()
         .enabled()
         .UnregisterValueChangedCallback(enabledCallbackUuid_);
   }

   void RebuildMesh();

   StormDevelopmentLayer*                            self_;
   std::shared_ptr<gl::draw::PlacefileTriangles>     triangles_;
   std::shared_ptr<manager::StormDevelopmentManager> manager_;
   boost::uuids::uuid                                enabledCallbackUuid_ {};
};

void StormDevelopmentLayer::Impl::RebuildMesh()
{
   const bool enabled =
      settings::StormDevelopmentSettings::Instance().enabled().GetValue();

   // When disabled, produce an empty mesh so nothing is drawn (the overlay
   // must not obscure the radar product underneath).
   auto grid = enabled ? manager_->Grid() : nullptr;

   triangles_->StartTriangles();

   if (grid != nullptr && grid->RowCount() >= 2 && grid->ColumnCount() >= 2)
   {
      auto item = std::make_shared<gr::Placefile::TrianglesDrawItem>();

      const auto vertex =
         [&](std::size_t row, std::size_t column,
             boost::gil::rgba8_pixel_t color)
      {
         const common::Coordinate c = grid->CoordinateAt(row, column);
         gr::Placefile::TrianglesDrawItem::Element element {};
         element.latitude_  = c.latitude_;
         element.longitude_ = c.longitude_;
         element.color_     = color;
         item->elements_.push_back(element);
      };

      for (std::size_t row = 0; row + 1 < grid->RowCount(); ++row)
      {
         for (std::size_t col = 0; col + 1 < grid->ColumnCount(); ++col)
         {
            const double v00 = grid->ValueAt(row, col);
            const double v01 = grid->ValueAt(row, col + 1);
            const double v10 = grid->ValueAt(row + 1, col);
            const double v11 = grid->ValueAt(row + 1, col + 1);
            if (std::isnan(v00) || std::isnan(v01) || std::isnan(v10) ||
                std::isnan(v11))
            {
               continue;
            }

            const auto c00 = LikelihoodColor(v00);
            const auto c01 = LikelihoodColor(v01);
            const auto c10 = LikelihoodColor(v10);
            const auto c11 = LikelihoodColor(v11);

            // Two triangles per cell; GL interpolates the per-vertex colors.
            vertex(row, col, c00);
            vertex(row, col + 1, c01);
            vertex(row + 1, col, c10);

            vertex(row, col + 1, c01);
            vertex(row + 1, col + 1, c11);
            vertex(row + 1, col, c10);
         }
      }

      if (!item->elements_.empty())
      {
         triangles_->AddTriangles(item);
      }

      logger_->debug("Rebuilt mesh: {} vertices", item->elements_.size());
   }

   triangles_->FinishTriangles();
   Q_EMIT self_->NeedsRendering();
}

StormDevelopmentLayer::StormDevelopmentLayer(
   const std::shared_ptr<gl::GlContext>& glContext) :
    DrawLayer(glContext, "StormDevelopmentLayer"),
    p(std::make_unique<Impl>(this, glContext))
{
   AddDrawItem(p->triangles_);
}

StormDevelopmentLayer::~StormDevelopmentLayer() = default;

void StormDevelopmentLayer::Initialize(
   const std::shared_ptr<MapContext>& mapContext)
{
   DrawLayer::Initialize(mapContext);
   p->RebuildMesh();
}

void StormDevelopmentLayer::Render(
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   DrawLayer::Render(mapContext, params);
   SCWX_GL_CHECK_ERROR();
}

void StormDevelopmentLayer::Deinitialize()
{
   DrawLayer::Deinitialize();
}

} // namespace scwx::qt::map
