//
// The piece table: a document as an append-only character buffer plus
// an ordered list of fragments over it. Nothing is ever erased from
// the buffer, which is why undo needs only fragment bookkeeping.
//
// Position layout of a one-paragraph document holding "Hi":
//
//      0        1      2 3  4
//      section  block  H i  (end)
//
// Derived from AbiWord src/text/ptbl/xp/pt_PieceTable.cpp and the
// pt_PT_*.cpp edit routines, Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef DOC_PIECE_TABLE_H
#define DOC_PIECE_TABLE_H

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "doc/attr_prop.h"
#include "doc/change_record.h"
#include "doc/fragments.h"

namespace doc {

// Receives every primitive change after it applied.
class change_listener {
public:
    virtual ~change_listener() = default;

    // React to one applied primitive change.
    virtual void on_change(const change_record &record) = 0;

    //
    // React to a strux about to be destroyed.
    //
    // Notes:
    //      Called before removal, because a listener may key state on
    //      the fragment's address.
    //
    virtual void on_strux_removed(const frag_strux &strux) = 0;
};

// The editable text and structure of one document.
class piece_table {
public:
    piece_table();

    // Return the format table.
    attr_prop_table &formats() { return formats_; }

    // Return the format table.
    const attr_prop_table &formats() const { return formats_; }

    // Return the fragment list, read-only for clients.
    const fragments &frags() const { return fragments_; }

    // Return the undo history.
    change_history &history() { return history_; }

    // Return the position one past the last character.
    position end_position() const;

    // Register a listener; it must outlive its registration.
    void add_listener(change_listener *listener);

    // Unregister a listener.
    void remove_listener(change_listener *listener);

    //
    // Insert characters in one format.
    //
    // Notes:
    //      Bypasses history while loading; see set_loading.
    //
    void insert_text(position pos, std::u32string_view text,
                     attr_prop_index format);

    // Start a structural element at a position.
    void insert_strux(position pos, strux_kind type,
                      attr_prop_index format);

    // Insert one inline object at a position.
    void insert_object(position pos, object_kind type,
                       attr_prop_index format);

    //
    // Delete everything in a half-open position range.
    //
    // Notes:
    //      The first section and block always survive, so a document
    //      never loses the structure text needs.
    //
    void delete_range(position begin, position end);

    // Change the format of text and objects in a range.
    void change_span_format(position begin, position end, format_op op,
                            const name_values &attributes,
                            const name_values &properties);

    //
    // Change every strux of one kind touching a range.
    //
    // Notes:
    //      The strux owning the range start is included even when it
    //      lies before the range: selecting part of a paragraph
    //      formats the whole paragraph.
    //
    void change_strux_format(position begin, position end, strux_kind type,
                             format_op op, const name_values &attributes,
                             const name_values &properties);

    //
    // Append content while loading a file.
    //
    // Notes:
    //      Loading skips history and listeners: a freshly opened
    //      document has nothing to undo.
    //
    void set_loading(bool loading) { loading_ = loading; }

    // Drop all content and history; only a loader calls this.
    void reset();

    // Return the characters of a text fragment.
    std::u32string_view text_of(const frag_text &item) const;

    // Return the strux that owns a position, of the wanted kind.
    frag_strux *strux_at(position pos, strux_kind type) const;

    // Return the plain text of a range; a block boundary reads '\n'.
    std::u32string text_in(position begin, position end) const;

    // Undo the most recent user action; false when none exists.
    bool undo();

    // Redo the most recently undone action; false when none exists.
    bool redo();

private:
    // Apply one primitive, then notify and optionally record it.
    void apply(const change_record &record);
    void commit(const change_record &record);
    void raw_insert_span(const change_record &record);
    void raw_delete_span(const change_record &record);
    void raw_change_span(const change_record &record);
    void raw_insert_single(const change_record &record);
    void raw_delete_single(const change_record &record);
    void raw_change_strux(const change_record &record);

    // Ensure a fragment boundary at a position; return the fragment
    // starting there.
    frag *split_at(position pos);

    // Merge a text fragment into its neighbors where contiguous.
    void coalesce(frag *item);

    std::u32string buffer_;
    fragments fragments_;
    attr_prop_table formats_;
    change_history history_;
    std::vector<change_listener *> listeners_;
    bool loading_ = false;
};

} // namespace doc

#endif
