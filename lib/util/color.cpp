//
// Parses color properties. Only the CSS color names documents actually
// use are recognized; hex is the canonical form written by AbiWord.
//
// Derived from AbiWord src/af/util/xp/ut_color.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "util/color.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <utility>

namespace util {
namespace {

// Return a hex digit's value, or -1.
int hex_value(char digit) {
    if (digit >= '0' && digit <= '9') return digit - '0';
    if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
    if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
    return -1;
}

} // namespace

std::optional<color> parse_color(std::string_view text) {
    static constexpr std::array<std::pair<std::string_view, color>, 10>
        names{{{"black", {0, 0, 0}},
               {"white", {255, 255, 255}},
               {"red", {255, 0, 0}},
               {"green", {0, 128, 0}},
               {"blue", {0, 0, 255}},
               {"yellow", {255, 255, 0}},
               {"gray", {128, 128, 128}},
               {"grey", {128, 128, 128}},
               {"orange", {255, 165, 0}},
               {"purple", {128, 0, 128}}}};
    if (text.empty() || text == "transparent")
        return std::nullopt;
    for (const auto &[name, value] : names)
        if (text == name)
            return value;
    if (text.front() == '#')
        text.remove_prefix(1);
    if (text.size() != 6)
        return std::nullopt;
    color result;
    std::uint8_t *channels[] = {&result.r, &result.g, &result.b};
    for (int index = 0; index < 3; ++index) {
        const int high = hex_value(text[index * 2]);
        const int low = hex_value(text[index * 2 + 1]);
        if (high < 0 || low < 0)
            return std::nullopt;
        *channels[index] = static_cast<std::uint8_t>(high * 16 + low);
    }
    return result;
}

std::string format_color(color value) {
    char buffer[8];
    std::snprintf(buffer, sizeof buffer, "%02x%02x%02x", value.r, value.g,
                  value.b);
    return buffer;
}

} // namespace util
