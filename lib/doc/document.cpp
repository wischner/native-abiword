//
// Document services around the piece table. Property evaluation here
// is the whole of AbiWord's cascade: span, span style, block, block
// style chain, section, document, then the property table.
//
// Derived from AbiWord src/text/ptbl/xp/pd_Document.cpp, pd_Style.cpp,
// pt_PT_Styles.cpp and PP_evalProperty in pp_Property.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "doc/document.h"

#include "doc/properties.h"

namespace doc {
namespace {

// The longest based-on chain followed, guarding against style cycles
// a hand-edited or hostile file could contain.
constexpr int max_style_depth = 16;

const char *const normal_properties =
    "font-family:Times New Roman; margin-top:0pt; font-variant:normal; "
    "margin-left:0pt; text-indent:0in; widows:2; font-style:normal; "
    "font-weight:normal; text-decoration:none; color:000000; "
    "line-height:1.0; text-align:left; margin-bottom:0pt; "
    "text-position:normal; margin-right:0pt; bgcolor:transparent; "
    "font-size:12pt; font-stretch:normal";

// One built-in style in AbiWord's definition order.
struct builtin_style {
    const char *name;
    const char *based_on;
    const char *followed_by;
    const char *properties;
};

const builtin_style builtin_styles[] = {
    {"Normal", "", "Current Settings", normal_properties},
    {"Heading 1", "Normal", "Normal",
     "keep-with-next:1; margin-top:22pt; margin-bottom:3pt; "
     "font-family:Arial; font-size:17pt; font-weight:bold"},
    {"Heading 2", "Normal", "Normal",
     "keep-with-next:1; margin-top:22pt; margin-bottom:3pt; "
     "font-family:Arial; font-size:14pt; font-weight:bold"},
    {"Heading 3", "Normal", "Normal",
     "keep-with-next:1; margin-top:22pt; margin-bottom:3pt; "
     "font-family:Arial; font-size:12pt; font-weight:bold"},
    {"Heading 4", "Normal", "Normal",
     "keep-with-next:1; margin-top:22pt; margin-bottom:3pt; "
     "font-family:Arial; font-size:11pt; font-weight:bold"},
    {"Plain Text", "Normal", "Current Settings",
     "font-family:Courier New"},
    {"Block Text", "Normal", "Current Settings",
     "margin-left:1in; margin-right:1in; margin-bottom:6pt"},
};

} // namespace

const std::vector<std::string> &builtin_style_names() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> result;
        for (const builtin_style &entry : builtin_styles)
            result.emplace_back(entry.name);
        return result;
    }();
    return names;
}

document::document() {
    load_builtin_styles();
    const attr_prop_index normal =
        table_.formats().intern(attr_prop({{"style", "Normal"}}, {}));
    table_.set_loading(true);
    table_.insert_strux(0, strux_kind::section, empty_attr_prop);
    table_.insert_strux(1, strux_kind::block, normal);
    table_.set_loading(false);
}

void document::load_builtin_styles() {
    for (const builtin_style &entry : builtin_styles) {
        style value;
        value.name = entry.name;
        value.based_on = entry.based_on;
        value.followed_by = entry.followed_by;
        define_style(std::move(value),
                     parse_properties(entry.properties));
    }
}

void document::define_style(style value, const name_values &properties) {
    value.format = table_.formats().intern(attr_prop({}, properties));
    std::string key = value.name;
    styles_.insert_or_assign(std::move(key), std::move(value));
}

const style *document::find_style(std::string_view name) const {
    const auto found = styles_.find(name);
    return found == styles_.end() ? nullptr : &found->second;
}

void document::set_document_properties(const name_values &properties) {
    document_format_ =
        table_.formats().intern(attr_prop({}, properties));
}

void document::set_data(std::string name, data_item item) {
    data_.insert_or_assign(std::move(name), std::move(item));
}

void document::clear_content() {
    table_.reset();
}

bool document::lookup(std::string_view name, const attr_prop &format,
                      std::string_view &value) const {
    if (const auto found = format.property(name)) {
        value = *found;
        return true;
    }
    const auto style_name = format.attribute("style");
    if (!style_name)
        return false;
    const style *current = find_style(*style_name);
    for (int depth = 0; current && depth < max_style_depth; ++depth) {
        const attr_prop &style_format =
            table_.formats().get(current->format);
        if (const auto found = style_format.property(name)) {
            value = *found;
            return true;
        }
        current = find_style(current->based_on);
    }
    return false;
}

std::string_view document::property(std::string_view name,
                                    const attr_prop *span,
                                    const attr_prop *block,
                                    const attr_prop *section) const {
    const property_definition *definition = find_property(name);
    if (!definition)
        return {};
    std::string_view value;
    const attr_prop *const levels[] = {
        span, block, section, &table_.formats().get(document_format_)};
    for (const attr_prop *level : levels) {
        if (!level)
            continue;
        if (lookup(name, *level, value))
            return value;
        if (!definition->inherits)
            break;
    }
    return definition->initial;
}

std::string_view
document::block_property(std::string_view name,
                         const frag_strux &block) const {
    return property(name, nullptr, &table_.formats().get(block.format()),
                    nullptr);
}

scoped_glob::scoped_glob(document &target) : target_(target) {
    target_.table().history().begin_glob();
}

scoped_glob::~scoped_glob() {
    target_.table().history().end_glob();
}

} // namespace doc
