//
// Resolves a paragraph's properties and turns its fragments into runs.
//
// Derived from AbiWord src/text/fmt/xp/fl_BlockLayout.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <charconv>
#include <ctime>

#include "fmt/block_layout.h"
#include "util/utf8.h"

namespace fmt {
namespace {

using namespace doc;

// Default image size when a file gives none.
constexpr lu default_image_extent = util::layout_resolution;

// Return the current date or time for a field.
std::u32string clock_text(const char *format) {
    const std::time_t now = std::time(nullptr);
    char buffer[64];
    const std::size_t size =
        std::strftime(buffer, sizeof buffer, format, std::localtime(&now));
    return util::to_utf32(std::string_view(buffer, size));
}

// Return the displayed value of a field.
std::u32string field_value(const attr_prop &format) {
    const std::string_view type =
        format.attribute("type").value_or(std::string_view{});
    if (type == "time") return clock_text("%X");
    if (type == "date") return clock_text("%A, %B %d, %Y");
    if (type == "date_dfl") return clock_text("%x");
    if (type == "page_number") return U"#";
    if (type == "page_count") return U"#";
    if (type == "list_label") return U"•";
    return U"";
}

} // namespace

std::vector<tab_stop> parse_tab_stops(std::string_view text) {
    std::vector<tab_stop> result;
    while (!text.empty()) {
        const std::size_t comma = text.find(',');
        const std::string_view entry = text.substr(0, comma);
        const std::size_t slash = entry.find('/');
        tab_stop stop;
        stop.position = util::to_lu(entry.substr(0, slash));
        if (slash != std::string_view::npos && slash + 1 < entry.size()) {
            const char type = entry[slash + 1];
            stop.kind = type == 'R'   ? tab_kind::right
                        : type == 'C' ? tab_kind::center
                                      : tab_kind::left;
        }
        result.push_back(stop);
        if (comma == std::string_view::npos)
            break;
        text.remove_prefix(comma + 1);
    }
    std::ranges::sort(result, {}, &tab_stop::position);
    return result;
}

void block_layout::resolve_properties(const format_context &context) {
    const document &source = context.document;
    const auto value = [&](std::string_view name) {
        return source.block_property(name, *strux_);
    };
    block_properties result;
    const std::string_view align = value("text-align");
    result.align = align == "center"    ? alignment::center
                   : align == "right"   ? alignment::right
                   : align == "justify" ? alignment::justify
                                        : alignment::left;
    result.margin_left = util::to_lu(value("margin-left"));
    result.margin_right = util::to_lu(value("margin-right"));
    result.margin_top = util::to_lu(value("margin-top"));
    result.margin_bottom = util::to_lu(value("margin-bottom"));
    result.text_indent = util::to_lu(value("text-indent"));
    result.default_tab =
        std::max<lu>(util::to_lu(value("default-tab-interval")), 20);
    const std::string_view spacing = value("line-height");
    if (spacing.ends_with('+')) {
        result.spacing = line_spacing::at_least;
        result.spacing_length = util::to_lu(spacing);
    } else if (util::parse_dimension_unit(spacing) !=
               util::dimension::none) {
        result.spacing = line_spacing::exact;
        result.spacing_length = util::to_lu(spacing);
    } else {
        result.spacing_multiple = util::leading_number(spacing);
        if (result.spacing_multiple <= 0)
            result.spacing_multiple = 1.0;
    }
    result.widows = static_cast<int>(util::leading_number(value("widows")));
    result.orphans =
        static_cast<int>(util::leading_number(value("orphans")));
    const auto truthy = [](std::string_view text) {
        return text == "1" || text == "yes" || text == "true";
    };
    result.keep_with_next = truthy(value("keep-with-next"));
    result.keep_together = truthy(value("keep-together"));
    result.tabs = parse_tab_stops(value("tabstops"));
    properties_ = std::move(result);
}

text_style
block_layout::style_for(const format_context &context,
                        const attr_prop *span) const {
    const document &source = context.document;
    const piece_table &table = source.table();
    const attr_prop *block = &table.formats().get(strux_->format());
    const attr_prop *section =
        context.section ? &table.formats().get(context.section->format())
                        : nullptr;
    const auto value = [&](std::string_view name) {
        return source.property(name, span, block, section);
    };
    gfx::font_request request;
    request.family = value("font-family");
    request.size_points = util::to_points(value("font-size"));
    if (request.size_points <= 0)
        request.size_points = 12;
    const std::string_view weight = value("font-weight");
    request.bold = weight == "bold" || util::leading_number(weight) >= 600;
    const std::string_view slant = value("font-style");
    request.italic = slant == "italic" || slant == "oblique";

    text_style style;
    const std::string_view position = value("text-position");
    if (position == "superscript" || position == "subscript") {
        const lu ascent = context.fonts.find(request)->ascent();
        request.size_points *= 2.0 / 3.0;
        style.rise = position == "superscript" ? ascent / 2 : -ascent / 4;
    }
    style.face = context.fonts.find(request);
    style.ink = util::parse_color(value("color")).value_or(util::color{});
    style.background = util::parse_color(value("bgcolor"));
    const std::string_view decoration = value("text-decoration");
    style.underline = decoration.find("underline") != decoration.npos;
    style.overline = decoration.find("overline") != decoration.npos;
    style.strike = decoration.find("line-through") != decoration.npos;
    return style;
}

std::vector<std::unique_ptr<run>>
block_layout::build_runs(const format_context &context) const {
    const piece_table &table = context.document.table();
    const fragments &frags = table.frags();
    std::vector<std::unique_ptr<run>> runs;
    std::uint32_t offset = 0;
    for (const frag *item = frags.next(strux_);
         item && item->kind() != frag_kind::strux &&
         item->kind() != frag_kind::end_of_doc;
         item = frags.next(item)) {
        const attr_prop &format = table.formats().get(item->format());
        if (item->kind() == frag_kind::text) {
            const text_style style = style_for(context, &format);
            const std::u32string_view text =
                table.text_of(*static_cast<const frag_text *>(item));
            // A tab is its own run; everything else shares one.
            std::size_t begin = 0;
            while (begin < text.size()) {
                if (text[begin] == U'\t') {
                    runs.push_back(std::make_unique<tab_run>(
                        offset + begin, style));
                    ++begin;
                    continue;
                }
                std::size_t end = text.find(U'\t', begin);
                if (end == std::u32string_view::npos)
                    end = text.size();
                runs.push_back(std::make_unique<text_run>(
                    offset + static_cast<std::uint32_t>(begin),
                    std::u32string(text.substr(begin, end - begin)),
                    style));
                begin = end;
            }
        } else {
            const auto type = static_cast<const frag_object *>(item)->type();
            const text_style style = style_for(context, &format);
            const lu ascent = style.face->ascent();
            const lu descent = style.face->descent();
            switch (type) {
            case object_kind::image: {
                const lu width = format.property("width")
                    ? util::to_lu(*format.property("width"))
                    : default_image_extent;
                const lu height = format.property("height")
                    ? util::to_lu(*format.property("height"))
                    : default_image_extent;
                runs.push_back(std::make_unique<image_run>(
                    offset,
                    std::string(format.attribute("dataid").value_or("")),
                    std::min(width, context.column_width), height));
                break;
            }
            case object_kind::field:
                runs.push_back(std::make_unique<field_run>(
                    offset, field_value(format), style));
                break;
            case object_kind::forced_line_break:
                runs.push_back(std::make_unique<break_run>(
                    offset, break_kind::line, ascent, descent));
                break;
            case object_kind::forced_column_break:
                runs.push_back(std::make_unique<break_run>(
                    offset, break_kind::column, ascent, descent));
                break;
            case object_kind::forced_page_break:
                runs.push_back(std::make_unique<break_run>(
                    offset, break_kind::page, ascent, descent));
                break;
            }
        }
        offset += item->length();
    }
    const text_style mark = style_for(context, nullptr);
    runs.push_back(std::make_unique<end_of_paragraph_run>(
        offset, mark.face->ascent(), mark.face->descent()));
    return runs;
}

} // namespace fmt
