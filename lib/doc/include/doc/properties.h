//
// Definitions of the formatting properties a document can carry.
//
// Derived from AbiWord src/text/ptbl/xp/pp_Property.h,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef DOC_PROPERTIES_H
#define DOC_PROPERTIES_H

#include <string_view>

namespace doc {

// One property's name, last-resort value and inheritance.
struct property_definition {
    std::string_view name;
    std::string_view initial;
    bool inherits;
};

// Return a property's definition, or null for an unknown name.
const property_definition *find_property(std::string_view name);

} // namespace doc

#endif
