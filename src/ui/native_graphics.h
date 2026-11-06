//
// Implements the gfx port on Native: fonts from installed faces and a
// painter over a gpx. This is the only bridge from the toolkit-free
// libraries to the UI toolkit.
//
// Derived from AbiWord src/af/gr/xp/gr_Graphics.cpp and gr_CharWidths,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef UI_NATIVE_GRAPHICS_H
#define UI_NATIVE_GRAPHICS_H

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <native.h>

#include "doc/document.h"
#include "gfx/graphics.h"

namespace ui {

// One installed face: shared measurements and device-size copies.
class font_face {
public:
    // Open a face from its description.
    explicit font_face(const native::font_description &description);

    // Report whether the face opened.
    bool valid() const { return reference_.valid(); }

    // Return ascent as a fraction of the em.
    double ascent() const { return ascent_; }

    // Return descent plus line gap as a fraction of the em.
    double descent() const { return descent_; }

    // Return a character's advance as a fraction of the em.
    double advance(char32_t character);

    // Return this face at an em size in pixels.
    const native::font_t &at_size(int pixels);

private:
    native::font_description description_;
    native::font_t reference_;
    double ascent_ = 0.8;
    double descent_ = 0.2;
    std::unordered_map<char32_t, double> advances_;
    std::map<int, native::font_t> sizes_;
};

// Finds faces among the installed fonts.
class font_catalog final : public gfx::font_provider {
public:
    // Enumerate installed fonts.
    font_catalog();

    // Return family names, sorted.
    const std::vector<std::string> &families() const { return families_; }

    std::shared_ptr<const gfx::font>
    find(const gfx::font_request &request) override;

private:
    const native::font_description *
    best_face(const gfx::font_request &request) const;
    std::shared_ptr<font_face>
    open_face(const native::font_description &description);

    std::map<std::string, std::vector<native::font_description>> by_family_;
    std::vector<std::string> families_;
    std::map<std::string, std::shared_ptr<font_face>> faces_;
    std::map<std::string, std::shared_ptr<const gfx::font>> fonts_;
};

// Decoded document images, kept across paints.
class image_cache {
public:
    // Return a decoded image, or null when the data cannot be decoded.
    const native::img *find(const doc::document &source,
                            std::string_view name);

    // Forget everything, e.g. when another document opens.
    void clear();

private:
    // Holds a decoded image; img neither copies nor moves.
    struct entry {
        entry(const std::uint8_t *data, std::size_t size);
        native::img image;
    };

    std::map<std::string, std::unique_ptr<entry>, std::less<>> images_;
};

// Paints into a Native graphics context.
class native_painter final : public gfx::painter {
public:
    // Create a painter for one paint.
    native_painter(native::gpx &target, double device_per_inch,
                   const doc::document &source, image_cache &images);

    double device_per_inch() const override { return dpi_; }
    void fill_rect(const gfx::device_rect &bounds,
                   util::color color) override;
    void frame_rect(const gfx::device_rect &bounds,
                    util::color color) override;
    void draw_line(int x1, int y1, int x2, int y2, util::color color,
                   int thickness) override;
    void draw_text(const gfx::font &face, std::u32string_view text, int x,
                   int baseline, util::color color) override;
    void draw_image(std::string_view data_name,
                    const gfx::device_rect &bounds) override;

private:
    native::gpx &target_;
    double dpi_;
    const doc::document &source_;
    image_cache &images_;
};

} // namespace ui

#endif
