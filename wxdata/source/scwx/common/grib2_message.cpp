#include <scwx/common/grib2_message.hpp>
#include <scwx/util/logger.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

#include <openjpeg.h>

namespace scwx
{
namespace common
{
namespace grib2
{

static const std::string logPrefix_ = "scwx::common::grib2_message";
static const auto        logger_    = util::Logger::Create(logPrefix_);

// --- big-endian readers -----------------------------------------------------

static std::uint16_t ReadU16(const std::uint8_t* p)
{
   return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}

static std::uint32_t ReadU32(const std::uint8_t* p)
{
   return (static_cast<std::uint32_t>(p[0]) << 24) |
          (static_cast<std::uint32_t>(p[1]) << 16) |
          (static_cast<std::uint32_t>(p[2]) << 8) |
          static_cast<std::uint32_t>(p[3]);
}

// GRIB2 signed integers are sign-magnitude, not two's complement.
static std::int16_t ReadS16(const std::uint8_t* p)
{
   const std::uint16_t raw = ReadU16(p);
   const std::int16_t   mag = static_cast<std::int16_t>(raw & 0x7FFF);
   return (raw & 0x8000) ? static_cast<std::int16_t>(-mag) : mag;
}

static float ReadF32(const std::uint8_t* p)
{
   const std::uint32_t bits = ReadU32(p);
   float               f {};
   std::memcpy(&f, &bits, sizeof(f));
   return f;
}

// Big-endian bit reader over a byte buffer (MSB first).
namespace
{
class BitReader
{
public:
   BitReader(const std::uint8_t* data, std::size_t byteLength) :
       data_ {data}, bitLength_ {byteLength * 8}
   {
   }

   std::uint64_t Read(unsigned bits)
   {
      std::uint64_t value = 0;
      for (unsigned i = 0; i < bits; ++i)
      {
         value <<= 1;
         if (pos_ < bitLength_)
         {
            value |= (data_[pos_ >> 3] >> (7 - (pos_ & 7))) & 0x1;
            ++pos_;
         }
      }
      return value;
   }

   // Advance to the next byte boundary (GRIB2 complex packing byte-aligns
   // after each of the group reference/width/length arrays).
   void AlignToByte()
   {
      if ((pos_ & 7) != 0)
      {
         pos_ += 8 - (pos_ & 7);
      }
   }

private:
   const std::uint8_t* data_;
   std::size_t         bitLength_;
   std::size_t         pos_ {0};
};
} // namespace

// --- JPEG2000 (template 5.40) via OpenJPEG ----------------------------------
// ponytail: kept for generality; HRRR 2-D fields actually use template 5.3.

namespace
{
struct MemStream
{
   const std::uint8_t* data;
   std::size_t         size;
   std::size_t         pos;
};

OPJ_SIZE_T MemRead(void* buffer, OPJ_SIZE_T count, void* user)
{
   auto* m = static_cast<MemStream*>(user);
   if (m->pos >= m->size)
   {
      return static_cast<OPJ_SIZE_T>(-1);
   }
   const std::size_t take = std::min<std::size_t>(count, m->size - m->pos);
   std::memcpy(buffer, m->data + m->pos, take);
   m->pos += take;
   return take;
}

OPJ_OFF_T MemSkip(OPJ_OFF_T count, void* user)
{
   auto* m = static_cast<MemStream*>(user);
   m->pos += static_cast<std::size_t>(count);
   return count;
}

OPJ_BOOL MemSeek(OPJ_OFF_T pos, void* user)
{
   auto* m = static_cast<MemStream*>(user);
   m->pos  = static_cast<std::size_t>(pos);
   return OPJ_TRUE;
}
} // namespace

static bool DecodeJpeg2000(const std::uint8_t*        data,
                           std::size_t                length,
                           std::size_t                expectedPoints,
                           std::vector<std::int64_t>& out)
{
   MemStream mem {data, length, 0};

   opj_stream_t* stream = opj_stream_default_create(OPJ_TRUE);
   if (stream == nullptr)
   {
      return false;
   }
   opj_stream_set_user_data(stream, &mem, nullptr);
   opj_stream_set_user_data_length(stream, length);
   opj_stream_set_read_function(stream, MemRead);
   opj_stream_set_skip_function(stream, MemSkip);
   opj_stream_set_seek_function(stream, MemSeek);

   opj_codec_t* codec = opj_create_decompress(OPJ_CODEC_J2K);
   if (codec == nullptr)
   {
      opj_stream_destroy(stream);
      return false;
   }

   opj_dparameters_t parameters;
   opj_set_default_decoder_parameters(&parameters);

   opj_image_t* image = nullptr;
   bool         ok    = opj_setup_decoder(codec, &parameters) &&
             opj_read_header(stream, codec, &image) &&
             opj_decode(codec, stream, image) &&
             opj_end_decompress(codec, stream);

   if (ok && image != nullptr && image->numcomps >= 1)
   {
      const opj_image_comp_t& comp = image->comps[0];
      const std::size_t       n    = static_cast<std::size_t>(comp.w) * comp.h;
      if (n == expectedPoints && comp.data != nullptr)
      {
         out.resize(n);
         for (std::size_t i = 0; i < n; ++i)
         {
            out[i] = comp.data[i];
         }
      }
      else
      {
         ok = false;
      }
   }
   else
   {
      ok = false;
   }

   if (image != nullptr)
   {
      opj_image_destroy(image);
   }
   opj_destroy_codec(codec);
   opj_stream_destroy(stream);
   return ok;
}

// --- simple packing (template 5.0) ------------------------------------------

static void UnpackSimple(const std::uint8_t*        data,
                         std::size_t                length,
                         std::size_t                numberOfPoints,
                         std::uint8_t               bitsPerValue,
                         std::vector<std::int64_t>& out)
{
   out.assign(numberOfPoints, 0);
   if (bitsPerValue == 0)
   {
      return; // constant field; all X == 0
   }

   BitReader reader {data, length};
   for (std::size_t i = 0; i < numberOfPoints; ++i)
   {
      out[i] = static_cast<std::int64_t>(reader.Read(bitsPerValue));
   }
}

// --- complex packing +/- spatial differencing (templates 5.2 / 5.3) ---------

struct ComplexParams
{
   std::uint8_t  nBitsGroupRef {};
   std::uint32_t numberOfGroups {};
   std::uint8_t  groupWidthRef {};
   std::uint8_t  nBitsGroupWidth {};
   std::uint32_t groupLengthRef {};
   std::uint8_t  groupLengthInc {};
   std::uint32_t lastGroupLength {};
   std::uint8_t  nBitsGroupLength {};
   std::uint8_t  spatialOrder {}; // 0 for 5.2
   std::uint8_t  extraBytes {};   // 0 for 5.2
};

static bool UnpackComplex(const std::uint8_t*        data,
                          std::size_t                length,
                          std::size_t                numberOfPoints,
                          const ComplexParams&       cp,
                          std::vector<std::int64_t>& out)
{
   BitReader reader {data, length};

   // Spatial differencing extra descriptors (template 5.3), byte-multiples.
   std::int64_t ival1 = 0;
   std::int64_t ival2 = 0;
   std::int64_t minsd = 0;
   if (cp.spatialOrder != 0)
   {
      const unsigned nb = static_cast<unsigned>(cp.extraBytes) * 8;
      if (nb == 0)
      {
         return false;
      }
      ival1 = static_cast<std::int64_t>(reader.Read(nb));
      if (cp.spatialOrder == 2)
      {
         ival2 = static_cast<std::int64_t>(reader.Read(nb));
      }
      const std::uint64_t sign = reader.Read(1);
      const std::int64_t  mag  = static_cast<std::int64_t>(reader.Read(nb - 1));
      minsd                    = (sign != 0) ? -mag : mag;
   }

   const std::uint32_t ng = cp.numberOfGroups;

   std::vector<std::int64_t> groupRef(ng);
   for (std::uint32_t g = 0; g < ng; ++g)
   {
      groupRef[g] = static_cast<std::int64_t>(reader.Read(cp.nBitsGroupRef));
   }
   reader.AlignToByte();

   std::vector<std::int64_t> groupWidth(ng);
   for (std::uint32_t g = 0; g < ng; ++g)
   {
      groupWidth[g] =
         static_cast<std::int64_t>(reader.Read(cp.nBitsGroupWidth)) +
         cp.groupWidthRef;
   }
   reader.AlignToByte();

   std::vector<std::int64_t> groupLength(ng);
   for (std::uint32_t g = 0; g < ng; ++g)
   {
      groupLength[g] =
         static_cast<std::int64_t>(reader.Read(cp.nBitsGroupLength)) *
            cp.groupLengthInc +
         cp.groupLengthRef;
   }
   reader.AlignToByte();
   if (ng > 0)
   {
      groupLength[ng - 1] = cp.lastGroupLength;
   }

   out.clear();
   out.reserve(numberOfPoints);
   for (std::uint32_t g = 0; g < ng; ++g)
   {
      const std::int64_t width = groupWidth[g];
      const std::int64_t len   = groupLength[g];
      if (width == 0)
      {
         for (std::int64_t k = 0; k < len; ++k)
         {
            out.push_back(groupRef[g]);
         }
      }
      else
      {
         for (std::int64_t k = 0; k < len; ++k)
         {
            out.push_back(
               static_cast<std::int64_t>(reader.Read(
                  static_cast<unsigned>(width))) +
               groupRef[g]);
         }
      }
   }

   if (out.size() != numberOfPoints)
   {
      logger_->warn("Complex unpack produced {} values (expected {})",
                    out.size(),
                    numberOfPoints);
      return false;
   }

   // Undo spatial differencing.
   if (cp.spatialOrder == 1)
   {
      out[0] = ival1;
      for (std::size_t n = 1; n < numberOfPoints; ++n)
      {
         out[n] += minsd;
         out[n] += out[n - 1];
      }
   }
   else if (cp.spatialOrder == 2)
   {
      out[0] = ival1;
      if (numberOfPoints > 1)
      {
         out[1] = ival2;
      }
      for (std::size_t n = 2; n < numberOfPoints; ++n)
      {
         out[n] += minsd;
         out[n] += 2 * out[n - 1] - out[n - 2];
      }
   }

   return true;
}

// --- message walk -----------------------------------------------------------

std::optional<DecodedField> DecodeMessage(const std::uint8_t* data,
                                          std::size_t         length)
{
   if (data == nullptr || length < 16 || std::memcmp(data, "GRIB", 4) != 0 ||
       data[7] != 2)
   {
      return std::nullopt;
   }

   std::size_t numberOfPoints = 0;

   std::uint16_t drTemplate   = 0;
   float         refValue     = 0.0f;
   std::int16_t  binaryScale  = 0;
   std::int16_t  decimalScale = 0;
   std::uint8_t  bitsPerValue = 0;
   ComplexParams complex {};
   bool          haveDrs = false;

   const std::uint8_t* section7      = nullptr;
   std::size_t         section7Bytes = 0;

   std::size_t offset = 16; // past section 0
   while (offset + 5 <= length)
   {
      if (std::memcmp(data + offset, "7777", 4) == 0)
      {
         break;
      }

      const std::uint32_t sectionLength = ReadU32(data + offset);
      const std::uint8_t  sectionNumber = data[offset + 4];
      if (sectionLength < 5 || offset + sectionLength > length)
      {
         return std::nullopt;
      }
      const std::uint8_t* s = data + offset;

      switch (sectionNumber)
      {
      case 3: // Grid Definition
         numberOfPoints = ReadU32(s + 6);
         break;

      case 5: // Data Representation
         numberOfPoints = ReadU32(s + 5);
         drTemplate     = ReadU16(s + 9);
         if (sectionLength >= 21)
         {
            refValue     = ReadF32(s + 11);
            binaryScale  = ReadS16(s + 15);
            decimalScale = ReadS16(s + 17);
            bitsPerValue = s[19];
            haveDrs      = true;
         }
         if ((drTemplate == 2 || drTemplate == 3) && sectionLength >= 47)
         {
            complex.nBitsGroupRef    = s[19];
            complex.numberOfGroups   = ReadU32(s + 31);
            complex.groupWidthRef    = s[35];
            complex.nBitsGroupWidth  = s[36];
            complex.groupLengthRef   = ReadU32(s + 37);
            complex.groupLengthInc   = s[41];
            complex.lastGroupLength  = ReadU32(s + 42);
            complex.nBitsGroupLength = s[46];
            if (drTemplate == 3 && sectionLength >= 49)
            {
               complex.spatialOrder = s[47];
               complex.extraBytes   = s[48];
            }
         }
         break;

      case 6: // Bitmap
         if (s[5] != 255)
         {
            logger_->warn("Bitmapped GRIB2 fields are not supported");
            return std::nullopt;
         }
         break;

      case 7: // Data
         section7      = s + 5;
         section7Bytes = sectionLength - 5;
         break;

      default:
         break;
      }

      offset += sectionLength;
   }

   if (!haveDrs || section7 == nullptr || numberOfPoints == 0)
   {
      return std::nullopt;
   }

   std::vector<std::int64_t> packed;
   if (drTemplate == 0)
   {
      UnpackSimple(
         section7, section7Bytes, numberOfPoints, bitsPerValue, packed);
   }
   else if (drTemplate == 2 || drTemplate == 3)
   {
      if (!UnpackComplex(
             section7, section7Bytes, numberOfPoints, complex, packed))
      {
         return std::nullopt;
      }
   }
   else if (drTemplate == 40)
   {
      if (!DecodeJpeg2000(section7, section7Bytes, numberOfPoints, packed))
      {
         logger_->warn("JPEG2000 decode failed");
         return std::nullopt;
      }
   }
   else
   {
      logger_->warn("Unsupported data representation template 5.{}",
                    drTemplate);
      return std::nullopt;
   }

   // Y = (R + X * 2^E) * 10^-D
   const double twoE  = std::pow(2.0, binaryScale);
   const double tenND = std::pow(10.0, -decimalScale);

   DecodedField field {};
   field.numberOfPoints_ = numberOfPoints;
   field.values_.resize(numberOfPoints);
   for (std::size_t i = 0; i < numberOfPoints; ++i)
   {
      field.values_[i] = (static_cast<double>(refValue) +
                          static_cast<double>(packed[i]) * twoE) *
                         tenND;
   }

   return field;
}

} // namespace grib2
} // namespace common
} // namespace scwx
