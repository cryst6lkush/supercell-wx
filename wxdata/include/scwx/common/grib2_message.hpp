#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace scwx
{
namespace common
{
namespace grib2
{

/**
 * @brief A single decoded GRIB2 field: unpacked values in grid-scan order
 * (i fastest), plus the grid point count from the Grid Definition Section.
 */
struct DecodedField
{
   std::size_t         numberOfPoints_ {};
   std::vector<double> values_ {};
};

/**
 * @brief Decode one complete GRIB2 message (Section 0 "GRIB" through "7777").
 *
 * Supports data-representation templates 5.0 (simple packing) and 5.40
 * (JPEG2000), which cover the HRRR 2-D surface fields, and only the "no
 * bitmap" case. Returns std::nullopt on any unsupported/ malformed input.
 */
std::optional<DecodedField> DecodeMessage(const std::uint8_t* data,
                                          std::size_t         length);

} // namespace grib2
} // namespace common
} // namespace scwx
