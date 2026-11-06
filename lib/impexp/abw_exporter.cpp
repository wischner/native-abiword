//
// Writes AWML. Output stays loadable by AbiWord itself, which is why
// the element set and attribute spelling follow it exactly.
//
// Derived from AbiWord src/wp/impexp/xp/ie_exp_AbiWord_1.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <cstdio>

#include "impexp/file_format.h"
#include "impexp/xml_reader.h"
#include "util/utf8.h"

namespace impexp {
namespace {

using namespace doc;

// Return attributes as XML, with properties in AbiWord's "props".
std::string format_xml(const attr_prop &format) {
    std::string result;
    for (const auto &[name, value] : format.attributes())
        result += ' ' + name + "=\"" + xml_escape(value) + '"';
    if (!format.properties().empty())
        result += " props=\"" +
                  xml_escape(format_properties(format.properties())) + '"';
    return result;
}

// Return text with control characters AWML keeps literally escaped.
std::string text_xml(std::u32string_view text) {
    return xml_escape(util::to_utf8(text));
}

// Writes one document.
class abw_writer {
public:
    explicit abw_writer(const document &source)
        : source_(source), table_(source.table()) {
    }

    std::string run() {
        out_ += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<!DOCTYPE abiword PUBLIC \"-//ABISOURCE//DTD AWML 1.0 "
                "Strict//EN\" \"http://www.abisource.com/awml.dtd\">\n"
                "<abiword template=\"false\" fileformat=\"1.1\"";
        const attr_prop &own =
            table_.formats().get(source_.document_format());
        if (!own.properties().empty())
            out_ += " props=\"" +
                    xml_escape(format_properties(own.properties())) + '"';
        out_ += ">\n";
        write_metadata();
        write_styles();
        write_page();
        write_content();
        write_data();
        out_ += "</abiword>\n";
        return std::move(out_);
    }

private:
    void write_metadata() {
        if (source_.metadata().empty())
            return;
        out_ += "<metadata>\n";
        for (const auto &[key, value] : source_.metadata())
            out_ += "<m key=\"" + xml_escape(key) + "\">" +
                    xml_escape(value) + "</m>\n";
        out_ += "</metadata>\n";
    }

    void write_styles() {
        out_ += "<styles>\n";
        for (const auto &[name, value] : source_.styles()) {
            out_ += "<s type=\"";
            out_.push_back(value.type);
            out_ += "\" name=\"" + xml_escape(name) + "\" basedon=\"" +
                    xml_escape(value.based_on) + "\" followedby=\"" +
                    xml_escape(value.followed_by) + "\" props=\"" +
                    xml_escape(format_properties(
                        table_.formats().get(value.format).properties())) +
                    "\"/>\n";
        }
        out_ += "</styles>\n";
    }

    void write_page() {
        const page_setup &page = source_.page();
        char size[96];
        std::snprintf(size, sizeof size,
                      "width=\"%.6f\" height=\"%.6f\" units=\"in\" "
                      "page-scale=\"%.6f\"",
                      page.width_inches, page.height_inches, page.scale);
        out_ += "<pagesize pagetype=\"" + xml_escape(page.name) +
                "\" orientation=\"" +
                (page.portrait ? "portrait" : "landscape") + "\" " + size +
                "/>\n";
    }

    void close_block() {
        if (block_open_)
            out_ += "</p>\n";
        block_open_ = false;
    }

    void close_section() {
        close_block();
        if (section_open_)
            out_ += "</section>\n";
        section_open_ = false;
    }

    void write_content() {
        const fragments &frags = table_.frags();
        for (const frag *item = frags.first(); item;
             item = frags.next(item)) {
            const attr_prop &format = table_.formats().get(item->format());
            switch (item->kind()) {
            case frag_kind::strux:
                write_strux(*static_cast<const frag_strux *>(item), format);
                break;
            case frag_kind::text:
                write_text(*static_cast<const frag_text *>(item), format);
                break;
            case frag_kind::object:
                write_object(*static_cast<const frag_object *>(item),
                             format);
                break;
            case frag_kind::end_of_doc:
                break;
            }
        }
        close_section();
    }

    void write_strux(const frag_strux &item, const attr_prop &format) {
        if (item.type() == strux_kind::section) {
            close_section();
            out_ += "<section" + format_xml(format) + ">\n";
            section_open_ = true;
            return;
        }
        close_block();
        out_ += "<p" + format_xml(format) + ">";
        block_open_ = true;
    }

    void write_text(const frag_text &item, const attr_prop &format) {
        const std::string text = text_xml(table_.text_of(item));
        if (format.attributes().empty() && format.properties().empty())
            out_ += text;
        else
            out_ += "<c" + format_xml(format) + ">" + text + "</c>";
    }

    void write_object(const frag_object &item, const attr_prop &format) {
        switch (item.type()) {
        case object_kind::image:
            out_ += "<image" + format_xml(format) + "/>";
            break;
        case object_kind::field:
            out_ += "<field" + format_xml(format) + "/>";
            break;
        case object_kind::forced_line_break:
            out_ += "<br/>";
            break;
        case object_kind::forced_column_break:
            out_ += "<cbr/>";
            break;
        case object_kind::forced_page_break:
            out_ += "<pbr/>";
            break;
        }
    }

    void write_data() {
        if (source_.data().empty())
            return;
        out_ += "<data>\n";
        for (const auto &[name, item] : source_.data())
            out_ += "<d name=\"" + xml_escape(name) + "\" mime-type=\"" +
                    xml_escape(item.mime_type) + "\" base64=\"yes\">" +
                    base64_encode(item.bytes) + "</d>\n";
        out_ += "</data>\n";
    }

    const document &source_;
    const piece_table &table_;
    std::string out_;
    bool block_open_ = false;
    bool section_open_ = false;
};

// Writes AWML documents.
class abw_exporter final : public exporter {
public:
    std::string write(const document &source) override {
        return abw_writer(source).run();
    }
};

} // namespace

std::unique_ptr<exporter> make_abw_exporter() {
    return std::make_unique<abw_exporter>();
}

} // namespace impexp
