//
// One paragraph: its resolved properties, its runs and the lines they
// break into. A block reformats independently of its neighbors, which
// is what keeps typing cost proportional to the paragraph.
//
// Derived from AbiWord src/text/fmt/xp/fl_BlockLayout.cpp, fp_Line.cpp,
// fb_LineBreaker.cpp and fb_Alignment.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef FMT_BLOCK_LAYOUT_H
#define FMT_BLOCK_LAYOUT_H

#include <memory>
#include <string>
#include <vector>

#include "doc/document.h"
#include "fmt/run.h"

namespace fmt {

// A paragraph's horizontal alignment.
enum class alignment { left, center, right, justify };

// What a tab stop aligns.
enum class tab_kind { left, center, right };

// One explicit tab stop.
struct tab_stop {
    lu position = 0;
    tab_kind kind = tab_kind::left;
};

// How the line-height property is interpreted.
enum class line_spacing { multiple, exact, at_least };

// Resolved paragraph properties.
struct block_properties {
    alignment align = alignment::left;
    lu margin_left = 0;
    lu margin_right = 0;
    lu margin_top = 0;
    lu margin_bottom = 0;
    lu text_indent = 0;
    lu default_tab = 720;
    line_spacing spacing = line_spacing::multiple;
    double spacing_multiple = 1.0;
    lu spacing_length = 0;
    int widows = 2;
    int orphans = 2;
    bool keep_with_next = false;
    bool keep_together = false;
    std::vector<tab_stop> tabs;
};

// Parse an AbiWord "tabstops" property, e.g. "1in/L0,3in/R0".
std::vector<tab_stop> parse_tab_stops(std::string_view text);

// One formatted line.
class line {
public:
    // Return the runs, left to right.
    const std::vector<std::unique_ptr<run>> &runs() const { return runs_; }

    // Return the first character offset in the block.
    std::uint32_t first_offset() const;

    // Return the offset after the last character.
    std::uint32_t end_offset() const;

    // Return the line's left edge within the column.
    lu x() const { return x_; }

    // Return the height of the line box.
    lu height() const { return height_; }

    // Return the baseline within the line box.
    lu baseline() const { return baseline_; }

    // Return the summed width of the runs.
    lu content_width() const { return content_width_; }

    // Return the break that ended the line, if forced.
    break_kind forced_break() const;

    // Return the x within the column of a character offset.
    lu x_of_offset(std::uint32_t offset) const;

    // Return the character offset nearest an x within the column.
    std::uint32_t offset_at_x(lu x) const;

    // Draw the line.
    void draw(gfx::painter &painter, int column_x, int baseline) const;

private:
    friend class block_layout;

    std::vector<std::unique_ptr<run>> runs_;
    lu x_ = 0;
    lu height_ = 0;
    lu baseline_ = 0;
    lu content_width_ = 0;
};

// Environment a block is formatted in.
struct format_context {
    const doc::document &document;
    gfx::font_provider &fonts;
    const doc::frag_strux *section = nullptr;

    // Text column width.
    lu column_width = 0;
};

// One laid-out paragraph.
class block_layout {
public:
    // Create the layout of a block strux, unformatted.
    explicit block_layout(const doc::frag_strux *strux);

    // Return the strux this block lays out.
    const doc::frag_strux *strux() const { return strux_; }

    // Return resolved properties; valid after format().
    const block_properties &properties() const { return properties_; }

    // Return the lines; valid after format().
    const std::vector<std::unique_ptr<line>> &lines() const {
        return lines_;
    }

    // Report whether content changed since the last format().
    bool dirty() const { return dirty_; }

    // Request a reformat.
    void mark_dirty() { dirty_ = true; }

    // Rebuild runs and lines from the document.
    void format(const format_context &context);

private:
    void resolve_properties(const format_context &context);
    std::vector<std::unique_ptr<run>>
    build_runs(const format_context &context) const;
    void break_lines(std::vector<std::unique_ptr<run>> runs,
                     const format_context &context);
    text_style style_for(const format_context &context,
                         const doc::attr_prop *span) const;
    void finish_line(line &item, bool last_line,
                     const format_context &context);
    lu tab_target(lu x, tab_kind &kind) const;

    const doc::frag_strux *strux_;
    block_properties properties_;
    std::vector<std::unique_ptr<line>> lines_;
    bool dirty_ = true;
};

} // namespace fmt

#endif
