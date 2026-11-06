//
// Measurement units, ported from AbiWord's ut_units. The layout engine
// works in integer layout units of 1/1440 inch, so a document measures
// the same at every zoom and on every display.
//
// Derived from AbiWord src/af/util/xp/ut_units.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef UTIL_UNITS_H
#define UTIL_UNITS_H

#include <cstdint>
#include <string>
#include <string_view>

namespace util {

// A length in layout units.
using lu = std::int32_t;

// Layout units per inch.
inline constexpr lu layout_resolution = 1440;

// A unit a document can express a dimension in.
enum class dimension { none, in, cm, mm, pi, pt, px, percent };

//
// Return the unit named by the suffix of a dimension string.
//
// Parameters:
//      text        - Value such as "1.5in" or "12pt".
//      fallback    - Result when no known unit is present.
//
dimension parse_dimension_unit(std::string_view text,
                               dimension fallback = dimension::none);

//
// Convert a dimension string to inches.
//
// Parameters:
//      text        - Value such as "2.54cm". A bare number is inches.
//
// Returns:
//      The value in inches, or 0 for text without a number.
//
double to_inches(std::string_view text);

//
// Convert a dimension string to layout units.
//
// Sample call:
//      util::lu margin = util::to_lu("1in");   // 1440
//
lu to_lu(std::string_view text);

// Convert a dimension string to points.
double to_points(std::string_view text);

// Return the numeric prefix of a string, or 0 without one.
double leading_number(std::string_view text);

// Format inches with four decimals, e.g. "1.2500in".
std::string format_inches(double inches);

// Format layout units as a compact inch dimension, e.g. "0.5in".
std::string format_lu(lu value);

// Scale layout units to device pixels at a resolution, rounding.
int lu_to_device(lu value, double device_per_inch);

// Scale device pixels to layout units at a resolution, rounding.
lu device_to_lu(int value, double device_per_inch);

} // namespace util

#endif
