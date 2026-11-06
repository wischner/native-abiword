//
// Text and structure editing, formatting and undo. Each user action is
// one history glob, so undo always reverses what the user did, however
// many primitive changes it took.
//
// Derived from AbiWord src/text/fmt/xp/fv_View.cpp and fv_View_cmd.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>

#include "edit/view.h"
#include "util/utf8.h"

namespace edit {

using namespace doc;
using fmt::block_layout;

void view::on_change(const change_record &record) {
    switch (record.type) {
    case change_type::insert_span:
    case change_type::insert_object:
    case change_type::insert_strux:
    case change_type::change_span:
    case change_type::change_strux:
        last_change_ = record.pos + record.length;
        break;
    case change_type::delete_span:
    case change_type::delete_object:
    case change_type::delete_strux:
        last_change_ = record.pos;
        break;
    }
}

void view::on_strux_removed(const frag_strux &) {
}

void view::delete_selection() {
    if (!has_selection())
        return;
    const range target = selection();
    document_.table().delete_range(target.begin, target.end);
    point_ = anchor_ = target.begin;
    layout_.update();
    point_ = anchor_ = clamp(point_);
}

attr_prop_index view::typing_format() const {
    const piece_table &table = document_.table();
    const fragments &frags = table.frags();
    attr_prop_index base = empty_attr_prop;
    const frag *at = frags.find(point_);
    const frag *before = frags.previous(at);
    if (frags.position_of(at) != point_)
        base = at->format();
    else if (before && before->kind() != frag_kind::strux)
        base = before->format();
    else if (at->kind() == frag_kind::text)
        base = at->format();
    if (pending_.empty())
        return base;
    auto &formats = document_.table().formats();
    const attr_prop merged =
        attr_prop(formats.get(base)).applied(format_op::add, {}, pending_);
    return formats.intern(merged);
}

void view::insert_text(std::u32string_view text) {
    if (text.empty())
        return;
    const scoped_glob glob(document_);
    delete_selection();
    document_.table().insert_text(point_, text, typing_format());
    point_ = anchor_ = point_ + static_cast<position>(text.size());
    pending_.clear();
    changed();
}

void view::insert_paragraph() {
    const scoped_glob glob(document_);
    delete_selection();
    const block_layout *block = layout_.block_at(point_);
    attr_prop_index format = block->strux()->format();
    // At a paragraph's end the style's successor takes over: Enter
    // after a heading starts a Normal paragraph.
    if (point_ == layout_.block_end(*block)) {
        auto &formats = document_.table().formats();
        const auto name = formats.get(format).attribute("style");
        const style *current =
            name ? document_.find_style(*name) : nullptr;
        if (current && document_.find_style(current->followed_by))
            format = formats.intern(attr_prop(
                {{"style", current->followed_by}}, {}));
    }
    document_.table().insert_strux(point_, strux_kind::block, format);
    point_ = anchor_ = point_ + 1;
    changed();
}

void view::insert_break(object_kind kind) {
    const scoped_glob glob(document_);
    delete_selection();
    document_.table().insert_object(point_, kind, empty_attr_prop);
    point_ = anchor_ = point_ + 1;
    changed();
}

void view::insert_image(std::string_view mime_type,
                        std::vector<std::uint8_t> bytes, util::lu width,
                        util::lu height) {
    const scoped_glob glob(document_);
    delete_selection();
    std::string name = "image-" + std::to_string(document_.data().size());
    while (document_.data().contains(name))
        name += "-";
    document_.set_data(name, {std::string(mime_type), std::move(bytes)});
    auto &formats = document_.table().formats();
    const attr_prop_index format = formats.intern(attr_prop(
        {{"dataid", name}}, {{"width", util::format_lu(width)},
                             {"height", util::format_lu(height)}}));
    document_.table().insert_object(point_, object_kind::image, format);
    point_ = anchor_ = point_ + 1;
    changed();
}

void view::delete_backward() {
    if (has_selection()) {
        const scoped_glob glob(document_);
        delete_selection();
        changed();
        return;
    }
    const position target = step_left(point_);
    if (target == point_)
        return;
    const block_layout *block = layout_.block_at(point_);
    const position end =
        point_ == layout_.block_start(*block)
            ? layout_.block_start(*block) // Joins with the previous block.
            : point_;
    document_.table().delete_range(target, end);
    point_ = anchor_ = target;
    changed();
}

void view::delete_forward() {
    if (has_selection()) {
        delete_backward();
        return;
    }
    const position target = step_right(point_);
    if (target == point_)
        return;
    document_.table().delete_range(point_, target);
    changed();
    point_ = anchor_ = clamp(point_);
}

void view::delete_word_backward() {
    if (!has_selection())
        anchor_ = word_left(point_);
    delete_backward();
}

void view::delete_word_forward() {
    if (!has_selection())
        anchor_ = word_right(point_);
    delete_backward();
}

bool view::undo() {
    if (!document_.table().undo())
        return false;
    changed();
    point_ = anchor_ = clamp(last_change_);
    return true;
}

bool view::redo() {
    if (!document_.table().redo())
        return false;
    changed();
    point_ = anchor_ = clamp(last_change_);
    return true;
}

const attr_prop *view::section_format(position pos) const {
    const piece_table &table = document_.table();
    const frag_strux *section = table.strux_at(pos, strux_kind::section);
    return section ? &table.formats().get(section->format()) : nullptr;
}

std::string view::char_property(std::string_view name) const {
    if (const auto found = pending_.find(name); found != pending_.end())
        return found->second;
    const piece_table &table = document_.table();
    const block_layout *block = layout_.block_at(point_);
    const attr_prop *span = nullptr;
    const position pos = selection().begin;
    const fragments &frags = table.frags();
    const frag *at = frags.find(pos);
    const frag *before = frags.previous(at);
    if (!has_selection() && frags.position_of(at) == pos && before &&
        before->kind() != frag_kind::strux)
        at = before;
    if (at->kind() == frag_kind::text || at->kind() == frag_kind::object)
        span = &table.formats().get(at->format());
    return std::string(document_.property(
        name, span, &table.formats().get(block->strux()->format()),
        section_format(pos)));
}

std::string view::block_property(std::string_view name) const {
    return std::string(document_.block_property(
        name, *layout_.block_at(point_)->strux()));
}

std::string view::block_style() const {
    const auto style =
        document_.table()
            .formats()
            .get(layout_.block_at(point_)->strux()->format())
            .attribute("style");
    return std::string(style.value_or("Normal"));
}

void view::set_char_format(const name_values &properties) {
    if (!has_selection()) {
        for (const auto &[name, value] : properties)
            pending_.insert_or_assign(name, value);
        return;
    }
    const range target = selection();
    document_.table().change_span_format(target.begin, target.end,
                                         format_op::add, {}, properties);
    changed();
}

void view::toggle_char_format(std::string_view name, std::string_view on,
                              std::string_view off) {
    const bool active = char_property(name) == on;
    set_char_format({{std::string(name), std::string(active ? off : on)}});
}

void view::toggle_decoration(std::string_view word) {
    std::string value = char_property("text-decoration");
    if (value == "none")
        value.clear();
    const std::size_t found = value.find(word);
    if (found != std::string::npos) {
        value.erase(found, word.size());
    } else {
        if (!value.empty())
            value.push_back(' ');
        value += word;
    }
    while (!value.empty() && value.front() == ' ')
        value.erase(0, 1);
    while (!value.empty() && value.back() == ' ')
        value.pop_back();
    set_char_format({{"text-decoration", value.empty() ? "none" : value}});
}

void view::set_block_format(const name_values &properties) {
    const range target = selection();
    document_.table().change_strux_format(target.begin, target.end,
                                          strux_kind::block, format_op::add,
                                          {}, properties);
    changed();
}

void view::set_section_format(const name_values &properties) {
    document_.table().change_strux_format(point_, point_ + 1,
                                          strux_kind::section, format_op::add,
                                          {}, properties);
    changed();
}

std::string view::section_property(std::string_view name) const {
    return std::string(document_.property(name, nullptr, nullptr,
                                          section_format(point_)));
}

void view::set_block_style(const std::string &name) {
    if (!document_.find_style(name))
        return;
    const range target = selection();
    document_.table().change_strux_format(
        target.begin, target.end, strux_kind::block, format_op::add,
        {{"style", name}}, {});
    changed();
}

} // namespace edit
