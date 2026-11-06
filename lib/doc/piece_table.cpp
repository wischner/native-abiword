//
// Public piece table operations. Each edit here is a planner: it walks
// the fragments, then expresses the edit as primitive change records
// which piece_table_apply.cpp carries out. Undo reuses those same
// primitives, so forward and backward edits cannot drift apart.
//
// Derived from AbiWord src/text/ptbl/xp/pt_PieceTable.cpp and the
// pt_PT_*.cpp edit routines, Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "doc/piece_table.h"

#include <algorithm>
#include <stdexcept>

namespace doc {

piece_table::piece_table() {
    fragments_.push_back(std::make_unique<frag_end_of_doc>());
}

position piece_table::end_position() const {
    return fragments_.document_size();
}

void piece_table::add_listener(change_listener *listener) {
    listeners_.push_back(listener);
}

void piece_table::remove_listener(change_listener *listener) {
    std::erase(listeners_, listener);
}

void piece_table::commit(const change_record &record) {
    apply(record);
    if (!loading_)
        history_.push(record);
}

void piece_table::insert_text(position pos, std::u32string_view text,
                              attr_prop_index format) {
    if (text.empty())
        return;
    change_record record;
    record.type = change_type::insert_span;
    record.pos = pos;
    record.length = static_cast<std::uint32_t>(text.size());
    record.buffer_offset = static_cast<std::uint32_t>(buffer_.size());
    record.format = format;
    buffer_.append(text);
    commit(record);
}

void piece_table::insert_strux(position pos, strux_kind type,
                               attr_prop_index format) {
    change_record record;
    record.type = change_type::insert_strux;
    record.pos = pos;
    record.strux = type;
    record.format = format;
    commit(record);
}

void piece_table::insert_object(position pos, object_kind type,
                                attr_prop_index format) {
    change_record record;
    record.type = change_type::insert_object;
    record.pos = pos;
    record.object = type;
    record.format = format;
    commit(record);
}

void piece_table::delete_range(position begin, position end) {
    end = std::min(end, end_position());
    if (begin >= end)
        return;
    history_.begin_glob();
    // The first section and its first block anchor the document.
    begin = std::max<position>(begin, 2);
    position remaining = end > begin ? end - begin : 0;
    while (remaining > 0) {
        // A deletion can coalesce its neighbors across the range ends,
        // so the boundaries are re-established on every step.
        split_at(begin + remaining);
        const frag *item = split_at(begin);
        change_record record;
        record.pos = begin;
        record.format = item->format();
        record.length = item->length();
        switch (item->kind()) {
        case frag_kind::text:
            record.type = change_type::delete_span;
            record.buffer_offset =
                static_cast<const frag_text *>(item)->offset();
            break;
        case frag_kind::strux:
            record.type = change_type::delete_strux;
            record.strux = static_cast<const frag_strux *>(item)->type();
            break;
        case frag_kind::object:
            record.type = change_type::delete_object;
            record.object =
                static_cast<const frag_object *>(item)->type();
            break;
        case frag_kind::end_of_doc:
            history_.end_glob();
            return;
        }
        remaining -= record.length;
        commit(record);
    }
    history_.end_glob();
}

void piece_table::change_span_format(position begin, position end,
                                     format_op op,
                                     const name_values &attributes,
                                     const name_values &properties) {
    end = std::min(end, end_position());
    if (begin >= end)
        return;
    history_.begin_glob();
    position pos = begin;
    while (pos < end) {
        // Applying a change coalesces fragments, possibly across the
        // range ends, so the boundaries are re-established each step.
        split_at(end);
        const frag *item = split_at(pos);
        const std::uint32_t length = item->length();
        if (item->kind() == frag_kind::text ||
            item->kind() == frag_kind::object) {
            const attr_prop_index changed = formats_.intern(
                formats_.get(item->format())
                    .applied(op, attributes, properties));
            if (changed != item->format()) {
                change_record record;
                record.type = change_type::change_span;
                record.pos = pos;
                record.length = length;
                record.format = changed;
                record.old_format = item->format();
                commit(record);
            }
        }
        pos += length;
    }
    history_.end_glob();
}

void piece_table::change_strux_format(position begin, position end,
                                      strux_kind type, format_op op,
                                      const name_values &attributes,
                                      const name_values &properties) {
    history_.begin_glob();
    const frag *item = strux_at(begin, type);
    while (item && item->kind() != frag_kind::end_of_doc) {
        const position pos = fragments_.position_of(item);
        if (pos >= end && pos != fragments_.position_of(
                                     strux_at(begin, type)))
            break;
        if (item->kind() == frag_kind::strux &&
            static_cast<const frag_strux *>(item)->type() == type) {
            const attr_prop_index changed = formats_.intern(
                formats_.get(item->format())
                    .applied(op, attributes, properties));
            if (changed != item->format()) {
                change_record record;
                record.type = change_type::change_strux;
                record.pos = pos;
                record.strux = type;
                record.format = changed;
                record.old_format = item->format();
                commit(record);
            }
        }
        item = fragments_.next(item);
    }
    history_.end_glob();
}

void piece_table::reset() {
    for (frag *item = fragments_.first();
         item && item->kind() != frag_kind::end_of_doc;
         item = fragments_.first())
        fragments_.remove(item);
    buffer_.clear();
    history_.clear();
}

std::u32string_view piece_table::text_of(const frag_text &item) const {
    return std::u32string_view(buffer_).substr(item.offset(),
                                               item.length());
}

frag_strux *piece_table::strux_at(position pos, strux_kind type) const {
    for (frag *item = fragments_.find(pos); item;
         item = fragments_.previous(item)) {
        if (item->kind() == frag_kind::strux) {
            auto *strux = static_cast<frag_strux *>(item);
            if (strux->type() == type)
                return strux;
        }
    }
    return nullptr;
}

std::u32string piece_table::text_in(position begin, position end) const {
    std::u32string result;
    end = std::min(end, end_position());
    if (begin >= end)
        return result;
    const frag *item = fragments_.find(begin);
    position start = fragments_.position_of(item);
    while (item && start < end) {
        const std::uint32_t skip = begin > start ? begin - start : 0;
        const std::uint32_t take =
            std::min(item->length(), end - start) - skip;
        switch (item->kind()) {
        case frag_kind::text:
            result.append(text_of(*static_cast<const frag_text *>(item))
                              .substr(skip, take));
            break;
        case frag_kind::strux:
            if (static_cast<const frag_strux *>(item)->type() ==
                    strux_kind::block &&
                start >= begin && !result.empty())
                result.push_back(U'\n');
            break;
        case frag_kind::object:
            if (static_cast<const frag_object *>(item)->type() ==
                object_kind::forced_line_break)
                result.push_back(U'\n');
            break;
        case frag_kind::end_of_doc:
            break;
        }
        start += item->length();
        item = fragments_.next(item);
    }
    return result;
}

bool piece_table::undo() {
    const std::vector<change_record> records = history_.take_undo();
    for (const change_record &record : records)
        apply(inverted(record));
    return !records.empty();
}

bool piece_table::redo() {
    const std::vector<change_record> records = history_.take_redo();
    for (const change_record &record : records)
        apply(record);
    return !records.empty();
}

} // namespace doc
