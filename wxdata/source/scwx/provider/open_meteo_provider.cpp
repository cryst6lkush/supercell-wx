#include <scwx/provider/open_meteo_provider.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/json.hpp>
#include <scwx/util/logger.hpp>

#include <algorithm>
#include <atomic>

#include <boost/json.hpp>
#include <cpr/cpr.h>
#include <fmt/format.h>

namespace scwx::provider
{

static const std::string logPrefix_ = "scwx::provider::open_meteo_provider";
static const auto        logger_    = util::Logger::Create(logPrefix_);

static const std::string kBaseUrl_ = "https://api.open-meteo.com/v1/forecast";
static const std::string kCurrentParams_ =
   "cape,convective_inhibition,relative_humidity_2m,surface_pressure,"
   "pressure_msl,wind_speed_10m,dew_point_2m";

// Open-Meteo has no documented hard cap on points per request, but batching
// keeps individual requests small and responsive.
static constexpr std::size_t kMaxPointsPerRequest = 140;

class OpenMeteoProvider::Impl
{
public:
   explicit Impl() = default;
   ~Impl() { running_ = false; }
   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   cpr::Header       header_ {network::cpr::GetHeader()};
   std::atomic<bool> running_ {true};
};

OpenMeteoProvider::OpenMeteoProvider() : p(std::make_unique<Impl>()) {}
OpenMeteoProvider::~OpenMeteoProvider() = default;

OpenMeteoProvider::OpenMeteoProvider(OpenMeteoProvider&&) noexcept = default;
OpenMeteoProvider&
OpenMeteoProvider::operator=(OpenMeteoProvider&&) noexcept = default;

boost::outcome_v2::result<std::vector<types::open_meteo::PointResponse>>
OpenMeteoProvider::FetchGrid(const std::vector<common::Coordinate>& points)
{
   if (points.empty())
   {
      return std::vector<types::open_meteo::PointResponse> {};
   }

   logger_->debug("FetchGrid: {} points", points.size());

   std::vector<cpr::AsyncResponse> asyncResponses {};

   for (std::size_t offset = 0; offset < points.size();
        offset += kMaxPointsPerRequest)
   {
      const std::size_t chunkSize =
         std::min(kMaxPointsPerRequest, points.size() - offset);

      std::string latitudeCsv {};
      std::string longitudeCsv {};

      for (std::size_t i = 0; i < chunkSize; ++i)
      {
         if (i > 0)
         {
            latitudeCsv += ',';
            longitudeCsv += ',';
         }

         const common::Coordinate& point = points[offset + i];
         latitudeCsv += fmt::format("{}", point.latitude_);
         longitudeCsv += fmt::format("{}", point.longitude_);
      }

      auto parameters =
         cpr::Parameters {{"latitude", latitudeCsv},
                          {"longitude", longitudeCsv},
                          {"current", kCurrentParams_},
                          {"wind_speed_unit", "ms"},
                          {"timezone", "UTC"}};

      asyncResponses.emplace_back(
         cpr::GetAsync(cpr::Url {kBaseUrl_},
                       p->header_,
                       parameters,
                       network::cpr::GetDefaultTimeout(),
                       network::cpr::GetDefaultConnectTimeout(),
                       network::cpr::GetDefaultLowSpeed(),
                       network::cpr::GetDefaultProgressCallback(p->running_)));
   }

   std::vector<types::open_meteo::PointResponse> result {};
   result.reserve(points.size());

   for (auto& asyncResponse : asyncResponses)
   {
      auto response = asyncResponse.get();

      if (response.status_code != cpr::status::HTTP_OK)
      {
         logger_->warn("Open-Meteo request failed: {}", response.status_code);
         return boost::system::errc::make_error_code(
            boost::system::errc::no_message);
      }

      const boost::json::value json = util::json::ReadJsonString(response.text);

      try
      {
         if (json.is_array())
         {
            auto chunkResult = boost::json::value_to<
               std::vector<types::open_meteo::PointResponse>>(json);
            result.insert(result.end(),
                          std::make_move_iterator(chunkResult.begin()),
                          std::make_move_iterator(chunkResult.end()));
         }
         else if (json.is_object())
         {
            // A single-coordinate request returns a bare object
            result.push_back(
               boost::json::value_to<types::open_meteo::PointResponse>(json));
         }
         else
         {
            logger_->warn("Unexpected Open-Meteo response shape");
            return boost::system::errc::make_error_code(
               boost::system::errc::bad_message);
         }
      }
      catch (const std::exception& ex)
      {
         logger_->warn("Error parsing Open-Meteo response: {}", ex.what());
         return boost::system::errc::make_error_code(
            boost::system::errc::bad_message);
      }
   }

   return result;
}

void OpenMeteoProvider::Shutdown() noexcept
{
   p->running_ = false;
}

} // namespace scwx::provider
