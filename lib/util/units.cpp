//
// Parses and scales document dimensions. The conversion table matches
// AbiWord's UT_convertToInches, with 96 pixels to the inch.
//
// Derived from AbiWord src/af/util/xp/ut_units.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "util/units.h"

#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace util {
namespace {

constexpr double points_per_inch = 72.0;
constexpr double pixels_per_inch = 96.0;

// Return the text following the leading number.
std::string_view number_suffix(std::string_view text) {
    std::size_t index = 0;
    while (index < text.size() &&
           std::isspace(static_cast<unsigned char>(text[index])))
        ++index;
    while (index < text.size() &&
           (std::isdigit(static_cast<unsigned char>(text[index])) ||
            text[index] == '.' || text[index] == '-' ||
            text[index] == '+'))
        ++index;
    while (index < text.size() &&
           std::isspace(static_cast<unsigned char>(text[index])))
        ++index;
    return text.substr(index);
}

} // namespace

dimension parse_dimension_unit(std::string_view text, dimension fallback) {
    static constexpr std::array<std::pair<std::string_view, dimension>, 9>
        names{{{"in", dimension::in},
               {"\"", dimension::in},
               {"cm", dimension::cm},
               {"mm", dimension::mm},
               {"pi", dimension::pi},
               {"pc", dimension::pi},
               {"pt", dimension::pt},
               {"px", dimension::px},
               {"%", dimension::percent}}};
    const std::string_view suffix = number_suffix(text);
    for (const auto &[name, unit] : names)
        if (suffix.starts_with(name))
            return unit;
    return fallback;
}

double leading_number(std::string_view text) {
    const std::string copy(text);
    return std::strtod(copy.c_str(), nullptr);
}

double to_inches(std::string_view text) {
    const double value = leading_number(text);
    switch (parse_dimension_unit(text, dimension::in)) {
    case dimension::cm:
        return value / 2.54;
    case dimension::mm:
        return value / 25.4;
    case dimension::pi:
        return value / 6.0;
    case dimension::pt:
        return value / points_per_inch;
    case dimension::px:
        return value / pixels_per_inch;
    default:
        return value;
    }
}

lu to_lu(std::string_view text) {
    return static_cast<lu>(std::lround(to_inches(text) * layout_resolution));
}

double to_points(std::string_view text) {
    return to_inches(text) * points_per_inch;
}

std::string format_inches(double inches) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.4fin", inches);
    return buffer;
}

std::string format_lu(lu value) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.4gin",
                  static_cast<double>(value) / layout_resolution);
    return buffer;
}

int lu_to_device(lu value, double device_per_inch) {
    return static_cast<int>(std::lround(
        static_cast<double>(value) * device_per_inch / layout_resolution));
}

lu device_to_lu(int value, double device_per_inch) {
    return static_cast<lu>(std::lround(
        static_cast<double>(value) * layout_resolution / device_per_inch));
}

} // namespace util
