//
// An RGB color with AbiWord's "rrggbb" and named-color property syntax.
//
// Derived from AbiWord src/af/util/xp/ut_color.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef UTIL_COLOR_H
#define UTIL_COLOR_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace util {

// A display color; alpha is not a document concept.
struct color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    friend bool operator==(const color &, const color &) = default;
};

//
// Parse a color property.
//
// Parameters:
//      text        - "ff0000", "#ff0000", "red" or "transparent".
//
// Returns:
//      The color, or nothing for "transparent" and unparsable text.
//
std::optional<color> parse_color(std::string_view text);

// Format a color as lowercase "rrggbb".
std::string format_color(color value);

} // namespace util

#endif
