//
// Exercises the editing view headless: typing, motion, selection,
// formatting, undo, clipboard, search, and what painting emits.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <string>
#include <vector>

#include "check.h"
#include "edit/commands.h"
#include "edit/view.h"
#include "util/utf8.h"

namespace {

using namespace doc;
using namespace edit;

// A view over a fresh document.
struct fixture {
    document source;
    gfx::fixed_font_provider fonts;
    view target{source, fonts};

    std::u32string text() const {
        return source.table().text_in(0, source.table().end_position());
    }

    void run(std::string_view name) {
        CHECK(run_command(target, name));
    }
};

// Records drawing calls.
class recording_painter final : public gfx::painter {
public:
    std::vector<std::string> text;
    int fills = 0;

    double device_per_inch() const override { return 96; }
    void fill_rect(const gfx::device_rect &, util::color) override {
        ++fills;
    }
    void frame_rect(const gfx::device_rect &, util::color) override {
    }
    void draw_line(int, int, int, int, util::color, int) override {
    }
    void draw_text(const gfx::font &, std::u32string_view value, int, int,
                   util::color) override {
        text.push_back(util::to_utf8(value));
    }
    void draw_image(std::string_view, const gfx::device_rect &) override {
    }
};

void test_typing_and_paragraphs() {
    fixture f;
    f.target.insert_text(U"Hello");
    f.target.insert_paragraph();
    f.target.insert_text(U"World");
    CHECK(f.text() == U"Hello\nWorld");
    CHECK(f.target.point() == f.target.document_end());
    f.target.undo();
    CHECK(f.text() == U"Hello\n");
    f.target.undo();
    CHECK(f.text() == U"Hello");
    f.target.undo();
    CHECK(f.text().empty());
    CHECK(f.target.point() == f.target.document_start());
}

void test_backspace_joins_paragraphs() {
    fixture f;
    f.target.insert_text(U"ab");
    f.target.insert_paragraph();
    f.target.insert_text(U"cd");
    f.run("warpInsPtBOL");
    f.run("delLeft");
    CHECK(f.text() == U"abcd");
    CHECK(f.target.point() == f.target.document_start() + 2);
    f.run("delLeft");
    CHECK(f.text() == U"acd");
    // At the document's start backspace does nothing.
    f.run("warpInsPtBOD");
    f.run("delLeft");
    CHECK(f.text() == U"acd");
}

void test_delete_forward_joins_paragraphs() {
    fixture f;
    f.target.insert_text(U"ab");
    f.target.insert_paragraph();
    f.target.insert_text(U"cd");
    f.target.set_point(f.target.document_start() + 2);
    f.run("delRight");
    CHECK(f.text() == U"abcd");
}

void test_motion_crosses_blocks() {
    fixture f;
    f.target.insert_text(U"ab");
    f.target.insert_paragraph();
    f.target.insert_text(U"cd");
    const position end_of_first = f.target.document_start() + 2;
    f.target.set_point(end_of_first);
    f.run("warpInsPtRight");
    CHECK(f.target.point() == end_of_first + 1);
    f.run("warpInsPtLeft");
    CHECK(f.target.point() == end_of_first);
    f.run("warpInsPtLineDown");
    CHECK(f.target.point() == end_of_first + 3);
    f.run("warpInsPtLineUp");
    CHECK(f.target.point() == end_of_first);
}

void test_word_motion_and_selection() {
    fixture f;
    f.target.insert_text(U"one two  three");
    const position start = f.target.document_start();
    f.target.set_point(start);
    f.run("warpInsPtEOW");
    CHECK(f.target.point() == start + 4);
    f.run("warpInsPtEOW");
    CHECK(f.target.point() == start + 9);
    f.run("warpInsPtBOW");
    CHECK(f.target.point() == start + 4);
    f.target.select_word(start + 5);
    CHECK(f.target.selection_text() == "two");
    f.target.select_all();
    CHECK(f.target.selection_text() == "one two  three");
    f.run("delLeft");
    CHECK(f.text().empty());
}

void test_selection_replacement_is_one_undo() {
    fixture f;
    f.target.insert_text(U"hello world");
    f.target.select_word(f.target.document_start() + 1);
    f.target.insert_text(U"bye");
    CHECK(f.text() == U"bye world");
    f.target.undo();
    CHECK(f.text() == U"hello world");
}

void test_character_formatting() {
    fixture f;
    f.target.insert_text(U"abc");
    const position start = f.target.document_start();
    f.target.set_point(start + 1);
    f.target.extend_to(start + 2);
    f.run("toggleBold");
    f.target.set_point(start + 2);
    CHECK(f.target.char_property("font-weight") == "bold");
    f.target.set_point(start + 1);
    CHECK(f.target.char_property("font-weight") == "normal");
    // Toggling at a collapsed caret formats what is typed next.
    f.run("toggleItalic");
    CHECK(f.target.char_property("font-style") == "italic");
    f.target.insert_text(U"x");
    CHECK(f.target.char_property("font-style") == "italic");
    f.run("warpInsPtRight");
    f.run("warpInsPtRight");
    CHECK(f.target.char_property("font-style") == "normal");
    f.run("toggleUline");
    CHECK(f.target.char_property("text-decoration") == "underline");
    f.run("toggleStrike");
    CHECK(f.target.char_property("text-decoration") ==
          "underline line-through");
    f.run("toggleUline");
    CHECK(f.target.char_property("text-decoration") == "line-through");
}

void test_paragraph_formatting() {
    fixture f;
    f.target.insert_text(U"a");
    f.target.insert_paragraph();
    f.target.insert_text(U"b");
    f.target.select_all();
    f.run("alignCenter");
    CHECK(f.target.block_property("text-align") == "center");
    f.target.set_point(f.target.document_start());
    CHECK(f.target.block_property("text-align") == "center");
    f.run("marginIncrease");
    CHECK(util::to_lu(f.target.block_property("margin-left")) == 720);
}

void test_heading_style_followed_by_normal() {
    fixture f;
    f.target.insert_text(U"Title");
    f.target.set_block_style("Heading 1");
    CHECK(f.target.block_style() == "Heading 1");
    CHECK(f.target.char_property("font-size") == "17pt");
    f.target.insert_paragraph();
    CHECK(f.target.block_style() == "Normal");
    CHECK(f.target.char_property("font-size") == "12pt");
}

void test_clipboard() {
    fixture f;
    f.target.insert_text(U"plain");
    f.target.select_all();
    f.run("toggleBold");
    const std::string text = f.target.copy();
    CHECK(text == "plain");
    f.run("warpInsPtEOD");
    // An internal paste keeps formatting.
    f.target.paste_text(text);
    CHECK(f.text() == U"plainplain");
    CHECK(f.target.char_property("font-weight") == "bold");
    // Foreign text splits into paragraphs and is one undo step.
    f.target.paste_text("x\ny\r\nz");
    CHECK(f.text() == U"plainplainx\ny\nz");
    f.target.undo();
    CHECK(f.text() == U"plainplain");
    f.target.select_all();
    CHECK(f.target.cut() == "plainplain");
    CHECK(f.text().empty());
}

void test_multi_paragraph_copy() {
    fixture f;
    f.target.insert_text(U"ab");
    f.target.insert_paragraph();
    f.target.insert_text(U"cd");
    f.target.select_all();
    const std::string text = f.target.copy();
    CHECK(text == "ab\ncd");
    f.run("warpInsPtEOD");
    f.target.paste_text(text);
    CHECK(f.text() == U"ab\ncdab\ncd");
}

void test_find_and_replace() {
    fixture f;
    f.target.insert_text(U"cat Catalog");
    f.target.insert_paragraph();
    f.target.insert_text(U"a cat");
    f.target.set_point(f.target.document_start());
    CHECK(f.target.find_next(U"cat", {}));
    CHECK(f.target.selection_text() == "cat");
    CHECK(f.target.find_next(U"cat", {}));
    CHECK(f.target.selection_text() == "Cat");
    CHECK(f.target.find_next(U"cat", {true, true}));
    CHECK(f.target.selection().begin == f.target.document_start() + 14);
    // Search wraps.
    CHECK(f.target.find_next(U"cat", {true, true}));
    CHECK(f.target.selection().begin == f.target.document_start());
    CHECK(!f.target.find_next(U"dog", {}));
    CHECK(f.target.replace_all(U"cat", U"dog", {}) == 3);
    CHECK(f.text() == U"dog dogalog\na dog");
    f.target.undo();
    CHECK(f.text() == U"cat Catalog\na cat");
}

void test_replace_with_containing_text_terminates() {
    fixture f;
    f.target.insert_text(U"aa");
    CHECK(f.target.replace_all(U"a", U"aa", {}) == 2);
    CHECK(f.text() == U"aaaa");
}

void test_hit_testing_and_painting() {
    fixture f;
    f.target.set_viewport_width(1000);
    f.target.insert_text(U"Hello big world");
    const gfx::device_rect caret = f.target.caret_rect();
    CHECK(caret.x > 0);
    CHECK(f.target.position_at(caret.x, caret.y + 2) == f.target.point());
    recording_painter painter;
    f.target.draw(painter, 0, 0, 1000, 800);
    CHECK((painter.text ==
           std::vector<std::string>{"Hello", "big", "world"}));
    const int plain_fills = painter.fills;
    f.target.select_all();
    recording_painter selected;
    f.target.draw(selected, 0, 0, 1000, 800);
    CHECK(selected.fills == plain_fills + 1);
}

void test_zoom_keeps_layout() {
    fixture f;
    std::u32string text;
    for (int word = 0; word < 100; ++word)
        text += U"word ";
    f.target.insert_text(text);
    const auto lines = f.target.layout().blocks()[0]->lines().size();
    const int width = f.target.content_width();
    f.target.set_zoom(2.0);
    CHECK(f.target.layout().blocks()[0]->lines().size() == lines);
    CHECK(f.target.content_width() > width);
}

void test_unknown_command() {
    fixture f;
    CHECK(!run_command(f.target, "noSuchCommand"));
}

} // namespace

int main() {
    test_typing_and_paragraphs();
    test_backspace_joins_paragraphs();
    test_delete_forward_joins_paragraphs();
    test_motion_crosses_blocks();
    test_word_motion_and_selection();
    test_selection_replacement_is_one_undo();
    test_character_formatting();
    test_paragraph_formatting();
    test_heading_style_followed_by_normal();
    test_clipboard();
    test_multi_paragraph_copy();
    test_find_and_replace();
    test_replace_with_containing_text_terminates();
    test_hit_testing_and_painting();
    test_zoom_keeps_layout();
    test_unknown_command();
    return testing::summary("view");
}
