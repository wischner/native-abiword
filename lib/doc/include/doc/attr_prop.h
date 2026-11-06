//
// Immutable attribute and property sets. An attribute is structural
// ("style", "dataid"); a property is formatting ("font-size"). Every
// fragment refers to one interned set by index, which is what makes
// undo cheap: a format change only swaps an index.
//
// Derived from AbiWord src/text/ptbl/xp/pp_AttrProp.cpp and
// pp_TableAttrProp.cpp, Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef DOC_ATTR_PROP_H
#define DOC_ATTR_PROP_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace doc {

// Name to value, ordered so equal sets compare and hash equal.
using name_values = std::map<std::string, std::string, std::less<>>;

// Index of an interned set in an attr_prop_table.
using attr_prop_index = std::uint32_t;

// The empty set, always present at index 0.
inline constexpr attr_prop_index empty_attr_prop = 0;

//
// How a change combines with existing formatting.
//
// Mirrors AbiWord's PTChangeFmt.
//
enum class format_op {
    add,     // Merge the supplied names, replacing equal names.
    remove,  // Remove the supplied names.
    replace  // Discard existing names and use the supplied ones.
};

// One value set: attributes plus properties.
class attr_prop {
public:
    attr_prop() = default;

    // Construct from attributes and properties.
    attr_prop(name_values attributes, name_values properties);

    // Return an attribute's value, or nothing when unset.
    std::optional<std::string_view>
    attribute(std::string_view name) const;

    // Return a property's value, or nothing when unset.
    std::optional<std::string_view> property(std::string_view name) const;

    // Return all attributes.
    const name_values &attributes() const { return attributes_; }

    // Return all properties.
    const name_values &properties() const { return properties_; }

    //
    // Return a copy with a formatting change applied.
    //
    // Parameters:
    //      op          - How the change combines with this set.
    //      attributes  - Attribute changes.
    //      properties  - Property changes.
    //
    attr_prop applied(format_op op, const name_values &attributes,
                      const name_values &properties) const;

    friend bool operator==(const attr_prop &, const attr_prop &) = default;

private:
    name_values attributes_;
    name_values properties_;
};

//
// Parse an AbiWord property string.
//
// Parameters:
//      text        - Text such as "font-size:12pt; color:ff0000".
//
// Returns:
//      The parsed names. Malformed entries are skipped.
//
name_values parse_properties(std::string_view text);

// Format properties in the canonical "a:b; c:d" order.
std::string format_properties(const name_values &properties);

// Interns attr_prop values so equal sets share one index.
class attr_prop_table {
public:
    // Create a table holding the empty set at index 0.
    attr_prop_table();

    // Return the index of a value, adding it when new.
    attr_prop_index intern(const attr_prop &value);

    // Return the value at an index.
    const attr_prop &get(attr_prop_index index) const;

    // Return how many distinct sets exist.
    std::size_t size() const { return values_.size(); }

private:
    std::vector<attr_prop> values_;
    std::unordered_multimap<std::size_t, attr_prop_index> by_hash_;
};

} // namespace doc

#endif
