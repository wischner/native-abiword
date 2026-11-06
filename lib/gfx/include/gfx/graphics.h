//
// The graphics port. Layout measures through font_provider and paints
// through painter; neither interface knows about any UI toolkit, so
// the formatter runs headless and src/ui alone touches Native.
//
// This is AbiWord's GR_Graphics split in two, because measuring and
// drawing have different lifetimes: a layout outlives every paint.
//
// Derived from AbiWord src/af/gr/xp/gr_Graphics.h,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef GFX_GRAPHICS_H
#define GFX_GRAPHICS_H

#include <memory>
#include <string>
#include <string_view>

#include "util/color.h"
#include "util/units.h"

namespace gfx {

using util::lu;

// The font a piece of text asks for.
struct font_request {
    std::string family;
    double size_points = 12.0;
    bool bold = false;
    bool italic = false;

    friend bool operator==(const font_request &,
                           const font_request &) = default;
};

// A resolved font, measured in layout units at its point size.
class font {
public:
    virtual ~font() = default;

    // Return what was asked for.
    virtual const font_request &request() const = 0;

    // Return the distance from baseline to the top of the line box.
    virtual lu ascent() const = 0;

    // Return the distance from baseline to the bottom of the line box.
    virtual lu descent() const = 0;

    // Return the advance width of a character.
    virtual lu advance(char32_t character) const = 0;
};

// Finds fonts; results are shared and stay valid while held.
class font_provider {
public:
    virtual ~font_provider() = default;

    //
    // Return the closest available font.
    //
    // Notes:
    //      Never returns null: a word processor must lay out documents
    //      naming fonts this machine lacks.
    //
    virtual std::shared_ptr<const font>
    find(const font_request &request) = 0;
};

// A rectangle in device pixels.
struct device_rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// Draws in device pixels.
class painter {
public:
    virtual ~painter() = default;

    // Return device pixels per inch, zoom included.
    virtual double device_per_inch() const = 0;

    // Fill a rectangle.
    virtual void fill_rect(const device_rect &bounds,
                           util::color color) = 0;

    // Outline a rectangle.
    virtual void frame_rect(const device_rect &bounds,
                            util::color color) = 0;

    // Draw a line from one point to another.
    virtual void draw_line(int x1, int y1, int x2, int y2,
                           util::color color, int thickness) = 0;

    //
    // Draw text with its origin on a baseline.
    //
    // Parameters:
    //      face        - A font from this painter's provider.
    //      text        - Characters, all on one line.
    //      x           - Left edge in device pixels.
    //      baseline    - Baseline in device pixels.
    //      color       - Ink.
    //
    virtual void draw_text(const font &face, std::u32string_view text,
                           int x, int baseline, util::color color) = 0;

    // Draw embedded image data scaled into a rectangle.
    virtual void draw_image(std::string_view data_name,
                            const device_rect &bounds) = 0;
};

// Lays text out with a synthetic fixed-pitch font, without a display.
class fixed_font_provider final : public font_provider {
public:
    std::shared_ptr<const font> find(const font_request &request) override;
};

} // namespace gfx

#endif
