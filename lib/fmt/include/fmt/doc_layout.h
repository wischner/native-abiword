//
// The whole document's layout: blocks flowed onto pages and columns.
// It listens to the piece table, reformats only the blocks a change
// touched, then reflows. Queries map document positions to page
// coordinates and back.
//
// Derived from AbiWord src/text/fmt/xp/fl_DocLayout.cpp,
// fl_SectionLayout.cpp, fb_ColumnBreaker.cpp, fp_Page.cpp and
// fp_Column.cpp, Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef FMT_DOC_LAYOUT_H
#define FMT_DOC_LAYOUT_H

#include <memory>
#include <vector>

#include "doc/document.h"
#include "fmt/block_layout.h"

namespace fmt {

// A line placed on a page.
struct placed_line {
    const line *item = nullptr;
    const block_layout *block = nullptr;

    // Index of the block in the document.
    std::size_t block_index = 0;

    // Top of the line box within the page.
    lu y = 0;

    // Left edge of the column within the page.
    lu column_x = 0;
};

// One sheet of paper.
struct page {
    lu width = 0;
    lu height = 0;

    // Lines top to bottom, column by column.
    std::vector<placed_line> lines;

    // Text column bounds, for the rulers.
    lu margin_left = 0;
    lu margin_right = 0;
};

// A resolved caret position.
struct caret_location {
    std::size_t page = 0;

    // Within the page.
    lu x = 0;
    lu y = 0;
    lu height = 0;
};

// One section's resolved page geometry.
struct section_geometry {
    lu page_width = 0;
    lu page_height = 0;
    lu margin_left = 0;
    lu margin_right = 0;
    lu margin_top = 0;
    lu margin_bottom = 0;
    int columns = 1;
    lu column_gap = 0;

    // Return the width of one text column.
    lu column_width() const;
};

// Lays a document out.
class doc_layout final : public doc::change_listener {
public:
    // Create and fully format a layout, subscribing to the document.
    doc_layout(doc::document &source, gfx::font_provider &fonts);

    // Unsubscribe from the document.
    ~doc_layout() override;

    doc_layout(const doc_layout &) = delete;
    doc_layout &operator=(const doc_layout &) = delete;

    // Return the pages.
    const std::vector<page> &pages() const { return pages_; }

    // Return the blocks in document order.
    const std::vector<std::unique_ptr<block_layout>> &blocks() const {
        return blocks_;
    }

    // Bring the layout up to date after document changes.
    void update();

    // Reformat everything, e.g. after a page setup or style change.
    void rebuild();

    // Return the document position of a block's first character.
    doc::position block_start(const block_layout &block) const;

    // Return the position just after a block's last character.
    doc::position block_end(const block_layout &block) const;

    // Return the block after one, or null at the end.
    const block_layout *next_block(const block_layout &block) const;

    // Return the block before one, or null at the start.
    const block_layout *previous_block(const block_layout &block) const;

    // Return the block owning a position.
    const block_layout *block_at(doc::position pos) const;

    // Return the page coordinates of a document position.
    caret_location locate(doc::position pos) const;

    //
    // Return the document position nearest a point on a page.
    //
    // Parameters:
    //      page_index  - Index into pages().
    //      x           - Within the page.
    //      y           - Within the page.
    //
    doc::position position_at(std::size_t page_index, lu x, lu y) const;

    // Return the first and past-the-last positions of a position's line.
    std::pair<doc::position, doc::position>
    line_bounds(doc::position pos) const;

    //
    // Move vertically by lines.
    //
    // Parameters:
    //      pos         - Starting position.
    //      x           - Sticky horizontal position within the page.
    //      lines       - Lines to move; negative goes up.
    //
    doc::position move_lines(doc::position pos, lu x, int lines) const;

    void on_change(const doc::change_record &record) override;
    void on_strux_removed(const doc::frag_strux &strux) override;

private:
    struct line_address {
        std::size_t page = 0;
        std::size_t line = 0;
    };

    void build_blocks();
    section_geometry geometry_for(const doc::frag_strux *section) const;
    format_context context_for(const doc::frag_strux *section) const;
    void format_dirty_blocks();
    void flow();
    std::size_t block_index_of(const doc::frag_strux *strux) const;
    std::size_t block_index_at(doc::position pos) const;
    line_address address_of(doc::position pos) const;
    const doc::frag_strux *section_of(const block_layout &block) const;

    doc::document &document_;
    gfx::font_provider &fonts_;
    std::vector<std::unique_ptr<block_layout>> blocks_;
    std::vector<page> pages_;
    bool structure_dirty_ = true;
};

} // namespace fmt

#endif
