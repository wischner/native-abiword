//
// Page geometry and painting. Pages stack vertically, centered in the
// viewport. All of it derives from the layout on demand, so nothing can
// go stale; AbiWord's erase-then-draw sequencing has no equivalent.
//
// Derived from AbiWord src/text/fmt/xp/fv_View_protected.cpp and
// fp_Page.cpp, Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cmath>

#include "edit/view.h"

namespace edit {

using namespace doc;

namespace {

constexpr int page_gap = 16;
constexpr util::color paper{255, 255, 255};
constexpr util::color edge{120, 120, 120};
constexpr util::color shadow{90, 90, 90};

} // namespace

double view::device_per_inch() const {
    return 96.0 * zoom_;
}

void view::set_zoom(double zoom) {
    zoom_ = std::clamp(zoom, 0.25, 5.0);
}

void view::set_viewport_width(int width) {
    viewport_width_ = width;
}

int view::content_width() const {
    int widest = 0;
    for (const fmt::page &sheet : layout_.pages())
        widest = std::max(widest, util::lu_to_device(sheet.width,
                                                     device_per_inch()));
    return widest + page_gap * 2;
}

int view::page_top(std::size_t index) const {
    int top = page_gap;
    const auto &pages = layout_.pages();
    for (std::size_t page = 0; page < index && page < pages.size(); ++page)
        top += util::lu_to_device(pages[page].height, device_per_inch()) +
               page_gap;
    return top;
}

int view::content_height() const {
    return page_top(layout_.pages().size());
}

int view::page_x(std::size_t index) const {
    const int width = util::lu_to_device(layout_.pages().at(index).width,
                                         device_per_inch());
    return std::max(page_gap, (viewport_width_ - width) / 2);
}

int view::page_left() const {
    return page_x(layout_.locate(point_).page);
}

int view::current_page() const {
    return static_cast<int>(layout_.locate(point_).page) + 1;
}

gfx::device_rect view::caret_rect() const {
    const fmt::caret_location caret = layout_.locate(point_);
    const double dpi = device_per_inch();
    return {page_x(caret.page) + util::lu_to_device(caret.x, dpi),
            page_top(caret.page) + util::lu_to_device(caret.y, dpi), 1,
            std::max(1, util::lu_to_device(caret.height, dpi))};
}

position view::position_at(int x, int y) const {
    const auto &pages = layout_.pages();
    const double dpi = device_per_inch();
    std::size_t index = 0;
    while (index + 1 < pages.size() && y >= page_top(index + 1) - page_gap / 2)
        ++index;
    const util::lu page_px = util::device_to_lu(x - page_x(index), dpi);
    const util::lu page_py = util::device_to_lu(y - page_top(index), dpi);
    return clamp(layout_.position_at(index, page_px, page_py));
}

void view::draw_selection(gfx::painter &target, const fmt::page &sheet,
                          int origin_x, int origin_y) const {
    if (!has_selection())
        return;
    const range chosen = selection();
    const double dpi = device_per_inch();
    for (const fmt::placed_line &placed : sheet.lines) {
        const position base = layout_.block_start(*placed.block);
        const position first = base + placed.item->first_offset();
        const position last = base + placed.item->end_offset();
        if (last < chosen.begin || first >= chosen.end)
            continue;
        const std::uint32_t from =
            std::max(first, chosen.begin) - base;
        const std::uint32_t to = std::min(last, chosen.end) - base;
        const util::lu left = placed.column_x + placed.item->x_of_offset(from);
        util::lu right = placed.column_x + placed.item->x_of_offset(to);
        // A selected paragraph mark shows as a little extra width.
        if (chosen.end > last)
            right += util::layout_resolution / 16;
        if (right <= left)
            continue;
        const int x = origin_x + util::lu_to_device(left, dpi);
        target.draw_selection(
            {x, origin_y + util::lu_to_device(placed.y, dpi),
             origin_x + util::lu_to_device(right, dpi) - x,
             std::max(1, util::lu_to_device(placed.item->height(), dpi))},
            [&](gfx::painter &selected) {
                // Redraw the complete line under the selection clip so
                // partial words keep their original glyph positioning.
                placed.item->draw(
                    selected,
                    origin_x + util::lu_to_device(placed.column_x, dpi),
                    origin_y + util::lu_to_device(
                        placed.y + placed.item->baseline(), dpi));
            });
    }
}

void view::draw(gfx::painter &target, int scroll_x, int scroll_y, int width,
                int height) const {
    const auto &pages = layout_.pages();
    const double dpi = device_per_inch();
    for (std::size_t index = 0; index < pages.size(); ++index) {
        const fmt::page &sheet = pages[index];
        const int top = page_top(index) - scroll_y;
        const int sheet_height = util::lu_to_device(sheet.height, dpi);
        if (top >= height)
            break;
        if (top + sheet_height + 3 <= 0)
            continue;
        const int left = page_x(index) - scroll_x;
        const int sheet_width = util::lu_to_device(sheet.width, dpi);
        if (left >= width || left + sheet_width <= 0)
            continue;
        target.fill_rect({left + 3, top + 3, sheet_width, sheet_height},
                         shadow);
        target.fill_rect({left, top, sheet_width, sheet_height}, paper);
        target.frame_rect({left, top, sheet_width, sheet_height}, edge);
        for (const fmt::placed_line &placed : sheet.lines) {
            const int line_top = top + util::lu_to_device(placed.y, dpi);
            const int line_height =
                util::lu_to_device(placed.item->height(), dpi);
            if (line_top + line_height < 0 || line_top > height)
                continue;
            placed.item->draw(
                target, left + util::lu_to_device(placed.column_x, dpi),
                top + util::lu_to_device(
                          placed.y + placed.item->baseline(), dpi));
        }
        draw_selection(target, sheet, left, top);
    }
}

} // namespace edit
