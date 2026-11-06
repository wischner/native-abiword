//
// Exercises formatting with the fixed font, whose advance of half an
// em at 12pt is exactly 120 layout units per character.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <string>

#include "check.h"
#include "fmt/doc_layout.h"
#include "impexp/file_format.h"

namespace {

using namespace doc;
using namespace fmt;

constexpr lu char_width = 120;

// A document with its layout.
struct fixture {
    document source;
    gfx::fixed_font_provider fonts;
    doc_layout layout{source, fonts};

    piece_table &table() { return source.table(); }

    void set_block(const name_values &properties) {
        table().change_strux_format(2, table().end_position(),
                                    strux_kind::block, format_op::add, {},
                                    properties);
        layout.update();
    }
};

void test_empty_document() {
    fixture f;
    CHECK(f.layout.pages().size() == 1);
    CHECK(f.layout.pages()[0].lines.size() == 1);
    const caret_location caret = f.layout.locate(2);
    CHECK(caret.x == f.layout.pages()[0].margin_left);
    CHECK(caret.height == 240);
}

void test_wrapping_keeps_words_whole() {
    fixture f;
    // A 6.5in text column holds 78 characters.
    std::u32string text;
    for (int word = 0; word < 40; ++word)
        text += U"word ";
    f.table().insert_text(2, text, empty_attr_prop);
    f.layout.update();
    const auto &lines = f.layout.blocks()[0]->lines();
    CHECK(lines.size() == 3);
    // 15 words fit, their ink ending at 74; the 16th would end at 79.
    CHECK(lines[0]->end_offset() == 75);
    CHECK(lines[1]->first_offset() == 75);
}

void test_long_word_breaks_at_character() {
    fixture f;
    f.table().insert_text(2, std::u32string(100, U'x'), empty_attr_prop);
    f.layout.update();
    const auto &lines = f.layout.blocks()[0]->lines();
    CHECK(lines.size() == 2);
    CHECK(lines[0]->end_offset() == 78);
}

void test_trailing_spaces_hang() {
    fixture f;
    std::u32string text(78, U'a');
    text += U"    b";
    f.table().insert_text(2, text, empty_attr_prop);
    f.layout.update();
    const auto &lines = f.layout.blocks()[0]->lines();
    CHECK(lines.size() == 2);
    CHECK(lines[1]->first_offset() == 82);
}

void test_alignment() {
    fixture f;
    f.table().insert_text(2, U"ab", empty_attr_prop);
    f.set_block({{"text-align", "right"}});
    const lu column = 9360;
    CHECK(f.layout.blocks()[0]->lines()[0]->x() ==
          column - 2 * char_width);
    f.set_block({{"text-align", "center"}});
    CHECK(f.layout.blocks()[0]->lines()[0]->x() ==
          (column - 2 * char_width) / 2);
}

void test_justify_fills_the_line() {
    fixture f;
    std::u32string text;
    for (int word = 0; word < 40; ++word)
        text += U"word ";
    f.table().insert_text(2, text, empty_attr_prop);
    f.set_block({{"text-align", "justify"}});
    const auto &lines = f.layout.blocks()[0]->lines();
    const line &first = *lines[0];
    // The last word's ink ends exactly at the right margin.
    CHECK(first.x_of_offset(first.end_offset() - 1) == 9360);
    // The last line stays ragged.
    CHECK(lines.back()->content_width() < 9360);
}

void test_tabs() {
    fixture f;
    f.table().insert_text(2, U"a\tb", empty_attr_prop);
    f.layout.update();
    const line &first = *f.layout.blocks()[0]->lines()[0];
    CHECK(first.x_of_offset(2) == 720);
    f.set_block({{"tabstops", "3in/L0"}});
    CHECK(f.layout.blocks()[0]->lines()[0]->x_of_offset(2) == 4320);
    f.set_block({{"tabstops", "3in/R0"}});
    const line &right = *f.layout.blocks()[0]->lines()[0];
    // The text after a right tab ends at the stop.
    CHECK(right.x_of_offset(3) == 4320);
}

void test_margins_and_indent() {
    fixture f;
    f.table().insert_text(2, std::u32string(100, U'x'), empty_attr_prop);
    f.set_block({{"margin-left", "1in"}, {"text-indent", "0.5in"}});
    const auto &lines = f.layout.blocks()[0]->lines();
    CHECK(lines[0]->x() == 2160);
    CHECK(lines[1]->x() == 1440);
}

void test_line_height() {
    fixture f;
    f.table().insert_text(2, U"x", empty_attr_prop);
    f.set_block({{"line-height", "2.0"}});
    CHECK(f.layout.blocks()[0]->lines()[0]->height() == 480);
    f.set_block({{"line-height", "30pt"}});
    CHECK(f.layout.blocks()[0]->lines()[0]->height() == 600);
    f.set_block({{"line-height", "6pt+"}});
    CHECK(f.layout.blocks()[0]->lines()[0]->height() == 240);
}

// Append many paragraphs of one line each.
void fill_paragraphs(fixture &f, int count) {
    for (int index = 0; index < count; ++index) {
        f.table().insert_text(f.table().end_position(), U"line",
                              empty_attr_prop);
        f.table().insert_strux(f.table().end_position(), strux_kind::block,
                               empty_attr_prop);
    }
    f.layout.update();
}

void test_pagination() {
    fixture f;
    // 11in page with 1in margins gives 9in, or 54 lines of 240.
    fill_paragraphs(f, 54);
    CHECK(f.layout.pages().size() == 2);
    CHECK(f.layout.pages()[0].lines.size() == 54);
    CHECK(f.layout.pages()[1].lines.size() == 1);
    CHECK(f.layout.pages()[0].lines[0].y == 1440);
}

void test_forced_page_break() {
    fixture f;
    f.table().insert_text(2, U"a", empty_attr_prop);
    f.table().insert_object(3, object_kind::forced_page_break,
                            empty_attr_prop);
    f.table().insert_text(4, U"b", empty_attr_prop);
    f.layout.update();
    CHECK(f.layout.pages().size() == 2);
    CHECK(f.layout.locate(4).page == 1);
    CHECK(f.layout.locate(2).page == 0);
}

void test_widows_and_orphans() {
    fixture f;
    fill_paragraphs(f, 52);
    // A four-line paragraph with two lines of room left stays split
    // 2/2; with one line left it must move entirely.
    f.table().insert_text(f.table().end_position(),
                          std::u32string(78 * 4, U'x'), empty_attr_prop);
    f.layout.update();
    CHECK(f.layout.pages()[0].lines.size() == 54);
    CHECK(f.layout.pages()[1].lines.size() == 2);

    fixture g;
    fill_paragraphs(g, 53);
    g.table().insert_text(g.table().end_position(),
                          std::u32string(78 * 4, U'x'), empty_attr_prop);
    g.layout.update();
    CHECK(g.layout.pages()[0].lines.size() == 53);
    CHECK(g.layout.pages()[1].lines.size() == 4);
}

void test_keep_with_next() {
    fixture f;
    fill_paragraphs(f, 53);
    f.table().insert_text(f.table().end_position(), U"Title",
                          empty_attr_prop);
    f.set_block({});
    f.table().change_strux_format(
        f.table().end_position() - 1, f.table().end_position(),
        strux_kind::block, format_op::add, {}, {{"keep-with-next", "1"}});
    f.table().insert_strux(f.table().end_position(), strux_kind::block,
                           empty_attr_prop);
    f.table().insert_text(f.table().end_position(), U"Body",
                          empty_attr_prop);
    f.layout.update();
    CHECK(f.layout.pages()[0].lines.size() == 53);
    CHECK(f.layout.pages()[1].lines.size() == 2);
}

void test_incremental_matches_full_layout() {
    fixture f;
    f.table().insert_text(2, U"first", empty_attr_prop);
    f.table().insert_strux(7, strux_kind::block, empty_attr_prop);
    f.table().insert_text(8, U"second", empty_attr_prop);
    f.layout.update();
    CHECK(f.layout.blocks().size() == 2);
    // Join, undo, redo: blocks follow every structural change.
    f.table().delete_range(7, 8);
    f.layout.update();
    CHECK(f.layout.blocks().size() == 1);
    CHECK(f.layout.blocks()[0]->lines()[0]->end_offset() == 11);
    f.table().undo();
    f.layout.update();
    CHECK(f.layout.blocks().size() == 2);
    CHECK(f.layout.block_at(9) == f.layout.blocks()[1].get());
    CHECK(f.layout.block_at(3) == f.layout.blocks()[0].get());
    f.table().redo();
    f.layout.update();
    CHECK(f.layout.blocks().size() == 1);
}

void test_position_round_trip() {
    fixture f;
    std::u32string text;
    for (int word = 0; word < 60; ++word)
        text += U"word ";
    f.table().insert_text(2, text, empty_attr_prop);
    f.layout.update();
    for (position pos = 2; pos < f.table().end_position(); pos += 7) {
        const caret_location caret = f.layout.locate(pos);
        CHECK(f.layout.position_at(caret.page, caret.x, caret.y + 1) == pos);
    }
}

void test_vertical_motion_keeps_column() {
    fixture f;
    f.table().insert_text(2, std::u32string(200, U'x'), empty_attr_prop);
    f.layout.update();
    const lu x = f.layout.locate(12).x;
    const position below = f.layout.move_lines(12, x, 1);
    CHECK(below == 12 + 78);
    CHECK(f.layout.move_lines(below, x, -1) == 12);
    // Motion stops at the document's ends.
    CHECK(f.layout.move_lines(12, x, -5) == 12);
}

void test_real_file_lays_out() {
    document source;
    CHECK(impexp::load_file(FIXTURES "/tabs.abw", source).ok);
    gfx::fixed_font_provider fonts;
    doc_layout layout(source, fonts);
    CHECK(!layout.pages().empty());
    CHECK(!layout.blocks().empty());
}

} // namespace

int main() {
    test_empty_document();
    test_wrapping_keeps_words_whole();
    test_long_word_breaks_at_character();
    test_trailing_spaces_hang();
    test_alignment();
    test_justify_fills_the_line();
    test_tabs();
    test_margins_and_indent();
    test_line_height();
    test_pagination();
    test_forced_page_break();
    test_widows_and_orphans();
    test_keep_with_next();
    test_incremental_matches_full_layout();
    test_position_round_trip();
    test_vertical_motion_keeps_column();
    test_real_file_lays_out();
    return testing::summary("layout");
}
