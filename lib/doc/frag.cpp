//
// Fragment constructors. Length changes go through doc::fragments so
// the tree's order statistics stay correct.
//
// Derived from AbiWord src/text/ptbl/xp/pf_Frag.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "doc/frag.h"

namespace doc {

frag::frag(frag_kind kind, std::uint32_t length, attr_prop_index format)
    : kind_(kind), length_(length), format_(format) {
}

frag_text::frag_text(std::uint32_t offset, std::uint32_t length,
                     attr_prop_index format)
    : frag(frag_kind::text, length, format), offset_(offset) {
}

frag_strux::frag_strux(strux_kind type, attr_prop_index format)
    : frag(frag_kind::strux, 1, format), type_(type) {
}

frag_object::frag_object(object_kind type, attr_prop_index format)
    : frag(frag_kind::object, 1, format), type_(type) {
}

frag_end_of_doc::frag_end_of_doc()
    : frag(frag_kind::end_of_doc, 0, empty_attr_prop) {
}

} // namespace doc
