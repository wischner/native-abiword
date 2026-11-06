//
// The primitive fragment surgery behind piece_table. A primitive works
// purely from its change record, because undo replays records long
// after the fragments that produced them have gone.
//
// Derived from AbiWord src/text/ptbl/xp/pt_PT_InsertSpan.cpp,
// pt_PT_DeleteSpan.cpp, pt_PT_ChangeSpan.cpp, pt_PT_InsertStrux.cpp,
// pt_PT_DeleteStrux.cpp and pt_PT_ChangeStrux.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "doc/piece_table.h"

namespace doc {

frag *piece_table::split_at(position pos) {
    frag *item = fragments_.find(pos);
    const position start = fragments_.position_of(item);
    if (start == pos || item->kind() != frag_kind::text)
        return item;
    auto *text = static_cast<frag_text *>(item);
    const std::uint32_t head = pos - start;
    auto tail = std::make_unique<frag_text>(
        text->offset() + head, text->length() - head, text->format());
    fragments_.set_length(text, head);
    return fragments_.insert_after(text, std::move(tail));
}

void piece_table::coalesce(frag *item) {
    if (item->kind() != frag_kind::text)
        return;
    // Absorb the following fragment, then let the previous one absorb
    // this one, so a single call removes up to two seams.
    const auto try_merge = [this](frag *left_item) {
        if (!left_item || left_item->kind() != frag_kind::text)
            return false;
        frag *right_item = fragments_.next(left_item);
        if (!right_item || right_item->kind() != frag_kind::text ||
            right_item->format() != left_item->format())
            return false;
        auto *left = static_cast<frag_text *>(left_item);
        auto *right = static_cast<frag_text *>(right_item);
        if (left->offset() + left->length() != right->offset())
            return false;
        const std::uint32_t merged = left->length() + right->length();
        fragments_.remove(right);
        fragments_.set_length(left, merged);
        return true;
    };
    try_merge(item);
    try_merge(fragments_.previous(item));
}

void piece_table::raw_insert_span(const change_record &record) {
    frag *place = split_at(record.pos);
    auto item = std::make_unique<frag_text>(record.buffer_offset,
                                            record.length, record.format);
    coalesce(fragments_.insert_before(place, std::move(item)));
}

void piece_table::raw_delete_span(const change_record &record) {
    // Typing coalesces records, so one record can span fragments that
    // were split by later edits since undone.
    std::uint32_t remaining = record.length;
    while (remaining > 0) {
        split_at(record.pos + remaining);
        frag *item = split_at(record.pos);
        frag *before = fragments_.previous(item);
        remaining -= item->length();
        fragments_.remove(item);
        if (before)
            coalesce(before);
    }
}

void piece_table::raw_change_span(const change_record &record) {
    split_at(record.pos);
    split_at(record.pos + record.length);
    position pos = record.pos;
    const position end = record.pos + record.length;
    while (pos < end) {
        frag *item = fragments_.find(pos);
        pos += item->length();
        item->set_format(record.format);
    }
    coalesce(fragments_.find(record.pos));
    if (end < end_position())
        coalesce(fragments_.find(end));
}

void piece_table::raw_insert_single(const change_record &record) {
    frag *place = split_at(record.pos);
    std::unique_ptr<frag> item;
    if (record.type == change_type::insert_strux)
        item = std::make_unique<frag_strux>(record.strux, record.format);
    else
        item = std::make_unique<frag_object>(record.object, record.format);
    fragments_.insert_before(place, std::move(item));
}

void piece_table::raw_delete_single(const change_record &record) {
    frag *item = fragments_.find(record.pos);
    if (item->kind() == frag_kind::strux && !loading_)
        for (change_listener *listener : listeners_)
            listener->on_strux_removed(*static_cast<frag_strux *>(item));
    frag *before = fragments_.previous(item);
    fragments_.remove(item);
    if (before)
        coalesce(before);
}

void piece_table::raw_change_strux(const change_record &record) {
    fragments_.find(record.pos)->set_format(record.format);
}

void piece_table::apply(const change_record &record) {
    switch (record.type) {
    case change_type::insert_span:
        raw_insert_span(record);
        break;
    case change_type::delete_span:
        raw_delete_span(record);
        break;
    case change_type::change_span:
        raw_change_span(record);
        break;
    case change_type::insert_strux:
    case change_type::insert_object:
        raw_insert_single(record);
        break;
    case change_type::delete_strux:
    case change_type::delete_object:
        raw_delete_single(record);
        break;
    case change_type::change_strux:
        raw_change_strux(record);
        break;
    }
    if (!loading_)
        for (change_listener *listener : listeners_)
            listener->on_change(record);
}

} // namespace doc
