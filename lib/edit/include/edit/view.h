//
// The editing view: one document, its layout, a caret and a selection.
// It owns every operation a user can perform, and the front end only
// forwards input to it, so all editing behavior is testable headless.
//
// Coordinates: "view pixels" place all pages in one tall column at the
// current zoom. The front end adds its scroll offset.
//
// Derived from AbiWord src/text/fmt/xp/fv_View.cpp, fv_View_cmd.cpp and
// fv_View_protected.cpp, Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef EDIT_VIEW_H
#define EDIT_VIEW_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "doc/document.h"
#include "fmt/doc_layout.h"

namespace edit {

using doc::position;

// A half-open document range.
struct range {
    position begin = 0;
    position end = 0;
};

// One piece of rich clipboard content.
struct clip_item {
    // Text, or empty for a paragraph break or an object.
    std::u32string text;

    // Whether this item is a paragraph break.
    bool paragraph = false;

    // Whether this item is an inline object.
    bool object = false;
    doc::object_kind object_type = doc::object_kind::image;
    doc::attr_prop_index format = doc::empty_attr_prop;
};

// Options for find and replace.
struct find_options {
    bool match_case = false;
    bool whole_word = false;
};

// One open document being edited.
class view final : public doc::change_listener {
public:
    // Create a view of a document, caret at the start.
    view(doc::document &source, gfx::font_provider &fonts);

    // Stop listening to the document.
    ~view() override;

    view(const view &) = delete;
    view &operator=(const view &) = delete;

    // Return the document.
    doc::document &document() { return document_; }

    // Return the layout.
    const fmt::doc_layout &layout() const { return layout_; }

    // Reformat everything, e.g. after a page setup change.
    void relayout();

    //
    // Caret and selection
    //

    // Return the caret position.
    position point() const { return point_; }

    // Report whether text is selected.
    bool has_selection() const { return anchor_ != point_; }

    // Return the selected range, or an empty one at the caret.
    range selection() const;

    // Return the first caret position of the document.
    position document_start() const;

    // Return the last caret position of the document.
    position document_end() const;

    // Move the caret, dropping any selection.
    void set_point(position pos);

    // Move the caret, keeping the anchor.
    void extend_to(position pos);

    // Select all content.
    void select_all();

    // Select the word at a position.
    void select_word(position pos);

    // Select the visual line at a position.
    void select_line(position pos);

    // Select the paragraph at a position.
    void select_block(position pos);

    //
    // Caret motion; each takes whether to extend the selection.
    //
    void move_left(bool extend);
    void move_right(bool extend);
    void move_up(bool extend);
    void move_down(bool extend);
    void move_word_left(bool extend);
    void move_word_right(bool extend);
    void move_line_start(bool extend);
    void move_line_end(bool extend);
    void move_document_start(bool extend);
    void move_document_end(bool extend);

    // Move by a viewport height in pixels; negative moves up.
    void move_page(int pixels, bool extend);

    //
    // Editing
    //

    // Insert text at the caret, replacing any selection.
    void insert_text(std::u32string_view text);

    // Split the paragraph at the caret.
    void insert_paragraph();

    // Insert a forced break at the caret.
    void insert_break(doc::object_kind kind);

    // Insert image bytes as an inline object sized in layout units.
    void insert_image(std::string_view mime_type,
                      std::vector<std::uint8_t> bytes, util::lu width,
                      util::lu height);

    // Delete the selection, or the character before the caret.
    void delete_backward();

    // Delete the selection, or the character after the caret.
    void delete_forward();

    // Delete the selection, or back to the word start.
    void delete_word_backward();

    // Delete the selection, or forward to the word end.
    void delete_word_forward();

    // Undo the last action; false when there is none.
    bool undo();

    // Redo the last undone action; false when there is none.
    bool redo();

    //
    // Formatting
    //

    //
    // Return a character property at the caret.
    //
    // Notes:
    //      Formatting pending at a collapsed caret counts.
    //
    std::string char_property(std::string_view name) const;

    // Return a paragraph property at the caret.
    std::string block_property(std::string_view name) const;

    // Return the paragraph style name at the caret.
    std::string block_style() const;

    // Apply character properties to the selection or the next typing.
    void set_char_format(const doc::name_values &properties);

    // Flip a character property between a value and its opposite.
    void toggle_char_format(std::string_view name, std::string_view on,
                            std::string_view off);

    // Flip one word of a text-decoration value.
    void toggle_decoration(std::string_view word);

    // Apply paragraph properties to the selected paragraphs.
    void set_block_format(const doc::name_values &properties);

    // Apply section properties, such as page margins, at the caret.
    void set_section_format(const doc::name_values &properties);

    // Return a section property at the caret.
    std::string section_property(std::string_view name) const;

    // Apply a paragraph style to the selected paragraphs.
    void set_block_style(const std::string &name);

    //
    // Clipboard
    //

    // Return the selection as plain UTF-8.
    std::string selection_text() const;

    //
    // Capture the selection for pasting inside this application.
    //
    // Returns:
    //      Its plain text; pass that to paste_text later.
    //
    std::string copy();

    // Copy the selection, then delete it.
    std::string cut();

    //
    // Paste text from the system clipboard.
    //
    // Notes:
    //      When the text is what copy() last returned, the rich capture
    //      is pasted instead, so formatting survives inside the app.
    //
    void paste_text(std::string_view utf8);

    //
    // Search
    //

    // Select the next match after the caret, wrapping once.
    bool find_next(std::u32string_view needle, const find_options &options);

    // Replace the selection if it matches, then find the next match.
    bool replace_next(std::u32string_view needle,
                      std::u32string_view replacement,
                      const find_options &options);

    // Replace every match; returns how many.
    int replace_all(std::u32string_view needle,
                    std::u32string_view replacement,
                    const find_options &options);

    //
    // Geometry and painting
    //

    // Return the zoom factor, 1.0 being 100%.
    double zoom() const { return zoom_; }

    // Set the zoom factor.
    void set_zoom(double zoom);

    // Return device pixels per inch at the current zoom.
    double device_per_inch() const;

    // Set the width pages are centered in.
    void set_viewport_width(int width);

    // Return the total size of all pages in view pixels.
    int content_width() const;

    // Return the total height of all pages in view pixels.
    int content_height() const;

    // Return the caret rectangle in view pixels.
    gfx::device_rect caret_rect() const;

    // Return the document position under a view pixel.
    position position_at(int x, int y) const;

    // Return the 1-based page holding the caret.
    int current_page() const;

    // Return the left edge of the caret's page in view pixels.
    int page_left() const;

    //
    // Draw the visible part of the document.
    //
    // Parameters:
    //      target      - Destination, with its origin at the viewport.
    //      scroll_x    - View pixel at the viewport's left edge.
    //      scroll_y    - View pixel at the viewport's top edge.
    //      width       - Viewport width.
    //      height      - Viewport height.
    //
    void draw(gfx::painter &target, int scroll_x, int scroll_y, int width,
              int height) const;

    void on_change(const doc::change_record &record) override;
    void on_strux_removed(const doc::frag_strux &strux) override;

private:
    void apply_motion(position target, bool extend);
    void delete_selection();
    void changed();
    position step_left(position pos) const;
    position step_right(position pos) const;
    position clamp(position pos) const;
    position word_left(position pos) const;
    position word_right(position pos) const;
    std::u32string block_text(const fmt::block_layout &block) const;
    doc::attr_prop_index typing_format() const;
    const doc::attr_prop *section_format(position pos) const;
    int page_top(std::size_t index) const;
    int page_x(std::size_t index) const;
    void paste_plain(std::u32string_view text);
    std::optional<range> find_from(position start,
                                   std::u32string_view needle,
                                   const find_options &options,
                                   position limit) const;
    bool matches_selection(std::u32string_view needle,
                           const find_options &options) const;
    void draw_selection(gfx::painter &target, const fmt::page &sheet,
                        int origin_x, int origin_y) const;

    doc::document &document_;
    fmt::doc_layout layout_;
    position point_ = 2;
    position anchor_ = 2;

    // Horizontal position vertical motion tries to keep.
    std::optional<util::lu> sticky_x_;

    // Properties waiting for the next typed character.
    doc::name_values pending_;

    // End of the last document change, where undo leaves the caret.
    position last_change_ = 2;

    std::vector<clip_item> clip_;
    std::string clip_text_;

    double zoom_ = 1.0;
    int viewport_width_ = 0;
};

} // namespace edit

#endif
