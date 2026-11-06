//
// Fragments, the pieces of a piece table. A text fragment refers to a
// range of the append-only character buffer; a strux fragment marks
// the start of a structural element and an object fragment holds one
// inline object. Each carries an interned attr_prop index.
//
// A fragment doubles as its own red-black tree node, so a held pointer
// finds its document position in logarithmic time.
//
// Derived from AbiWord src/text/ptbl/xp/pf_Frag.h, pf_Frag_Text.h,
// pf_Frag_Strux.h and pf_Frag_Object.h,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef DOC_FRAG_H
#define DOC_FRAG_H

#include <cstdint>

#include "doc/attr_prop.h"

namespace doc {

// An index into the document's position space.
using position = std::uint32_t;

// A fragment's concrete kind.
enum class frag_kind { text, strux, object, end_of_doc };

// A structural element's kind.
enum class strux_kind { section, block };

// An inline object's kind.
enum class object_kind { image, field, forced_line_break,
                         forced_column_break, forced_page_break };

// A piece of the document.
class frag {
public:
    virtual ~frag() = default;

    frag(const frag &) = delete;
    frag &operator=(const frag &) = delete;

    // Return this fragment's concrete kind.
    frag_kind kind() const { return kind_; }

    // Return how many document positions this fragment covers.
    std::uint32_t length() const { return length_; }

    // Return the interned formatting of this fragment.
    attr_prop_index format() const { return format_; }

    // Replace this fragment's formatting.
    void set_format(attr_prop_index format) { format_ = format; }

protected:
    frag(frag_kind kind, std::uint32_t length, attr_prop_index format);

private:
    friend class fragments;

    frag_kind kind_;
    std::uint32_t length_;
    attr_prop_index format_;

    // Tree links owned by doc::fragments.
    frag *left_ = nullptr;
    frag *right_ = nullptr;
    frag *parent_ = nullptr;
    bool red_ = true;

    // Total length of the left subtree, the order statistic.
    std::uint32_t left_length_ = 0;
};

// A run of characters sharing one format.
class frag_text final : public frag {
public:
    // Create a fragment over a range of the character buffer.
    frag_text(std::uint32_t offset, std::uint32_t length,
              attr_prop_index format);

    // Return the buffer offset of the first character.
    std::uint32_t offset() const { return offset_; }

    // Move the buffer offset of the first character.
    void set_offset(std::uint32_t offset) { offset_ = offset; }

private:
    std::uint32_t offset_;
};

// The start of a section or block; occupies one position.
class frag_strux final : public frag {
public:
    // Create a structural marker.
    frag_strux(strux_kind type, attr_prop_index format);

    // Return the kind of element this strux starts.
    strux_kind type() const { return type_; }

private:
    strux_kind type_;
};

// One inline object; occupies one position.
class frag_object final : public frag {
public:
    // Create an inline object.
    frag_object(object_kind type, attr_prop_index format);

    // Return the kind of object.
    object_kind type() const { return type_; }

private:
    object_kind type_;
};

// The sentinel after the last position; occupies none.
class frag_end_of_doc final : public frag {
public:
    frag_end_of_doc();
};

} // namespace doc

#endif
