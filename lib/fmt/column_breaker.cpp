//
// Flows formatted lines onto pages and columns, honoring forced
// breaks, keep-with-next, keep-together and widow/orphan control.
// Space between paragraphs is the larger of the two margins that meet.
//
// Derived from AbiWord src/text/fmt/xp/fb_ColumnBreaker.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>

#include "fmt/doc_layout.h"

namespace fmt {
namespace {

// Sum the height of a block's lines from a starting line.
lu lines_height(const block_layout &block, std::size_t first,
                std::size_t count) {
    lu total = 0;
    const auto &lines = block.lines();
    for (std::size_t index = first;
         index < lines.size() && index < first + count; ++index)
        total += lines[index]->height();
    return total;
}

// Count the lines from a starting line that fit a height.
std::size_t fitting_lines(const block_layout &block, std::size_t first,
                          lu room) {
    const auto &lines = block.lines();
    std::size_t count = 0;
    for (std::size_t index = first; index < lines.size(); ++index) {
        room -= lines[index]->height();
        if (room < 0)
            break;
        ++count;
    }
    return count;
}

// Tracks the write position while flowing.
struct flow_state {
    std::vector<page> &pages;
    section_geometry geometry;
    int column = 0;
    lu y = 0;

    // Start a new page.
    void new_page() {
        page sheet;
        sheet.width = geometry.page_width;
        sheet.height = geometry.page_height;
        sheet.margin_left = geometry.margin_left;
        sheet.margin_right = geometry.margin_right;
        pages.push_back(std::move(sheet));
        column = 0;
        y = geometry.margin_top;
    }

    // Move to the next column, or to a new page after the last one.
    void next_column() {
        if (++column >= geometry.columns) {
            new_page();
            return;
        }
        y = geometry.margin_top;
    }

    // Return the lowest usable y.
    lu bottom() const {
        return geometry.page_height - geometry.margin_bottom;
    }

    // Return the left edge of the current column.
    lu column_x() const {
        return geometry.margin_left +
               column * (geometry.column_width() + geometry.column_gap);
    }

    // Report whether nothing has been placed in the current column.
    bool at_top() const {
        return y == geometry.margin_top;
    }
};

} // namespace

void doc_layout::flow() {
    pages_.clear();
    if (blocks_.empty())
        return;

    flow_state state{pages_, geometry_for(section_of(*blocks_.front())), 0,
                     0};
    state.new_page();
    const doc::frag_strux *section = section_of(*blocks_.front());
    lu previous_bottom = 0;

    for (std::size_t index = 0; index < blocks_.size(); ++index) {
        const block_layout &block = *blocks_[index];
        const block_properties &props = block.properties();
        const auto &lines = block.lines();

        if (const doc::frag_strux *owner = section_of(block); owner != section) {
            section = owner;
            state.geometry = geometry_for(section);
            state.new_page();
            previous_bottom = 0;
        }

        // Neighbouring margins collapse, and vanish at a column top.
        const lu gap = state.at_top()
            ? 0
            : std::max(previous_bottom, props.margin_top);
        state.y += gap;

        // Keep rules can push the whole block to the next column.
        const lu whole = lines_height(block, 0, lines.size());
        lu needed = props.keep_together ? whole : 0;
        if (props.keep_with_next && index + 1 < blocks_.size()) {
            const block_layout &next = *blocks_[index + 1];
            needed = whole +
                     std::max(props.margin_bottom,
                              next.properties().margin_top) +
                     lines_height(next, 0, 1);
        }
        if (needed > 0 && state.y + needed > state.bottom() &&
            !state.at_top())
            state.next_column();

        std::size_t number = 0;
        while (number < lines.size()) {
            std::size_t fit = fitting_lines(block, number,
                                            state.bottom() - state.y);
            const std::size_t left = lines.size() - number;
            if (fit < left) {
                const auto widows =
                    static_cast<std::size_t>(std::max(props.widows, 1));
                const auto orphans =
                    static_cast<std::size_t>(std::max(props.orphans, 1));
                if (left - fit < widows)
                    fit = left > widows ? std::min(fit, left - widows) : 0;
                if (number == 0 && fit < orphans)
                    fit = 0;
            }
            if (fit == 0) {
                if (!state.at_top()) {
                    state.next_column();
                    continue;
                }
                // An oversized line still has to go somewhere.
                fit = std::max<std::size_t>(
                    fitting_lines(block, number, state.bottom() - state.y),
                    1);
            }
            bool forced = false;
            const std::size_t stop = number + fit;
            while (number < stop && !forced) {
                const line &item = *lines[number++];
                pages_.back().lines.push_back(
                    {&item, &block, index, state.y, state.column_x()});
                state.y += item.height();
                if (item.forced_break() == break_kind::page) {
                    state.new_page();
                    forced = true;
                } else if (item.forced_break() == break_kind::column) {
                    state.next_column();
                    forced = true;
                }
            }
            if (!forced && number < lines.size())
                state.next_column();
        }
        previous_bottom = props.margin_bottom;
    }
}

} // namespace fmt
