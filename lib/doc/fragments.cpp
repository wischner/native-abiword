//
// Red-black tree mechanics. The algorithm is the textbook one AbiWord
// uses; a rotation preserves order statistics because only the two
// rotated nodes change which subtree they sit in.
//
// Null stands for AbiWord's shared black leaf. Erase therefore tracks
// the fixup position's parent explicitly.
//
// Derived from AbiWord src/text/ptbl/xp/pf_Fragments.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "doc/fragments.h"

#include <utility>

namespace doc {
fragments::fragments() = default;

fragments::~fragments() {
    destroy(root_);
}

void fragments::destroy(frag *node) {
    // Recurse on one side only: tree depth is logarithmic, but keeping
    // the right spine iterative costs nothing.
    while (node) {
        destroy(node->left_);
        frag *right = node->right_;
        delete node;
        node = right;
    }
}

frag *fragments::subtree_first(frag *node) const {
    while (node && node->left_)
        node = node->left_;
    return node;
}

frag *fragments::subtree_last(frag *node) const {
    while (node && node->right_)
        node = node->right_;
    return node;
}

frag *fragments::first() const {
    return subtree_first(root_);
}

frag *fragments::last() const {
    return subtree_last(root_);
}

frag *fragments::next(const frag *item) const {
    if (item->right_)
        return subtree_first(item->right_);
    const frag *node = item;
    frag *parent = node->parent_;
    while (parent && parent->right_ == node) {
        node = parent;
        parent = parent->parent_;
    }
    return parent;
}

frag *fragments::previous(const frag *item) const {
    if (item->left_)
        return subtree_last(item->left_);
    const frag *node = item;
    frag *parent = node->parent_;
    while (parent && parent->left_ == node) {
        node = parent;
        parent = parent->parent_;
    }
    return parent;
}

frag *fragments::find(position pos) const {
    frag *node = root_;
    frag *best = nullptr;
    while (node) {
        best = node;
        if (pos < node->left_length_) {
            node = node->left_;
        } else if (pos < node->left_length_ + node->length_) {
            return node;
        } else {
            pos -= node->left_length_ + node->length_;
            node = node->right_;
        }
    }
    return best ? subtree_last(root_) : nullptr;
}

position fragments::position_of(const frag *item) const {
    position result = item->left_length_;
    for (const frag *node = item; node->parent_; node = node->parent_)
        if (node->parent_->right_ == node)
            result += node->parent_->left_length_ + node->parent_->length_;
    return result;
}

void fragments::adjust_ancestors(frag *item, std::int64_t delta) {
    for (frag *node = item; node->parent_; node = node->parent_) {
        if (node->parent_->left_ == node)
            node->parent_->left_length_ = static_cast<std::uint32_t>(
                node->parent_->left_length_ + delta);
    }
}

void fragments::set_length(frag *item, std::uint32_t length) {
    const std::int64_t delta =
        static_cast<std::int64_t>(length) - item->length_;
    item->length_ = length;
    adjust_ancestors(item, delta);
    document_size_ = static_cast<position>(document_size_ + delta);
}

void fragments::rotate_left(frag *x) {
    frag *y = x->right_;
    x->right_ = y->left_;
    if (y->left_)
        y->left_->parent_ = x;
    y->parent_ = x->parent_;
    if (!x->parent_)
        root_ = y;
    else if (x == x->parent_->left_)
        x->parent_->left_ = y;
    else
        x->parent_->right_ = y;
    y->left_ = x;
    x->parent_ = y;
    y->left_length_ += x->left_length_ + x->length_;
}

void fragments::rotate_right(frag *y) {
    frag *x = y->left_;
    y->left_ = x->right_;
    if (x->right_)
        x->right_->parent_ = y;
    x->parent_ = y->parent_;
    if (!y->parent_)
        root_ = x;
    else if (y == y->parent_->right_)
        y->parent_->right_ = x;
    else
        y->parent_->left_ = x;
    x->right_ = y;
    y->parent_ = x;
    y->left_length_ -= x->left_length_ + x->length_;
}

void fragments::insert_fixup(frag *item) {
    while (item->parent_ && item->parent_->red_) {
        frag *parent = item->parent_;
        frag *grandparent = parent->parent_;
        const bool on_left = parent == grandparent->left_;
        frag *uncle = on_left ? grandparent->right_ : grandparent->left_;
        if (uncle && uncle->red_) {
            parent->red_ = false;
            uncle->red_ = false;
            grandparent->red_ = true;
            item = grandparent;
            continue;
        }
        if (item == (on_left ? parent->right_ : parent->left_)) {
            item = parent;
            if (on_left)
                rotate_left(item);
            else
                rotate_right(item);
            parent = item->parent_;
        }
        parent->red_ = false;
        grandparent->red_ = true;
        if (on_left)
            rotate_right(grandparent);
        else
            rotate_left(grandparent);
    }
    root_->red_ = false;
}

void fragments::link_after(frag *place, frag *item) {
    item->left_ = item->right_ = nullptr;
    item->left_length_ = 0;
    item->red_ = true;
    if (!place) {
        // Insert at the very front.
        frag *head = subtree_first(root_);
        item->parent_ = head;
        if (head)
            head->left_ = item;
        else
            root_ = item;
    } else if (!place->right_) {
        place->right_ = item;
        item->parent_ = place;
    } else {
        frag *successor = subtree_first(place->right_);
        successor->left_ = item;
        item->parent_ = successor;
    }
    adjust_ancestors(item, item->length_);
    document_size_ += item->length_;
    ++size_;
    insert_fixup(item);
}

frag *fragments::push_back(std::unique_ptr<frag> item) {
    frag *raw = item.release();
    link_after(last(), raw);
    return raw;
}

frag *fragments::insert_after(frag *place, std::unique_ptr<frag> item) {
    frag *raw = item.release();
    link_after(place, raw);
    return raw;
}

frag *fragments::insert_before(frag *place, std::unique_ptr<frag> item) {
    frag *raw = item.release();
    link_after(previous(place), raw);
    return raw;
}

void fragments::transplant(frag *from, frag *to) {
    if (!from->parent_)
        root_ = to;
    else if (from == from->parent_->left_)
        from->parent_->left_ = to;
    else
        from->parent_->right_ = to;
    if (to)
        to->parent_ = from->parent_;
}

void fragments::erase_fixup(frag *x, frag *parent) {
    while (x != root_ && !(x && x->red_)) {
        const bool on_left = x == parent->left_;
        frag *sibling = on_left ? parent->right_ : parent->left_;
        if (sibling->red_) {
            sibling->red_ = false;
            parent->red_ = true;
            if (on_left)
                rotate_left(parent);
            else
                rotate_right(parent);
            sibling = on_left ? parent->right_ : parent->left_;
        }
        frag *near = on_left ? sibling->left_ : sibling->right_;
        frag *far = on_left ? sibling->right_ : sibling->left_;
        if (!(near && near->red_) && !(far && far->red_)) {
            sibling->red_ = true;
            x = parent;
            parent = x->parent_;
            continue;
        }
        if (!(far && far->red_)) {
            near->red_ = false;
            sibling->red_ = true;
            if (on_left)
                rotate_right(sibling);
            else
                rotate_left(sibling);
            sibling = on_left ? parent->right_ : parent->left_;
            far = on_left ? sibling->right_ : sibling->left_;
        }
        sibling->red_ = parent->red_;
        parent->red_ = false;
        far->red_ = false;
        if (on_left)
            rotate_left(parent);
        else
            rotate_right(parent);
        x = root_;
        parent = nullptr;
    }
    if (x)
        x->red_ = false;
}

std::unique_ptr<frag> fragments::remove(frag *item) {
    // Take the item's length out of the index first; from here on it
    // is a zero-length node and no ancestor counts it.
    adjust_ancestors(item, -static_cast<std::int64_t>(item->length_));
    document_size_ -= item->length_;
    --size_;

    frag *fix_node = nullptr;
    frag *fix_parent = nullptr;
    bool removed_black = !item->red_;

    if (!item->left_ || !item->right_) {
        fix_node = item->left_ ? item->left_ : item->right_;
        fix_parent = item->parent_;
        transplant(item, fix_node);
    } else {
        frag *successor = subtree_first(item->right_);
        removed_black = !successor->red_;
        // The successor leaves the right subtree: nodes whose left
        // subtree held it stop counting it.
        for (frag *node = successor; node != item->right_;
             node = node->parent_)
            if (node->parent_->left_ == node)
                node->parent_->left_length_ -= successor->length_;
        fix_node = successor->right_;
        if (successor->parent_ == item) {
            fix_parent = successor;
        } else {
            fix_parent = successor->parent_;
            transplant(successor, successor->right_);
            successor->right_ = item->right_;
            successor->right_->parent_ = successor;
        }
        transplant(item, successor);
        successor->left_ = item->left_;
        successor->left_->parent_ = successor;
        successor->red_ = item->red_;
        successor->left_length_ = item->left_length_;
    }
    if (removed_black)
        erase_fixup(fix_node, fix_parent);
    item->left_ = item->right_ = item->parent_ = nullptr;
    return std::unique_ptr<frag>(item);
}

namespace {

// Validate one subtree, returning its black height or -1.
struct subtree_report {
    int black_height;
    std::uint64_t length;
};

} // namespace

bool fragments::check_invariants() const {
    if (root_ && root_->red_)
        return false;
    bool valid = true;
    std::size_t count = 0;
    const auto visit = [&](const auto &self,
                           const frag *node) -> subtree_report {
        if (!node)
            return {1, 0};
        ++count;
        if (node->red_ && ((node->left_ && node->left_->red_) ||
                           (node->right_ && node->right_->red_)))
            valid = false;
        if ((node->left_ && node->left_->parent_ != node) ||
            (node->right_ && node->right_->parent_ != node))
            valid = false;
        const subtree_report left = self(self, node->left_);
        const subtree_report right = self(self, node->right_);
        if (left.black_height != right.black_height ||
            left.length != node->left_length_)
            valid = false;
        return {left.black_height + (node->red_ ? 0 : 1),
                left.length + node->length_ + right.length};
    };
    const subtree_report whole = visit(visit, root_);
    return valid && count == size_ && whole.length == document_size_;
}

} // namespace doc
