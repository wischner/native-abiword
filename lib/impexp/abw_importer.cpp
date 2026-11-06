//
// Reads AWML, AbiWord's native format. Unknown elements are skipped so
// newer files still open. Table and frame structure has no layout yet:
// cell paragraphs flatten into the body and a warning says so.
//
// Header and footer sections stay in the piece table with their "type"
// attribute. They are not laid out yet, but they survive a save.
//
// Derived from AbiWord src/wp/impexp/xp/ie_imp_AbiWord_1.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "impexp/file_format.h"
#include "impexp/xml_reader.h"
#include "util/utf8.h"

namespace impexp {
namespace {

using namespace doc;

// Return one attribute or an empty view.
std::string_view value_of(const xml_attributes &attributes,
                          std::string_view name) {
    const auto found = attributes.find(name);
    return found == attributes.end() ? std::string_view{}
                                     : std::string_view(found->second);
}

// Split an element's attributes into interned document format.
attr_prop_index format_from(document &target,
                            const xml_attributes &attributes) {
    name_values kept;
    name_values properties;
    for (const auto &[name, value] : attributes) {
        if (name == "props")
            properties = parse_properties(value);
        else if (name != "xid" && name != "fragid")
            kept.insert_or_assign(name, value);
    }
    return target.table().formats().intern(
        attr_prop(std::move(kept), std::move(properties)));
}

// Builds a document from AWML parse events.
class abw_builder final : public xml_handler {
public:
    abw_builder(document &target, std::vector<std::string> &warnings)
        : target_(target), table_(target.table()), warnings_(warnings) {
        formats_.push_back(empty_attr_prop);
    }

    void start_element(std::string_view name,
                       const xml_attributes &attributes) override {
        if (name == "abiword") {
            target_.set_document_properties(
                parse_properties(value_of(attributes, "props")));
        } else if (name == "section") {
            add_strux(strux_kind::section,
                      format_from(target_, attributes));
            in_block_ = false;
        } else if (name == "p") {
            ensure_section();
            add_strux(strux_kind::block, format_from(target_, attributes));
            in_block_ = true;
        } else if (name == "c" || name == "a" || name == "ann") {
            ensure_block();
            formats_.push_back(merged_format(attributes));
        } else if (name == "image") {
            add_object(object_kind::image, attributes);
        } else if (name == "field") {
            add_object(object_kind::field, attributes);
        } else if (name == "br") {
            add_object(object_kind::forced_line_break, {});
        } else if (name == "cbr") {
            add_object(object_kind::forced_column_break, {});
        } else if (name == "pbr") {
            add_object(object_kind::forced_page_break, {});
        } else if (name == "s") {
            add_style(attributes);
        } else if (name == "pagesize") {
            set_page(attributes);
        } else if (name == "m") {
            metadata_key_ = value_of(attributes, "key");
            collected_.clear();
            collecting_ = true;
        } else if (name == "d") {
            data_name_ = value_of(attributes, "name");
            data_mime_ = value_of(attributes, "mime-type");
            data_base64_ = value_of(attributes, "base64") != "no";
            collected_.clear();
            collecting_ = true;
        } else if (name == "table" || name == "frame") {
            warn_once("Tables and frames were flattened to paragraphs.");
        } else if (name == "foot" || name == "endnote" ||
                   name == "annotate" || name == "toc") {
            warn_once("Notes and tables of contents were flattened.");
        }
    }

    void end_element(std::string_view name) override {
        if (name == "c" || name == "a" || name == "ann") {
            if (formats_.size() > 1)
                formats_.pop_back();
        } else if (name == "m") {
            target_.metadata().insert_or_assign(metadata_key_, collected_);
            collecting_ = false;
        } else if (name == "d") {
            data_item item;
            item.mime_type = data_mime_;
            if (data_base64_) {
                item.bytes = base64_decode(collected_);
            } else {
                item.bytes.assign(collected_.begin(), collected_.end());
            }
            target_.set_data(data_name_, std::move(item));
            collecting_ = false;
        } else if (name == "p") {
            in_block_ = false;
        }
    }

    void characters(std::string_view text) override {
        if (collecting_) {
            collected_.append(text);
            return;
        }
        if (!in_block_)
            return;
        // Line ends in a paragraph are file formatting, not content.
        std::u32string content = util::to_utf32(text);
        std::erase_if(content, [](char32_t c) {
            return c == U'\n' || c == U'\r';
        });
        if (content.empty())
            return;
        table_.insert_text(table_.end_position(), content, formats_.back());
    }

    // Guarantee the structure every document needs.
    void finish() {
        if (!has_block_)
            ensure_block();
    }

private:
    void add_strux(strux_kind type, attr_prop_index format) {
        table_.insert_strux(table_.end_position(), type, format);
        if (type == strux_kind::section)
            has_section_ = true;
        else
            has_block_ = true;
    }

    void ensure_section() {
        if (!has_section_)
            add_strux(strux_kind::section, empty_attr_prop);
    }

    void ensure_block() {
        ensure_section();
        if (!in_block_) {
            add_strux(strux_kind::block,
                      table_.formats().intern(
                          attr_prop({{"style", "Normal"}}, {})));
            in_block_ = true;
        }
    }

    void add_object(object_kind type, const xml_attributes &attributes) {
        ensure_block();
        table_.insert_object(table_.end_position(), type,
                             merged_format(attributes));
    }

    // Return the enclosing inline format with an element's on top.
    attr_prop_index merged_format(const xml_attributes &attributes) {
        const attr_prop &outer = table_.formats().get(formats_.back());
        const attr_prop_index own = format_from(target_, attributes);
        const attr_prop &inner = table_.formats().get(own);
        return table_.formats().intern(outer.applied(
            format_op::add, inner.attributes(), inner.properties()));
    }

    void add_style(const xml_attributes &attributes) {
        style value;
        value.name = value_of(attributes, "name");
        const std::string_view type = value_of(attributes, "type");
        value.type = type.empty() ? 'P' : type.front();
        value.based_on = value_of(attributes, "basedon");
        value.followed_by = value_of(attributes, "followedby");
        target_.define_style(std::move(value),
                             parse_properties(
                                 value_of(attributes, "props")));
    }

    void set_page(const xml_attributes &attributes) {
        page_setup &page = target_.page();
        page.name = value_of(attributes, "pagetype");
        page.portrait = value_of(attributes, "orientation") != "landscape";
        const double width = std::strtod(
            std::string(value_of(attributes, "width")).c_str(), nullptr);
        const double height = std::strtod(
            std::string(value_of(attributes, "height")).c_str(), nullptr);
        const std::string_view units = value_of(attributes, "units");
        const double per_inch = units == "cm"   ? 2.54
                                : units == "mm" ? 25.4
                                                : 1.0;
        if (width > 0 && height > 0) {
            page.width_inches = width / per_inch;
            page.height_inches = height / per_inch;
        }
    }

    void warn_once(const std::string &message) {
        if (std::ranges::find(warnings_, message) == warnings_.end())
            warnings_.push_back(message);
    }

    document &target_;
    piece_table &table_;
    std::vector<std::string> &warnings_;
    std::vector<attr_prop_index> formats_;
    bool has_section_ = false;
    bool has_block_ = false;
    bool in_block_ = false;
    bool collecting_ = false;
    std::string collected_;
    std::string metadata_key_;
    std::string data_name_;
    std::string data_mime_;
    bool data_base64_ = true;
};

// Reads AWML documents.
class abw_importer final : public importer {
public:
    import_result read(std::string_view bytes,
                       document &target) override {
        import_result result;
        target.clear_content();
        target.table().set_loading(true);
        abw_builder builder(target, result.warnings);
        result.ok = parse_xml(bytes, builder, result.error);
        builder.finish();
        target.table().set_loading(false);
        return result;
    }
};

} // namespace

std::unique_ptr<importer> make_abw_importer() {
    return std::make_unique<abw_importer>();
}

} // namespace impexp
