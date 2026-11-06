//
// A word-processing document: piece table, styles, page setup, embedded
// data, and property evaluation along the inheritance chain.
//
// Derived from AbiWord src/text/ptbl/xp/pd_Document.cpp, pd_Style.cpp,
// pt_PT_Styles.cpp and PP_evalProperty in pp_Property.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef DOC_DOCUMENT_H
#define DOC_DOCUMENT_H

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "doc/piece_table.h"

namespace doc {

// A named bundle of formatting.
struct style {
    std::string name;

    // 'P' for paragraph styles, 'C' for character styles.
    char type = 'P';

    std::string based_on;
    std::string followed_by;
    attr_prop_index format = empty_attr_prop;
};

// Embedded binary content such as an image.
struct data_item {
    std::string mime_type;
    std::vector<std::uint8_t> bytes;
};

// The physical page a section is laid out on.
struct page_setup {
    std::string name = "Letter";
    double width_inches = 8.5;
    double height_inches = 11.0;
    bool portrait = true;
    double scale = 1.0;
};

// An ordered list of the styles offered to users.
const std::vector<std::string> &builtin_style_names();

// One document.
class document {
public:
    // Create an empty document with built-in styles and one paragraph.
    document();

    // Return the piece table.
    piece_table &table() { return table_; }

    // Return the piece table.
    const piece_table &table() const { return table_; }

    // Return the page setup.
    page_setup &page() { return page_; }

    // Return the page setup.
    const page_setup &page() const { return page_; }

    // Return all styles by name.
    const std::map<std::string, style, std::less<>> &styles() const {
        return styles_;
    }

    //
    // Define or replace a style.
    //
    // Parameters:
    //      value       - Style with format left at empty.
    //      properties  - Its properties.
    //
    void define_style(style value, const name_values &properties);

    // Return a style, or null when unknown.
    const style *find_style(std::string_view name) const;

    // Return the document-level properties set.
    attr_prop_index document_format() const { return document_format_; }

    // Replace the document-level properties.
    void set_document_properties(const name_values &properties);

    // Return embedded data by name.
    const std::map<std::string, data_item, std::less<>> &data() const {
        return data_;
    }

    // Add or replace embedded data.
    void set_data(std::string name, data_item item);

    // Return metadata by key.
    std::map<std::string, std::string> &metadata() { return metadata_; }

    // Return metadata by key.
    const std::map<std::string, std::string> &metadata() const {
        return metadata_;
    }

    // Remove the initial paragraph before a loader fills the document.
    void clear_content();

    //
    // Evaluate a property along the inheritance chain.
    //
    // Parameters:
    //      name        - Property name.
    //      span        - Format of the text, or null.
    //      block       - Format of the paragraph, or null.
    //      section     - Format of the section, or null.
    //
    // Returns:
    //      The first value found. A property that does not inherit
    //      stops at the most specific level supplied. The result
    //      views storage owned by this document.
    //
    std::string_view property(std::string_view name, const attr_prop *span,
                              const attr_prop *block,
                              const attr_prop *section) const;

    // Evaluate a block-level property of a paragraph strux.
    std::string_view block_property(std::string_view name,
                                    const frag_strux &block) const;

    // Return the file this document was loaded from or saved to.
    const std::filesystem::path &path() const { return path_; }

    // Set the file this document belongs to.
    void set_path(std::filesystem::path path) { path_ = std::move(path); }

private:
    // Look a name up in a set and through its style chain.
    bool lookup(std::string_view name, const attr_prop &format,
                std::string_view &value) const;

    // Install AbiWord's built-in styles.
    void load_builtin_styles();

    piece_table table_;
    page_setup page_;
    std::map<std::string, style, std::less<>> styles_;
    std::map<std::string, data_item, std::less<>> data_;
    std::map<std::string, std::string> metadata_;
    attr_prop_index document_format_ = empty_attr_prop;
    std::filesystem::path path_;
};

// Opens a history glob for the current scope.
class scoped_glob {
public:
    // Begin a user action on a document.
    explicit scoped_glob(document &target);

    // End the user action.
    ~scoped_glob();

    scoped_glob(const scoped_glob &) = delete;
    scoped_glob &operator=(const scoped_glob &) = delete;

private:
    document &target_;
};

} // namespace doc

#endif
