//
// Change records and the undo history. Every edit is expressed as a
// sequence of primitive records. Inverting a record swaps its meaning,
// so undo is simply replaying inverses, and a layout listening to the
// document needs no separate undo path.
//
// Derived from AbiWord src/text/ptbl/xp/px_ChangeRecord.h and
// px_ChangeHistory.cpp, Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef DOC_CHANGE_RECORD_H
#define DOC_CHANGE_RECORD_H

#include <cstdint>
#include <vector>

#include "doc/frag.h"

namespace doc {

// The primitive operations of a piece table.
enum class change_type {
    insert_span,
    delete_span,
    change_span,
    insert_strux,
    delete_strux,
    change_strux,
    insert_object,
    delete_object
};

// One primitive edit.
struct change_record {
    change_type type = change_type::insert_span;
    position pos = 0;

    // Characters affected; 1 for strux and object records.
    std::uint32_t length = 1;

    // Character buffer offset of span records.
    std::uint32_t buffer_offset = 0;

    // Format after this record applies.
    attr_prop_index format = empty_attr_prop;

    // Format before this record applied; change records only.
    attr_prop_index old_format = empty_attr_prop;

    strux_kind strux = strux_kind::block;
    object_kind object = object_kind::image;

    // Records sharing a glob undo as one user action.
    std::uint32_t glob = 0;
};

// Return the record that exactly reverses another.
change_record inverted(const change_record &record);

// The undoable history of a document.
class change_history {
public:
    // Open a user action; nested opens join the outermost one.
    void begin_glob();

    // Close a user action.
    void end_glob();

    //
    // Record an applied change.
    //
    // Notes:
    //      Discards any redoable records. Contiguous typing coalesces
    //      into one record, so undo removes a typed burst at once.
    //
    void push(change_record record);

    // Report whether an undoable record exists.
    bool can_undo() const { return top_ > 0; }

    // Report whether a redoable record exists.
    bool can_redo() const { return top_ < records_.size(); }

    //
    // Return the records of the most recent user action, newest first,
    // and step the history back over them.
    //
    std::vector<change_record> take_undo();

    //
    // Return the records of the next undone action, oldest first, and
    // step the history forward over them.
    //
    std::vector<change_record> take_redo();

    // Mark the current state as the one on disk.
    void mark_saved();

    // Report whether the state differs from the one on disk.
    bool is_dirty() const;

    // Forget all history, e.g. after loading a file.
    void clear();

private:
    std::vector<change_record> records_;
    std::size_t top_ = 0;

    // Value of top_ at save; npos once that state became unreachable.
    static constexpr std::size_t never_saved = static_cast<std::size_t>(-1);
    std::size_t saved_top_ = 0;

    std::uint32_t next_glob_ = 1;
    std::uint32_t open_glob_ = 0;
    int glob_depth_ = 0;
};

} // namespace doc

#endif
