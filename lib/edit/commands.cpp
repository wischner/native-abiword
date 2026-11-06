//
// The command table. Names are AbiWord's edit method names, so its key
// binding tables carry over directly. Binary search keeps lookups free
// of allocation on every keystroke.
//
// Derived from AbiWord src/wp/ap/xp/ap_EditMethods.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "edit/commands.h"

#include <algorithm>
#include <iterator>
#include <string>

namespace edit {
namespace {

using doc::object_kind;

// One named command.
struct command {
    std::string_view name;
    void (*run)(view &);
};

// Change the font size at the caret by whole points.
void grow_font(view &target, int points) {
    const double size =
        util::to_points(target.char_property("font-size"));
    const int next = std::max(1, static_cast<int>(size) + points);
    target.set_char_format({{"font-size", std::to_string(next) + "pt"}});
}

// Set a paragraph's left margin relative to its current value.
void shift_left_margin(view &target, double inches) {
    const double current =
        util::to_inches(target.block_property("margin-left"));
    target.set_block_format(
        {{"margin-left",
          util::format_inches(std::max(0.0, current + inches))}});
}

// Keep sorted by name: run_command binary searches this.
constexpr command commands[] = {
    {"alignCenter", [](view &v) { v.set_block_format({{"text-align", "center"}}); }},
    {"alignJustify", [](view &v) { v.set_block_format({{"text-align", "justify"}}); }},
    {"alignLeft", [](view &v) { v.set_block_format({{"text-align", "left"}}); }},
    {"alignRight", [](view &v) { v.set_block_format({{"text-align", "right"}}); }},
    {"delBOW", [](view &v) { v.delete_word_backward(); }},
    {"delEOW", [](view &v) { v.delete_word_forward(); }},
    {"delLeft", [](view &v) { v.delete_backward(); }},
    {"delRight", [](view &v) { v.delete_forward(); }},
    {"extSelBOD", [](view &v) { v.move_document_start(true); }},
    {"extSelBOL", [](view &v) { v.move_line_start(true); }},
    {"extSelBOW", [](view &v) { v.move_word_left(true); }},
    {"extSelEOD", [](view &v) { v.move_document_end(true); }},
    {"extSelEOL", [](view &v) { v.move_line_end(true); }},
    {"extSelEOW", [](view &v) { v.move_word_right(true); }},
    {"extSelLeft", [](view &v) { v.move_left(true); }},
    {"extSelLineDown", [](view &v) { v.move_down(true); }},
    {"extSelLineUp", [](view &v) { v.move_up(true); }},
    {"extSelRight", [](view &v) { v.move_right(true); }},
    {"fontSizeDown", [](view &v) { grow_font(v, -1); }},
    {"fontSizeUp", [](view &v) { grow_font(v, 1); }},
    {"insertColumnBreak", [](view &v) { v.insert_break(object_kind::forced_column_break); }},
    {"insertLineBreak", [](view &v) { v.insert_break(object_kind::forced_line_break); }},
    {"insertPageBreak", [](view &v) { v.insert_break(object_kind::forced_page_break); }},
    {"insertParagraphBreak", [](view &v) { v.insert_paragraph(); }},
    {"insertTab", [](view &v) { v.insert_text(U"\t"); }},
    {"marginDecrease", [](view &v) { shift_left_margin(v, -0.5); }},
    {"marginIncrease", [](view &v) { shift_left_margin(v, 0.5); }},
    {"redo", [](view &v) { v.redo(); }},
    {"selectAll", [](view &v) { v.select_all(); }},
    {"spacing1", [](view &v) { v.set_block_format({{"line-height", "1.0"}}); }},
    {"spacing15", [](view &v) { v.set_block_format({{"line-height", "1.5"}}); }},
    {"spacing2", [](view &v) { v.set_block_format({{"line-height", "2.0"}}); }},
    {"toggleBold", [](view &v) { v.toggle_char_format("font-weight", "bold", "normal"); }},
    {"toggleItalic", [](view &v) { v.toggle_char_format("font-style", "italic", "normal"); }},
    {"toggleOline", [](view &v) { v.toggle_decoration("overline"); }},
    {"toggleStrike", [](view &v) { v.toggle_decoration("line-through"); }},
    {"toggleSubscript", [](view &v) { v.toggle_char_format("text-position", "subscript", "normal"); }},
    {"toggleSuperscript", [](view &v) { v.toggle_char_format("text-position", "superscript", "normal"); }},
    {"toggleUline", [](view &v) { v.toggle_decoration("underline"); }},
    {"undo", [](view &v) { v.undo(); }},
    {"warpInsPtBOD", [](view &v) { v.move_document_start(false); }},
    {"warpInsPtBOL", [](view &v) { v.move_line_start(false); }},
    {"warpInsPtBOW", [](view &v) { v.move_word_left(false); }},
    {"warpInsPtEOD", [](view &v) { v.move_document_end(false); }},
    {"warpInsPtEOL", [](view &v) { v.move_line_end(false); }},
    {"warpInsPtEOW", [](view &v) { v.move_word_right(false); }},
    {"warpInsPtLeft", [](view &v) { v.move_left(false); }},
    {"warpInsPtLineDown", [](view &v) { v.move_down(false); }},
    {"warpInsPtLineUp", [](view &v) { v.move_up(false); }},
    {"warpInsPtRight", [](view &v) { v.move_right(false); }},
};

} // namespace

bool run_command(view &target, std::string_view name) {
    const auto end = std::end(commands);
    const auto found = std::lower_bound(
        std::begin(commands), end, name,
        [](const command &entry, std::string_view key) {
            return entry.name < key;
        });
    if (found == end || found->name != name)
        return false;
    found->run(target);
    return true;
}

} // namespace edit
