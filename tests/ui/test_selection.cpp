//
// Paints real document selections, including partial words and styled text,
// and checks their pixels and clipping against Native's selection palette.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//
#include "test_selection.h"

#include <stdexcept>

#include "check.h"
#include "edit/view.h"
#include "native_graphics.h"

namespace {

// Document colors must yield to selection ink without changing stored style.
void check_selection(ui::font_catalog &fonts, double zoom) {
    doc::document document;
    edit::view view(document, fonts);
    ui::image_cache images;
    view.set_viewport_width(800);
    view.set_zoom(zoom);
    view.insert_text(U"Hello world");
    view.select_all();
    view.set_char_format({{"color", "ff0000"}, {"bgcolor", "00ff00"},
                          {"text-decoration", "underline line-through"}});
    const auto start = view.document_start();
    view.set_point(start + 1);
    const auto first = view.caret_rect();
    view.set_point(start + 8);
    const auto last = view.caret_rect();
    constexpr int scroll_x = 7;
    constexpr int scroll_y = 9;
    const native::rect bounds(first.x - scroll_x, first.y - scroll_y,
                              last.x - first.x, first.height);
    native::img plain(800, 300), selected(800, 300);
    auto render = [&](native::img &image) {
        auto &g = image.get_gpx();
        g.set_clip(native::rect(0, 0, 800, 300));
        g.clear(native::rgba(255, 255, 255, 255));
        ui::native_painter painter(g, view.device_per_inch(), document, images);
        view.draw(painter, scroll_x, scroll_y, 800, 300);
    };
    render(plain);
    view.set_point(start + 1);
    view.extend_to(start + 8);
    render(selected);
    auto appearance = native::theme::create(selected.get_gpx());
    const auto background = appearance->get_selection_background_color();
    const auto foreground = appearance->get_selection_foreground_color();
    int background_pixels = 0, foreground_pixels = 0;
    bool outside_unchanged = true, document_colors_hidden = true;
    for (int y = 0; y < selected.h(); ++y) {
        for (int x = 0; x < selected.w(); ++x) {
            const auto pixel = selected.pixels()[y * selected.w() + x];
            if (bounds.contains(native::point(x, y))) {
                background_pixels += pixel == background;
                foreground_pixels += pixel == foreground;
                document_colors_hidden &= pixel != native::rgba(255, 0, 0, 255)
                    && pixel != native::rgba(0, 255, 0, 255);
            } else {
                outside_unchanged &= pixel == plain.pixels()[y * plain.w() + x];
            }
        }
    }
    CHECK(background_pixels > bounds.w() * bounds.h() / 3);
    CHECK(foreground_pixels > 0);
    CHECK(document_colors_hidden);
    CHECK(outside_unchanged);
    CHECK(view.char_property("color") == "ff0000");
    CHECK(view.char_property("bgcolor") == "00ff00");
    if (native::resources::current_context().color == "monochrome") {
        CHECK(background == native::rgba(0, 0, 0, 255));
        CHECK(foreground == native::rgba(255, 255, 255, 255));
    }

    // Selection callbacks must not leak a smaller clip or selected ink.
    auto &g = selected.get_gpx();
    const native::rgba ink(12, 34, 56, 255);
    g.set_clip(native::rect(2, 3, 700, 200)).set_ink(ink);
    ui::native_painter painter(g, view.device_per_inch(), document, images);
    try {
        painter.draw_selection({10, 10, 20, 20}, [](gfx::painter &) {
            throw std::runtime_error("paint failure");
        });
        CHECK(false);
    } catch (const std::runtime_error &) {
        CHECK(g.get_ink() == ink);
        const auto clip = g.get_clip();
        CHECK(clip.x1() == 2 && clip.y1() == 3 &&
              clip.w() == 700 && clip.h() == 200);
    }
}

} // namespace

void test_selection() {
    ui::font_catalog fonts;
    for (const double zoom : {0.75, 1.0, 1.5})
        check_selection(fonts, zoom);
}
