//
// Checks embedded toolbar artwork and Native's reverse-video contract.
// Explicit contexts exercise asset selection even on a color backend.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//
#include "test_resources.h"

#include <algorithm>
#include <array>
#include <string_view>

#include "check.h"
#include "tool_icons.h"

namespace {

// Independently map the commands with established Ted artwork. Other
// commands must retain the embedded application fallback.
std::shared_ptr<const native::img> standard_icon(std::string_view command) {
    using id = native::standard_icon_id;
    static constexpr std::array commands{
        std::pair{"fileNew", id::file_new},
        std::pair{"fileOpen", id::file_open},
        std::pair{"fileSave", id::file_save},
        std::pair{"fileSaveAs", id::file_save_as},
        std::pair{"undo", id::undo},
        std::pair{"cut", id::cut},
        std::pair{"copy", id::copy},
        std::pair{"paste", id::paste}};
    for (const auto &[name, icon] : commands)
        if (name == command)
            return native::standard_icon(icon);
    return nullptr;
}

// Binary ink must have no gray, color or fractional alpha to dither.
bool is_mask(const native::img &image) {
    const auto count = image.w() * image.h();
    return std::all_of(image.pixels(), image.pixels() + count,
                      [](native::rgba pixel) {
        return pixel.r == 0 && pixel.g == 0 && pixel.b == 0 &&
               (pixel.a == 0 || pixel.a == 255);
    });
}

// Test the theme itself: application artwork must not choose these colors.
void test_highlight() {
    native::img surface(32, 24);
    auto &graphics = surface.get_gpx();
    auto appearance = native::theme::create(graphics);
    native::theme::state selected;
    selected.selected = true;
    appearance->draw_toolbar_button(native::rect(0, 0, 32, 24), "", selected);
    const auto palette = appearance->native_palette();
    CHECK(appearance->get_toolbar_text_color(selected) == palette.menu_hot_text);
    if (native::resources::current_context().color == "monochrome") {
        CHECK(palette.menu_hot_bg == native::rgba(0, 0, 0, 255));
        CHECK(palette.menu_hot_text == native::rgba(255, 255, 255, 255));
        CHECK(surface.pixels()[12 * 32 + 16] == palette.menu_hot_bg);
    }
}

} // namespace

void test_resources() {
    const auto &pack = ui::graphics_resources();
    CHECK(pack.entries().size() == 34);
    CHECK(ui::tool_icon("unknown-command") == nullptr);
    const auto automatic = native::resources::current_context();
    int icons = 0;
    for (const auto &entry : pack.entries()) {
        if (!entry.when.color.empty())
            continue;
        ++icons;
        CHECK(entry.type == "icon");
        const auto color = native::img::decode(entry.data.data(), entry.data.size());
        CHECK(color.w() == 16 && color.h() == 16);
        for (const auto toolkit : {"x11", "gemix"}) {
            const auto &variant = pack.get(
                entry.name, {"linux", toolkit, "monochrome", "", 0});
            CHECK(variant.when.color == "monochrome");
            const auto mono = native::img::decode(variant.data.data(), variant.data.size());
            CHECK(mono.w() == color.w() && mono.h() == color.h());
            CHECK(is_mask(mono));
            const auto opaque = std::count_if(mono.pixels(), mono.pixels() + 256,
                [](native::rgba pixel) { return pixel.a == 255; });
            CHECK(opaque > 0 && opaque < 256);
        }
        const auto &fallback = pack.get(entry.name, {"unknown", "", "color", "", 0});
        CHECK(fallback.data == entry.data);
        const auto &active = pack.get(entry.name);
        CHECK(active.when.color ==
              (automatic.color == "monochrome" ? "monochrome" : ""));
        const auto command = std::string_view(entry.name).substr(
            std::string_view("icons.toolbar.").size());
        const auto icon = ui::tool_icon(command);
        CHECK(icon != nullptr);
        const auto expected = native::img::decode(active.data.data(), active.data.size());
        if (icon) {
            CHECK(icon->w() == 16 && icon->h() == 16);
            if (const auto stock = standard_icon(command)) {
                CHECK(icon == stock);
                CHECK(is_mask(*icon));
            } else {
                CHECK(std::equal(icon->pixels(), icon->pixels() + 256, expected.pixels()));
            }
        }
    }
    CHECK(icons == 17);
    test_highlight();
    if (automatic.toolkit == "gemix") {
        CHECK(ui::tool_icon("fileSaveAs") ==
              native::standard_icon(native::standard_icon_id::file_save_as));
        CHECK(ui::tool_icon("fileSaveAs") != nullptr);
        // Redo is initially disabled. Its gray theme tint used to become
        // white in GEM and disappear completely on the toolbar.
        for (const auto command : {"undo", "redo"}) {
            const auto icon = ui::tool_icon(command);
            native::img surface(16, 16);
            auto &g = surface.get_gpx();
            auto appearance = native::theme::create(g);
            const auto count_ink = [&] {
                return std::count_if(surface.pixels(), surface.pixels() + 256,
                    [](native::rgba pixel) {
                        return pixel == native::rgba(0, 0, 0, 255);
                    });
            };
            g.clear(native::rgba(255, 255, 255, 255));
            appearance->draw_toolbar_icon(*icon, native::rect(0, 0, 16, 16), {});
            const auto enabled = count_ink();
            native::theme::state disabled;
            disabled.disabled = true;
            g.clear(native::rgba(255, 255, 255, 255));
            appearance->draw_toolbar_icon(*icon, native::rect(0, 0, 16, 16), disabled);
            CHECK(count_ink() > 0);
            CHECK(count_ink() < enabled);
        }
    }
}
