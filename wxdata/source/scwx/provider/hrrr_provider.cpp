#include <scwx/provider/hrrr_provider.hpp>
#include <scwx/common/grib2_message.hpp>
#include <scwx/common/lambert_conformal.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/logger.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <optional>
#include <sstream>
#include <string>

#include <boost/algorithm/string.hpp>
#include <cpr/cpr.h>
#include <fmt/chrono.h>
#include <fmt/format.h>

namespace scwx::provider
{

static const std::string logPrefix_ = "scwx::provider::hrrr_provider";
static const auto        logger_    = util::Logger::Create(logPrefix_);

static const std::string kBaseUrl_ =
   "https://noaa-hrrr-bdp-pds.s3.amazonaws.com";

// GRIB2 .idx record: msg:startByte:d=YYYYMMDDHH:VAR:LEVEL:FCST:...
struct IdxRecord
{
   std::size_t startByte_;
   std::string var_;
   std::string level_;
};

// A field we want, identified by its .idx var + level strings.
struct FieldSpec
{
   std::string var_;
   std::string level_;
};

class HrrrProvider::Impl
{
public:
   explicit Impl() = default;
   ~Impl() { running_ = false; }
   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   std::optional<std::string> Fetch(const std::string&            url,
                                    const std::optional<std::string>& range);
   std::vector<IdxRecord>     ParseIdx(const std::string& idxText);
   std::optional<std::vector<double>>
   FetchField(const std::string&            gribUrl,
              const std::vector<IdxRecord>& records,
              const FieldSpec&              spec);

   std::atomic<bool> running_ {true};
};

HrrrProvider::HrrrProvider() : p(std::make_unique<Impl>()) {}
HrrrProvider::~HrrrProvider() = default;

HrrrProvider::HrrrProvider(HrrrProvider&&) noexcept            = default;
HrrrProvider& HrrrProvider::operator=(HrrrProvider&&) noexcept = default;

std::optional<std::string>
HrrrProvider::Impl::Fetch(const std::string&                url,
                          const std::optional<std::string>& range)
{
   cpr::Header header = network::cpr::GetHeader();
   if (range.has_value())
   {
      header["Range"] = "bytes=" + *range;
   }

   auto response = cpr::Get(cpr::Url {url},
                            header,
                            network::cpr::GetDefaultTimeout(),
                            network::cpr::GetDefaultConnectTimeout(),
                            network::cpr::GetDefaultLowSpeed(),
                            network::cpr::GetDefaultProgressCallback(running_));

   if (cpr::status::is_success(response.status_code))
   {
      return std::move(response.text);
   }
   return std::nullopt;
}

std::vector<IdxRecord> HrrrProvider::Impl::ParseIdx(const std::string& idxText)
{
   std::vector<IdxRecord> records;
   std::istringstream      stream {idxText};
   std::string             line;

   while (std::getline(stream, line))
   {
      if (line.empty())
      {
         continue;
      }
      std::vector<std::string> fields;
      boost::split(fields, line, boost::is_any_of(":"));
      if (fields.size() < 5)
      {
         continue;
      }
      try
      {
         records.push_back(IdxRecord {
            static_cast<std::size_t>(std::stoull(fields[1])),
            fields[3],
            fields[4]});
      }
      catch (const std::exception&)
      {
         // skip malformed line
      }
   }
   return records;
}

std::optional<std::vector<double>>
HrrrProvider::Impl::FetchField(const std::string&            gribUrl,
                               const std::vector<IdxRecord>& records,
                               const FieldSpec&              spec)
{
   for (std::size_t i = 0; i < records.size(); ++i)
   {
      if (records[i].var_ != spec.var_ || records[i].level_ != spec.level_)
      {
         continue;
      }

      const std::size_t start = records[i].startByte_;
      std::string       range = std::to_string(start) + "-";
      if (i + 1 < records.size())
      {
         range = std::to_string(start) + "-" +
                 std::to_string(records[i + 1].startByte_ - 1);
      }

      auto messageBytes = Fetch(gribUrl, range);
      if (!messageBytes.has_value())
      {
         logger_->warn("Failed to fetch {} {}", spec.var_, spec.level_);
         return std::nullopt;
      }

      auto decoded = common::grib2::DecodeMessage(
         reinterpret_cast<const std::uint8_t*>(messageBytes->data()),
         messageBytes->size());
      if (!decoded.has_value())
      {
         logger_->warn("Failed to decode {} {}", spec.var_, spec.level_);
         return std::nullopt;
      }
      return std::move(decoded->values_);
   }

   logger_->warn("Field not found in index: {} {}", spec.var_, spec.level_);
   return std::nullopt;
}

boost::outcome_v2::result<std::vector<common::StormDevelopmentInputs>>
HrrrProvider::FetchInputs(const std::vector<common::Coordinate>& points)
{
   if (points.empty())
   {
      return std::vector<common::StormDevelopmentInputs> {};
   }

   // HRRR analysis (f00) posts ~1-2 h after the cycle; try recent runs back.
   const auto now = std::chrono::system_clock::now();
   for (int hoursBack = 2; hoursBack <= 5; ++hoursBack)
   {
      const auto     runTime = now - std::chrono::hours(hoursBack);
      const std::time_t tt    = std::chrono::system_clock::to_time_t(runTime);
      const std::string date  = fmt::format("{:%Y%m%d}", fmt::gmtime(tt));
      const std::string hour  = fmt::format("{:%H}", fmt::gmtime(tt));

      const std::string gribUrl =
         fmt::format("{}/hrrr.{}/conus/hrrr.t{}z.wrfsfcf00.grib2",
                     kBaseUrl_,
                     date,
                     hour);
      const std::string idxUrl = gribUrl + ".idx";

      logger_->debug("Trying HRRR run {} {}z", date, hour);

      auto idxText = p->Fetch(idxUrl, std::nullopt);
      if (!idxText.has_value())
      {
         continue; // run not posted yet, try an earlier one
      }

      const auto records = p->ParseIdx(*idxText);

      auto cape = p->FetchField(gribUrl, records, {"CAPE", "surface"});
      auto cin  = p->FetchField(gribUrl, records, {"CIN", "surface"});
      auto rh = p->FetchField(gribUrl, records, {"RH", "2 m above ground"});
      auto uwind =
         p->FetchField(gribUrl, records, {"UGRD", "10 m above ground"});
      auto vwind =
         p->FetchField(gribUrl, records, {"VGRD", "10 m above ground"});

      if (!cape.has_value() || !cin.has_value())
      {
         logger_->warn("HRRR run {} {}z missing CAPE/CIN", date, hour);
         return boost::system::errc::make_error_code(
            boost::system::errc::bad_message);
      }

      const common::LambertConformalGrid grid {};
      const std::size_t                  gridPoints = grid.nx_ * grid.ny_;
      if (cape->size() != gridPoints)
      {
         logger_->warn("Unexpected HRRR grid size {} (expected {})",
                       cape->size(),
                       gridPoints);
         return boost::system::errc::make_error_code(
            boost::system::errc::bad_message);
      }

      std::vector<common::StormDevelopmentInputs> results;
      results.reserve(points.size());

      for (const auto& point : points)
      {
         const auto index = grid.ToGridIndex(point.latitude_, point.longitude_);
         const long i     = std::lround(index.i);
         const long j     = std::lround(index.j);

         common::StormDevelopmentInputs inputs {};
         if (i >= 0 && static_cast<std::size_t>(i) < grid.nx_ && j >= 0 &&
             static_cast<std::size_t>(j) < grid.ny_)
         {
            const std::size_t k =
               static_cast<std::size_t>(j) * grid.nx_ + static_cast<std::size_t>(i);
            inputs.capeJPerKg_                 = (*cape)[k];
            inputs.convectiveInhibitionJPerKg_ = (*cin)[k];
            if (rh.has_value() && rh->size() == gridPoints)
            {
               inputs.relativeHumidity2mPercent_ = (*rh)[k];
            }
            if (uwind.has_value() && vwind.has_value() &&
                uwind->size() == gridPoints && vwind->size() == gridPoints)
            {
               const double u        = (*uwind)[k];
               const double v        = (*vwind)[k];
               inputs.windSpeed10mMs_ = std::sqrt(u * u + v * v);
            }
         }
         results.push_back(inputs);
      }

      logger_->debug("HRRR run {} {}z: sampled {} points",
                     date,
                     hour,
                     results.size());
      return results;
   }

   logger_->warn("No recent HRRR run available");
   return boost::system::errc::make_error_code(
      boost::system::errc::no_message);
}

void HrrrProvider::Shutdown() noexcept
{
   p->running_ = false;
}

} // namespace scwx::provider
