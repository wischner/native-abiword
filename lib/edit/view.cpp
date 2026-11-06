//
// Caret, selection and motion. A valid caret position lies inside a
// visible block: a strux position reads as the end of the block before
// it, and motion steps over strux positions so the user never sees one.
//
// Derived from AbiWord src/text/fmt/xp/fv_View.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "edit/view.h"

#include <algorithm>

#include "util/utf8.h"

namespace edit {

using namespace doc;
using fmt::block_layout;

view::view(doc::document &source, gfx::font_provider &fonts)
    : document_(source), layout_(source, fonts) {
    document_.table().add_listener(this);
    point_ = anchor_ = document_start();
}

view::~view() {
    document_.table().remove_listener(this);
}

void view::relayout() {
    layout_.rebuild();
    point_ = clamp(point_);
    anchor_ = clamp(anchor_);
}

void view::changed() {
    layout_.update();
    sticky_x_.reset();
}

range view::selection() const {
    return {std::min(point_, anchor_), std::max(point_, anchor_)};
}

position view::document_start() const {
    return layout_.block_start(*layout_.blocks().front());
}

position view::document_end() const {
    return layout_.block_end(*layout_.blocks().back());
}

position view::clamp(position pos) const {
    const block_layout *block = layout_.block_at(pos);
    return std::clamp(pos, layout_.block_start(*block),
                      layout_.block_end(*block));
}

void view::set_point(position pos) {
    point_ = anchor_ = clamp(pos);
    pending_.clear();
    sticky_x_.reset();
}

void view::extend_to(position pos) {
    point_ = clamp(pos);
    pending_.clear();
    sticky_x_.reset();
}

void view::apply_motion(position target, bool extend) {
    point_ = target;
    if (!extend)
        anchor_ = target;
    pending_.clear();
}

position view::step_left(position pos) const {
    const block_layout *block = layout_.block_at(pos);
    if (pos > layout_.block_start(*block))
        return pos - 1;
    const block_layout *before = layout_.previous_block(*block);
    return before ? layout_.block_end(*before) : pos;
}

position view::step_right(position pos) const {
    const block_layout *block = layout_.block_at(pos);
    if (pos < layout_.block_end(*block))
        return pos + 1;
    const block_layout *after = layout_.next_block(*block);
    return after ? layout_.block_start(*after) : pos;
}

void view::move_left(bool extend) {
    sticky_x_.reset();
    if (has_selection() && !extend)
        apply_motion(selection().begin, false);
    else
        apply_motion(step_left(point_), extend);
}

void view::move_right(bool extend) {
    sticky_x_.reset();
    if (has_selection() && !extend)
        apply_motion(selection().end, false);
    else
        apply_motion(step_right(point_), extend);
}

void view::move_up(bool extend) {
    if (!sticky_x_)
        sticky_x_ = layout_.locate(point_).x;
    apply_motion(clamp(layout_.move_lines(point_, *sticky_x_, -1)), extend);
}

void view::move_down(bool extend) {
    if (!sticky_x_)
        sticky_x_ = layout_.locate(point_).x;
    apply_motion(clamp(layout_.move_lines(point_, *sticky_x_, 1)), extend);
}

std::u32string view::block_text(const block_layout &block) const {
    const piece_table &table = document_.table();
    const fragments &frags = table.frags();
    std::u32string result;
    for (const frag *item = frags.next(block.strux());
         item && item->kind() != frag_kind::strux &&
         item->kind() != frag_kind::end_of_doc;
         item = frags.next(item)) {
        if (item->kind() == frag_kind::text)
            result += table.text_of(*static_cast<const frag_text *>(item));
        else
            result.push_back(0xfffc); // Holds an object's one position.
    }
    return result;
}

position view::word_left(position pos) const {
    const block_layout *block = layout_.block_at(pos);
    const position start = layout_.block_start(*block);
    if (pos == start)
        return step_left(pos);
    const std::u32string text = block_text(*block);
    std::size_t index = pos - start;
    while (index > 0 && !util::is_word_character(text[index - 1]))
        --index;
    while (index > 0 && util::is_word_character(text[index - 1]))
        --index;
    return start + static_cast<position>(index);
}

position view::word_right(position pos) const {
    const block_layout *block = layout_.block_at(pos);
    const position start = layout_.block_start(*block);
    const std::u32string text = block_text(*block);
    std::size_t index = pos - start;
    if (index >= text.size())
        return step_right(pos);
    while (index < text.size() && util::is_word_character(text[index]))
        ++index;
    while (index < text.size() && !util::is_word_character(text[index]))
        ++index;
    return start + static_cast<position>(index);
}

void view::move_word_left(bool extend) {
    sticky_x_.reset();
    apply_motion(word_left(point_), extend);
}

void view::move_word_right(bool extend) {
    sticky_x_.reset();
    apply_motion(word_right(point_), extend);
}

void view::move_line_start(bool extend) {
    sticky_x_.reset();
    apply_motion(layout_.line_bounds(point_).first, extend);
}

void view::move_line_end(bool extend) {
    sticky_x_.reset();
    const auto [first, end] = layout_.line_bounds(point_);
    // A wrapped line's end offset is the next line's start, so the
    // caret stays before it; a block's last line has no such neighbor.
    const block_layout *block = layout_.block_at(point_);
    const bool last = end >= layout_.block_end(*block);
    apply_motion(last ? end : std::max(first, end - 1), extend);
}

void view::move_document_start(bool extend) {
    sticky_x_.reset();
    apply_motion(document_start(), extend);
}

void view::move_document_end(bool extend) {
    sticky_x_.reset();
    apply_motion(document_end(), extend);
}

void view::move_page(int pixels, bool extend) {
    const gfx::device_rect caret = caret_rect();
    if (!sticky_x_)
        sticky_x_ = layout_.locate(point_).x;
    const int x = caret.x;
    apply_motion(position_at(x, std::max(0, caret.y + pixels)), extend);
}

void view::select_all() {
    anchor_ = document_start();
    point_ = document_end();
    sticky_x_.reset();
}

void view::select_word(position pos) {
    const block_layout *block = layout_.block_at(pos);
    const position start = layout_.block_start(*block);
    const std::u32string text = block_text(*block);
    std::size_t begin = pos - start;
    std::size_t end = begin;
    const auto same_class = [&](std::size_t index) {
        return util::is_word_character(text[index]);
    };
    const bool word =
        begin < text.size() ? same_class(begin) : begin > 0 && same_class(begin - 1);
    while (begin > 0 && same_class(begin - 1) == word)
        --begin;
    while (end < text.size() && same_class(end) == word)
        ++end;
    anchor_ = start + static_cast<position>(begin);
    point_ = start + static_cast<position>(end);
    sticky_x_.reset();
}

void view::select_line(position pos) {
    const auto [first, end] = layout_.line_bounds(pos);
    anchor_ = first;
    point_ = end;
    sticky_x_.reset();
}

void view::select_block(position pos) {
    const block_layout *block = layout_.block_at(pos);
    anchor_ = layout_.block_start(*block);
    point_ = layout_.block_end(*block);
    sticky_x_.reset();
}

} // namespace edit
