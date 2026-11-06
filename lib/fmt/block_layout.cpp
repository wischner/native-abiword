//
// The line breaker and line finishing. As in AbiWord a line breaks at
// the first break opportunity that overflows; trailing spaces stay on
// their line and may hang past the margin. A word wider than the whole
// line breaks at a character.
//
// Derived from AbiWord src/text/fmt/xp/fb_LineBreaker.cpp, fp_Line.cpp
// and fb_Alignment.cpp, Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "fmt/block_layout.h"

#include <algorithm>
#include <cmath>

#include "util/utf8.h"

namespace fmt {
namespace {

// Report whether a line may break after a character.
bool breaks_after(char32_t c) {
    return util::is_space(c) || c == U'-' || c == 0x2010 || c == 0x2013 ||
           c == 0x2014;
}

// Return how much of a text run fits, as a character count.
//
// Parameters:
//      item        - Run being placed.
//      room        - Width left on the line.
//      line_empty  - Whether nothing precedes this run on the line.
//
// Returns:
//      Characters to keep; 0 moves the run to the next line.
std::size_t fitting_prefix(const text_run &item, lu room, bool line_empty) {
    const std::u32string &text = item.text();
    std::size_t best = 0;
    std::size_t index = 0;
    while (index < text.size()) {
        // Measure up to the next opportunity, ignoring trailing spaces.
        std::size_t end = index;
        while (end < text.size() && !breaks_after(text[end]))
            ++end;
        std::size_t ink_end = end;
        if (end < text.size() && !util::is_space(text[end]))
            ++ink_end;
        while (end < text.size() && breaks_after(text[end]))
            ++end;
        if (item.prefix_width(ink_end) > room)
            break;
        best = end;
        index = end;
    }
    if (best > 0 || !line_empty)
        return best;
    // A single word is wider than the line: break between characters.
    std::size_t count = 1;
    while (count < text.size() && item.prefix_width(count + 1) <= room)
        ++count;
    return count;
}

} // namespace

std::uint32_t line::first_offset() const {
    return runs_.front()->offset();
}

std::uint32_t line::end_offset() const {
    return runs_.back()->offset() + runs_.back()->length();
}

break_kind line::forced_break() const {
    return runs_.back()->forced_break();
}

lu line::x_of_offset(std::uint32_t offset) const {
    for (const auto &item : runs_) {
        if (offset <= item->offset() + item->length())
            return x_ + item->x() + item->x_of_offset(offset);
    }
    return x_ + content_width_;
}

std::uint32_t line::offset_at_x(lu x) const {
    x -= x_;
    for (const auto &item : runs_) {
        if (x < item->x() + item->width() ||
            item.get() == runs_.back().get()) {
            const std::uint32_t found =
                item->offset_at_x(x - item->x());
            // A forced break's own position is not a place to stand on
            // from its line: the caret stays before it.
            if (item->forced_break() != break_kind::none)
                return item->offset();
            return found;
        }
    }
    return end_offset();
}

void line::draw(gfx::painter &painter, int column_x, int baseline) const {
    for (const auto &item : runs_) {
        draw_context context{painter, 0, baseline};
        context.x = column_x + to_device(painter, x_ + item->x());
        item->draw(context);
    }
}

block_layout::block_layout(const doc::frag_strux *strux) : strux_(strux) {
}

void block_layout::format(const format_context &context) {
    resolve_properties(context);
    break_lines(build_runs(context), context);
    dirty_ = false;
}

lu block_layout::tab_target(lu x, tab_kind &kind) const {
    for (const tab_stop &stop : properties_.tabs) {
        if (stop.position > x) {
            kind = stop.kind;
            return stop.position;
        }
    }
    kind = tab_kind::left;
    const lu interval = properties_.default_tab;
    return (x / interval + 1) * interval;
}

void block_layout::break_lines(std::vector<std::unique_ptr<run>> runs,
                               const format_context &context) {
    lines_.clear();
    const block_properties &props = properties_;
    const lu full_width =
        context.column_width - props.margin_left - props.margin_right;

    auto current = std::make_unique<line>();
    bool first_line = true;
    lu x = 0;
    lu indent = props.text_indent;

    // Pending centered or right tab and the width it is aligned over.
    tab_run *open_tab = nullptr;
    tab_kind open_kind = tab_kind::left;
    lu open_target = 0;
    lu open_x = 0;

    const auto close_open_tab = [&] {
        if (!open_tab)
            return;
        const lu text_width = x - open_x;
        const lu shift = open_kind == tab_kind::right ? text_width
                                                      : text_width / 2;
        const lu width = std::max<lu>(open_target - open_x - shift, 0);
        open_tab->set_width(width);
        x = open_x + width + text_width;
        open_tab = nullptr;
    };

    const auto end_line = [&](bool last) {
        close_open_tab();
        finish_line(*current, last, context);
        current->x_ += first_line ? indent : 0;
        lines_.push_back(std::move(current));
        current = std::make_unique<line>();
        first_line = false;
        x = 0;
    };

    std::size_t index = 0;
    while (index < runs.size()) {
        std::unique_ptr<run> &item = runs[index];
        const lu available =
            full_width - (first_line ? indent : 0);
        if (auto *text = dynamic_cast<text_run *>(item.get())) {
            const std::size_t keep = fitting_prefix(
                *text, available - x,
                current->runs_.empty());
            if (keep == 0) {
                end_line(false);
                continue;
            }
            if (keep < text->text().size()) {
                std::unique_ptr<text_run> tail = text->split(keep);
                x += text->width();
                current->runs_.push_back(std::move(item));
                item = std::move(tail);
                end_line(false);
                continue;
            }
            x += text->width();
        } else if (auto *tab = dynamic_cast<tab_run *>(item.get())) {
            close_open_tab();
            tab_kind kind = tab_kind::left;
            const lu target = tab_target(
                x + (first_line ? indent : 0), kind) -
                (first_line ? indent : 0);
            if (kind == tab_kind::left) {
                tab->set_width(target - x);
                x = target;
            } else {
                open_tab = tab;
                open_kind = kind;
                open_target = target;
                open_x = x;
            }
        } else {
            if (item->width() > available - x &&
                !current->runs_.empty()) {
                end_line(false);
                continue;
            }
            x += item->width();
        }
        const bool stops_line = item->forced_break() != break_kind::none;
        current->runs_.push_back(std::move(item));
        ++index;
        if (stops_line)
            end_line(false);
    }
    if (!current->runs_.empty())
        end_line(true);
}

void block_layout::finish_line(line &item, bool last_line,
                               const format_context &context) {
    const block_properties &props = properties_;

    lu ascent = 0;
    lu descent = 0;
    lu width = 0;
    lu trailing = 0;
    std::size_t spaces = 0;
    for (const auto &entry : item.runs_) {
        ascent = std::max(ascent, entry->ascent());
        descent = std::max(descent, entry->descent());
        width += entry->width();
    }
    // Trailing spaces and the spaces of runs after the last tab decide
    // alignment; justification only stretches after the last tab.
    std::size_t first_stretch = 0;
    for (std::size_t index = 0; index < item.runs_.size(); ++index)
        if (dynamic_cast<tab_run *>(item.runs_[index].get()))
            first_stretch = index + 1;
    for (auto entry = item.runs_.rbegin(); entry != item.runs_.rend();
         ++entry) {
        const auto *text = dynamic_cast<const text_run *>(entry->get());
        if (!text) {
            if ((*entry)->width() == 0)
                continue;
            break;
        }
        const lu spare = text->trailing_space_width();
        trailing += spare;
        if (spare < text->width())
            break;
    }

    const lu line_indent =
        lines_.empty() ? props.text_indent : 0;
    const lu available = context.column_width - props.margin_left -
                         props.margin_right - line_indent;
    const lu ink_width = width - trailing;
    const lu extra = std::max<lu>(available - ink_width, 0);

    item.x_ = props.margin_left;
    switch (props.align) {
    case alignment::center:
        item.x_ += extra / 2;
        break;
    case alignment::right:
        item.x_ += extra;
        break;
    case alignment::justify: {
        const bool forced = !item.runs_.empty() &&
                            item.forced_break() != break_kind::none;
        if (last_line || forced)
            break;
        for (std::size_t index = first_stretch; index < item.runs_.size();
             ++index)
            if (const auto *text = dynamic_cast<const text_run *>(
                    item.runs_[index].get()))
                spaces += text->space_count();
        // The trailing spaces of the line receive no stretch.
        std::size_t trailing_spaces = 0;
        for (auto entry = item.runs_.rbegin();
             entry != item.runs_.rend(); ++entry) {
            const auto *text = dynamic_cast<const text_run *>(entry->get());
            if (!text) {
                if ((*entry)->width() == 0)
                    continue;
                break;
            }
            const std::u32string &chars = text->text();
            std::size_t end = chars.size();
            while (end > 0 && chars[end - 1] == U' ') {
                --end;
                ++trailing_spaces;
            }
            if (end > 0)
                break;
        }
        const std::size_t gaps =
            spaces > trailing_spaces ? spaces - trailing_spaces : 0;
        if (gaps == 0)
            break;
        const lu per_space = extra / static_cast<lu>(gaps);
        lu remainder = extra - per_space * static_cast<lu>(gaps);
        std::size_t left = gaps;
        for (std::size_t index = first_stretch; index < item.runs_.size();
             ++index) {
            auto *text = dynamic_cast<text_run *>(item.runs_[index].get());
            if (!text)
                continue;
            const std::size_t count = std::min(text->space_count(), left);
            left -= count;
            const lu chunk =
                std::min<lu>(remainder, static_cast<lu>(count));
            text->set_space_stretch(per_space, chunk);
            remainder -= chunk;
        }
        break;
    }
    case alignment::left:
        break;
    }

    lu position = 0;
    for (const auto &entry : item.runs_) {
        entry->set_x(position);
        position += entry->width();
    }
    item.content_width_ = position;

    const lu natural = ascent + descent;
    lu height = natural;
    switch (props.spacing) {
    case line_spacing::multiple:
        height = static_cast<lu>(
            std::lround(natural * props.spacing_multiple));
        break;
    case line_spacing::exact:
        height = props.spacing_length;
        break;
    case line_spacing::at_least:
        height = std::max(natural, props.spacing_length);
        break;
    }
    item.height_ = height;
    item.baseline_ = height - descent;
}

} // namespace fmt
