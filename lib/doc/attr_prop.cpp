//
// Attribute/property values and their interning table. AbiWord's table
// keeps a sorted checksum vector; a hash multimap is the same idea in
// standard containers. Inherited styles are resolved elsewhere, by
// doc::document, so this file stays free of document knowledge.
//
// Derived from AbiWord src/text/ptbl/xp/pp_AttrProp.cpp and
// pp_TableAttrProp.cpp, Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "doc/attr_prop.h"

#include <functional>

namespace doc {
namespace {

// Return a string without surrounding spaces.
std::string_view trimmed(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
        text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
        text.remove_suffix(1);
    return text;
}

// Combine two hashes.
std::size_t mixed(std::size_t seed, std::size_t value) {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) +
                   (seed >> 2));
}

// Hash one ordered name/value map.
std::size_t hash_of(const name_values &values) {
    const std::hash<std::string> hasher;
    std::size_t result = values.size();
    for (const auto &[name, value] : values) {
        result = mixed(result, hasher(name));
        result = mixed(result, hasher(value));
    }
    return result;
}

// Apply one side of a format change to a map.
void apply_change(name_values &target, format_op op,
                  const name_values &change) {
    if (op == format_op::replace) {
        target = change;
        return;
    }
    for (const auto &[name, value] : change) {
        if (op == format_op::remove)
            target.erase(name);
        else
            target.insert_or_assign(name, value);
    }
}

// Find a name in a transparent map.
std::optional<std::string_view> lookup(const name_values &values,
                                       std::string_view name) {
    const auto found = values.find(name);
    if (found == values.end())
        return std::nullopt;
    return std::string_view(found->second);
}

} // namespace

attr_prop::attr_prop(name_values attributes, name_values properties)
    : attributes_(std::move(attributes)),
      properties_(std::move(properties)) {
}

std::optional<std::string_view>
attr_prop::attribute(std::string_view name) const {
    return lookup(attributes_, name);
}

std::optional<std::string_view>
attr_prop::property(std::string_view name) const {
    return lookup(properties_, name);
}

attr_prop attr_prop::applied(format_op op, const name_values &attributes,
                             const name_values &properties) const {
    attr_prop result = *this;
    apply_change(result.attributes_, op, attributes);
    apply_change(result.properties_, op, properties);
    return result;
}

name_values parse_properties(std::string_view text) {
    name_values result;
    while (!text.empty()) {
        const std::size_t end = text.find(';');
        const std::string_view entry = text.substr(0, end);
        const std::size_t colon = entry.find(':');
        if (colon != std::string_view::npos) {
            const std::string_view name = trimmed(entry.substr(0, colon));
            if (!name.empty())
                result.insert_or_assign(std::string(name),
                                        std::string(trimmed(
                                            entry.substr(colon + 1))));
        }
        if (end == std::string_view::npos)
            break;
        text.remove_prefix(end + 1);
    }
    return result;
}

std::string format_properties(const name_values &properties) {
    std::string result;
    for (const auto &[name, value] : properties) {
        if (!result.empty())
            result += "; ";
        result += name;
        result += ':';
        result += value;
    }
    return result;
}

attr_prop_table::attr_prop_table() {
    values_.emplace_back();
    by_hash_.emplace(mixed(hash_of({}), hash_of({})), empty_attr_prop);
}

attr_prop_index attr_prop_table::intern(const attr_prop &value) {
    const std::size_t hash =
        mixed(hash_of(value.attributes()), hash_of(value.properties()));
    const auto [first, last] = by_hash_.equal_range(hash);
    for (auto entry = first; entry != last; ++entry)
        if (values_[entry->second] == value)
            return entry->second;
    const auto index = static_cast<attr_prop_index>(values_.size());
    values_.push_back(value);
    by_hash_.emplace(hash, index);
    return index;
}

const attr_prop &attr_prop_table::get(attr_prop_index index) const {
    return values_.at(index);
}

} // namespace doc
