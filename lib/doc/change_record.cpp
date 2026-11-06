//
// Inverts change records and steps through the undo history. AbiWord's
// history also reconciles remote collaboration changes; this port has
// no collaboration, so a stack with an undo position is enough.
//
// Derived from AbiWord src/text/ptbl/xp/px_ChangeHistory.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "doc/change_record.h"

#include <utility>

namespace doc {
namespace {

// Report whether a typing record extends the previous one.
bool continues_typing(const change_record &last,
                      const change_record &next) {
    return last.type == change_type::insert_span &&
           next.type == change_type::insert_span &&
           last.pos + last.length == next.pos &&
           last.buffer_offset + last.length == next.buffer_offset &&
           last.format == next.format;
}

} // namespace

change_record inverted(const change_record &record) {
    change_record result = record;
    switch (record.type) {
    case change_type::insert_span:
        result.type = change_type::delete_span;
        break;
    case change_type::delete_span:
        result.type = change_type::insert_span;
        break;
    case change_type::insert_strux:
        result.type = change_type::delete_strux;
        break;
    case change_type::delete_strux:
        result.type = change_type::insert_strux;
        break;
    case change_type::insert_object:
        result.type = change_type::delete_object;
        break;
    case change_type::delete_object:
        result.type = change_type::insert_object;
        break;
    case change_type::change_span:
    case change_type::change_strux:
        std::swap(result.format, result.old_format);
        break;
    }
    return result;
}

void change_history::begin_glob() {
    if (glob_depth_++ == 0)
        open_glob_ = next_glob_++;
}

void change_history::end_glob() {
    if (glob_depth_ > 0 && --glob_depth_ == 0)
        open_glob_ = 0;
}

void change_history::push(change_record record) {
    if (top_ < records_.size()) {
        records_.resize(top_);
        if (saved_top_ != never_saved && saved_top_ > top_)
            saved_top_ = never_saved;
    }
    const bool at_saved_point = saved_top_ == top_;
    if (!records_.empty() && !at_saved_point && open_glob_ == 0 &&
        continues_typing(records_.back(), record)) {
        records_.back().length += record.length;
        return;
    }
    record.glob = open_glob_ != 0 ? open_glob_ : next_glob_++;
    records_.push_back(record);
    top_ = records_.size();
}

std::vector<change_record> change_history::take_undo() {
    std::vector<change_record> result;
    if (top_ == 0)
        return result;
    const std::uint32_t glob = records_[top_ - 1].glob;
    while (top_ > 0 && records_[top_ - 1].glob == glob)
        result.push_back(records_[--top_]);
    return result;
}

std::vector<change_record> change_history::take_redo() {
    std::vector<change_record> result;
    if (top_ == records_.size())
        return result;
    const std::uint32_t glob = records_[top_].glob;
    while (top_ < records_.size() && records_[top_].glob == glob)
        result.push_back(records_[top_++]);
    return result;
}

void change_history::mark_saved() {
    saved_top_ = top_;
}

bool change_history::is_dirty() const {
    return saved_top_ != top_;
}

void change_history::clear() {
    records_.clear();
    top_ = 0;
    saved_top_ = 0;
}

} // namespace doc
