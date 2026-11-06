//
// The ordered fragment collection: a red-black tree indexed by document
// position through each node's left-subtree length.
//
// Every operation a keystroke needs is O(log n) in the fragment count,
// and fragment pointers stay valid until the fragment is removed.
//
// Derived from AbiWord src/text/ptbl/xp/pf_Fragments.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef DOC_FRAGMENTS_H
#define DOC_FRAGMENTS_H

#include <cstddef>
#include <cstdint>
#include <memory>

#include "doc/frag.h"

namespace doc {

// Owns fragments in document order.
class fragments {
public:
    fragments();
    ~fragments();

    fragments(const fragments &) = delete;
    fragments &operator=(const fragments &) = delete;

    // Return the number of fragments.
    std::size_t size() const { return size_; }

    // Return the total length of all fragments.
    position document_size() const { return document_size_; }

    // Return the first fragment, or null when empty.
    frag *first() const;

    // Return the last fragment, or null when empty.
    frag *last() const;

    // Return the fragment after one, or null at the end.
    frag *next(const frag *item) const;

    // Return the fragment before one, or null at the start.
    frag *previous(const frag *item) const;

    //
    // Return the fragment containing a position.
    //
    // Notes:
    //      A position at or past the end returns the last fragment.
    //
    frag *find(position pos) const;

    // Return the position of a fragment's first character.
    position position_of(const frag *item) const;

    // Append a fragment and return it.
    frag *push_back(std::unique_ptr<frag> item);

    // Insert a fragment after another one and return it.
    frag *insert_after(frag *place, std::unique_ptr<frag> item);

    // Insert a fragment before another one and return it.
    frag *insert_before(frag *place, std::unique_ptr<frag> item);

    // Unlink a fragment and hand back its ownership.
    std::unique_ptr<frag> remove(frag *item);

    // Change a fragment's length, keeping the index correct.
    void set_length(frag *item, std::uint32_t length);

    //
    // Verify tree shape, colors and order statistics.
    //
    // Returns:
    //      Whether every red-black and size invariant holds. Tests
    //      call this after randomized edit sequences.
    //
    bool check_invariants() const;

private:
    void rotate_left(frag *x);
    void rotate_right(frag *y);
    void insert_fixup(frag *item);
    void erase_fixup(frag *x, frag *parent);
    void link_after(frag *place, frag *item);
    void transplant(frag *from, frag *to);
    void adjust_ancestors(frag *item, std::int64_t delta);
    frag *subtree_first(frag *node) const;
    frag *subtree_last(frag *node) const;
    void destroy(frag *node);

    frag *root_ = nullptr;
    std::size_t size_ = 0;
    position document_size_ = 0;
};

} // namespace doc

#endif
