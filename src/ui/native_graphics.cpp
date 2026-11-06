//
// Fonts and painting on Native. Widths are measured once at a large
// reference size and scaled, so wrapping never depends on zoom. Common
// Office fonts map to their metric-compatible Liberation equivalents.
//
// Derived from AbiWord src/af/gr/xp/gr_Graphics.cpp and gr_CharWidths,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "native_graphics.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <set>

#include "util/utf8.h"

namespace ui {
namespace {

// Em size of the measuring font; advances are integers at this size.
constexpr int reference_em = 2048;

constexpr double lu_per_point = 20.0;

constexpr int coordinate_limit = std::numeric_limits<native::coord>::max();

// Fonts tried, in order, when a family is missing.
constexpr std::array<std::string_view, 6> fallback_families{
    "Liberation Serif", "Noto Serif", "DejaVu Serif",
    "Liberation Sans",  "Noto Sans",  "DejaVu Sans"};

// Metric-compatible replacements for fonts Linux rarely has.
constexpr std::array<std::pair<std::string_view, std::string_view>, 6>
    substitutes{{{"Times New Roman", "Liberation Serif"},
                 {"Times", "Liberation Serif"},
                 {"Arial", "Liberation Sans"},
                 {"Helvetica", "Liberation Sans"},
                 {"Courier New", "Liberation Mono"},
                 {"Courier", "Liberation Mono"}}};

// Report whether a style name marks a narrower or unusual cut.
bool is_special_cut(const std::string &style) {
    static constexpr std::array<std::string_view, 3> words{
        "Condensed", "Narrow", "Compressed"};
    return std::ranges::any_of(words, [&](std::string_view word) {
        return style.find(word) != std::string::npos;
    });
}

// Clamp a pixel coordinate into Native's coordinate range.
native::coord to_coord(int value) {
    return static_cast<native::coord>(
        std::clamp(value, -coordinate_limit, coordinate_limit));
}

// Convert a document rectangle to a Native one.
native::rect to_rect(const gfx::device_rect &bounds) {
    return native::rect(
        to_coord(bounds.x), to_coord(bounds.y),
        static_cast<native::dim>(
            std::clamp(bounds.width, 0, coordinate_limit)),
        static_cast<native::dim>(
            std::clamp(bounds.height, 0, coordinate_limit)));
}

// Convert a document color to an opaque Native one.
native::rgba to_rgba(util::color color) {
    return native::rgba(color.r, color.g, color.b, 255);
}

// One face at one point size, in layout units.
class native_font final : public gfx::font {
public:
    native_font(gfx::font_request request, std::shared_ptr<font_face> face)
        : request_(std::move(request)), face_(std::move(face)),
          em_(request_.size_points * lu_per_point) {
    }

    const gfx::font_request &request() const override {
        return request_;
    }

    gfx::lu ascent() const override {
        return static_cast<gfx::lu>(std::lround(face_->ascent() * em_));
    }

    gfx::lu descent() const override {
        return static_cast<gfx::lu>(std::lround(face_->descent() * em_));
    }

    gfx::lu advance(char32_t character) const override {
        return static_cast<gfx::lu>(
            std::lround(face_->advance(character) * em_));
    }

    // Return the toolkit font for a device resolution.
    const native::font_t &at_resolution(double device_per_inch) const {
        const double pixels = request_.size_points * device_per_inch / 72.0;
        return face_->at_size(std::max(1, static_cast<int>(
                                              std::lround(pixels))));
    }

private:
    gfx::font_request request_;
    std::shared_ptr<font_face> face_;
    double em_;
};

} // namespace

font_face::font_face(const native::font_description &description)
    : description_(description),
      reference_(native::font_t::from_file(
          description.path, reference_em, description.face_index,
          native::font_size_basis::em)) {
    if (!reference_.valid())
        return;
    const native::font_metrics metrics = reference_.get_metrics();
    ascent_ = static_cast<double>(metrics.ascent) / reference_em;
    descent_ = static_cast<double>(metrics.descent + metrics.leading) /
               reference_em;
}

double font_face::advance(char32_t character) {
    const auto found = advances_.find(character);
    if (found != advances_.end())
        return found->second;
    const double value =
        static_cast<double>(
            reference_.measure_character(character).advance) /
        reference_em;
    advances_.emplace(character, value);
    return value;
}

const native::font_t &font_face::at_size(int pixels) {
    const auto found = sizes_.find(pixels);
    if (found != sizes_.end())
        return found->second;
    return sizes_
        .emplace(pixels, native::font_t::from_file(
                             description_.path, pixels,
                             description_.face_index,
                             native::font_size_basis::em))
        .first->second;
}

font_catalog::font_catalog() {
    for (native::font_description &description :
         native::font_t::enumerate_installed())
        if (!description.family.empty())
            by_family_[description.family].push_back(
                std::move(description));
    families_.reserve(by_family_.size());
    for (const auto &[family, faces] : by_family_)
        families_.push_back(family);
}

const native::font_description *
font_catalog::best_face(const gfx::font_request &request) const {
    std::vector<std::string> names{request.family};
    for (const auto &[wanted, replacement] : substitutes)
        if (wanted == request.family)
            names.emplace_back(replacement);
    names.insert(names.end(), fallback_families.begin(),
                 fallback_families.end());
    const int weight = request.bold ? 700 : 400;
    for (const std::string &name : names) {
        const auto family = by_family_.find(name);
        if (family == by_family_.end())
            continue;
        const native::font_description *best = nullptr;
        int best_score = std::numeric_limits<int>::max();
        for (const native::font_description &face : family->second) {
            const int score = std::abs(face.weight - weight) +
                              (face.italic != request.italic ? 1000 : 0) +
                              (is_special_cut(face.style) ? 500 : 0);
            if (score < best_score) {
                best = &face;
                best_score = score;
            }
        }
        if (best)
            return best;
    }
    return by_family_.empty() ? nullptr
                              : &by_family_.begin()->second.front();
}

std::shared_ptr<font_face>
font_catalog::open_face(const native::font_description &description) {
    const std::string key = description.path.string() + '#' +
                            std::to_string(description.face_index);
    auto &entry = faces_[key];
    if (!entry)
        entry = std::make_shared<font_face>(description);
    return entry;
}

std::shared_ptr<const gfx::font>
font_catalog::find(const gfx::font_request &request) {
    const std::string key = request.family + '|' +
                            std::to_string(request.size_points) +
                            (request.bold ? "b" : "") +
                            (request.italic ? "i" : "");
    auto &entry = fonts_[key];
    if (entry)
        return entry;
    native::font_description none;
    const native::font_description *face = best_face(request);
    entry = std::make_shared<native_font>(
        request, open_face(face ? *face : none));
    return entry;
}

image_cache::entry::entry(const std::uint8_t *data, std::size_t size)
    : image(native::img::decode(data, size)) {
}

const native::img *image_cache::find(const doc::document &source,
                                     std::string_view name) {
    if (const auto found = images_.find(name); found != images_.end())
        return found->second ? &found->second->image : nullptr;
    std::unique_ptr<entry> image;
    const auto data = source.data().find(name);
    if (data != source.data().end()) {
        try {
            image = std::make_unique<entry>(data->second.bytes.data(),
                                            data->second.bytes.size());
        } catch (const std::exception &) {
            // An SVG or damaged image paints as an empty frame.
        }
    }
    const auto stored =
        images_.emplace(std::string(name), std::move(image)).first;
    return stored->second ? &stored->second->image : nullptr;
}

void image_cache::clear() {
    images_.clear();
}

native_painter::native_painter(native::gpx &target, double device_per_inch,
                               const doc::document &source,
                               image_cache &images)
    : target_(target), dpi_(device_per_inch), source_(source),
      images_(images) {
}

void native_painter::fill_rect(const gfx::device_rect &bounds,
                               util::color color) {
    target_.set_ink(to_rgba(color)).draw_rect(to_rect(bounds), true);
}

void native_painter::frame_rect(const gfx::device_rect &bounds,
                                util::color color) {
    target_.set_ink(to_rgba(color)).set_pen(1);
    target_.draw_rect(to_rect(bounds), false);
}

void native_painter::draw_line(int x1, int y1, int x2, int y2,
                               util::color color, int thickness) {
    target_.set_ink(to_rgba(color))
        .set_pen(static_cast<std::uint8_t>(std::clamp(thickness, 1, 16)));
    target_.draw_line(native::point(to_coord(x1), to_coord(y1)),
                      native::point(to_coord(x2), to_coord(y2)));
}

void native_painter::draw_text(const gfx::font &face,
                               std::u32string_view text, int x,
                               int baseline, util::color color) {
    const native::font_t &font =
        static_cast<const native_font &>(face).at_resolution(dpi_);
    if (!font.valid())
        return;
    target_.set_font(font).set_ink(to_rgba(color));
    target_.draw_text(
        util::to_utf8(text),
        native::point(to_coord(x),
                      to_coord(baseline - font.get_metrics().ascent)));
}

void native_painter::draw_image(std::string_view data_name,
                                const gfx::device_rect &bounds) {
    const native::img *image = images_.find(source_, data_name);
    if (image)
        target_.draw_img(*image, to_rect(bounds));
    else
        frame_rect(bounds, {128, 128, 128});
}

} // namespace ui
