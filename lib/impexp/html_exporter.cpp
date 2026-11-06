//
// Exports a document as standalone HTML. Formatting maps to inline CSS
// because most AbiWord properties already are CSS property names.
//
// Derived from AbiWord src/wp/impexp/xp/ie_exp_HTML.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <array>

#include "impexp/file_format.h"
#include "impexp/xml_reader.h"
#include "util/utf8.h"

namespace impexp {
namespace {

using namespace doc;

// Return inline CSS for the properties CSS shares with AbiWord.
std::string css_of(const attr_prop &format) {
    static constexpr std::array<std::string_view, 12> shared{
        "font-family", "font-size", "font-style", "font-weight",
        "text-decoration", "text-align", "text-indent", "margin-left",
        "margin-right", "margin-top", "margin-bottom", "line-height"};
    std::string css;
    for (const std::string_view name : shared)
        if (const auto value = format.property(name))
            css += std::string(name) + ':' + std::string(*value) + ';';
    if (const auto color = format.property("color"))
        css += "color:#" + std::string(*color) + ';';
    if (const auto back = format.property("bgcolor");
        back && *back != "transparent")
        css += "background-color:#" + std::string(*back) + ';';
    return css;
}

// Return an element name for a paragraph style.
std::string_view tag_for(const attr_prop &format) {
    const auto style = format.attribute("style");
    if (style == "Heading 1") return "h1";
    if (style == "Heading 2") return "h2";
    if (style == "Heading 3") return "h3";
    if (style == "Heading 4") return "h4";
    return "p";
}

// Writes standalone HTML.
class html_exporter final : public exporter {
public:
    std::string write(const document &source) override {
        const piece_table &table = source.table();
        std::string out = "<!DOCTYPE html>\n<html><head>"
                          "<meta charset=\"utf-8\"><title>";
        const auto title = source.metadata().find("dc.title");
        if (title != source.metadata().end())
            out += xml_escape(title->second);
        out += "</title></head><body>\n";
        std::string_view open_tag;
        const fragments &frags = table.frags();
        for (const frag *item = frags.first(); item;
             item = frags.next(item)) {
            const attr_prop &format = table.formats().get(item->format());
            if (item->kind() == frag_kind::strux &&
                static_cast<const frag_strux *>(item)->type() ==
                    strux_kind::block) {
                close(out, open_tag);
                open_tag = tag_for(format);
                out += '<' + std::string(open_tag);
                if (const std::string css = css_of(format); !css.empty())
                    out += " style=\"" + xml_escape(css) + '"';
                out += '>';
            } else if (item->kind() == frag_kind::text) {
                write_span(out, table, *static_cast<const frag_text *>(item),
                           format);
            } else if (item->kind() == frag_kind::object &&
                       static_cast<const frag_object *>(item)->type() ==
                           object_kind::forced_line_break) {
                out += "<br>";
            }
        }
        close(out, open_tag);
        out += "</body></html>\n";
        return out;
    }

private:
    static void close(std::string &out, std::string_view tag) {
        if (!tag.empty())
            out += "</" + std::string(tag) + ">\n";
    }

    static void write_span(std::string &out, const piece_table &table,
                           const frag_text &item,
                           const attr_prop &format) {
        const std::string text =
            xml_escape(util::to_utf8(table.text_of(item)));
        const std::string css = css_of(format);
        if (css.empty())
            out += text;
        else
            out += "<span style=\"" + xml_escape(css) + "\">" + text +
                   "</span>";
    }
};

} // namespace

std::unique_ptr<exporter> make_html_exporter() {
    return std::make_unique<html_exporter>();
}

} // namespace impexp
