//
// Commands that need the window: files, clipboard, dialogs and zoom.
// Everything else goes straight to edit::run_command, so a menu item, a
// toolbar button and a key binding all run the same code.
//
// Derived from AbiWord src/wp/ap/xp/ap_EditMethods.cpp and
// ap_Menu_Layouts.cpp, Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <fstream>
#include <iterator>

#include "edit/commands.h"
#include "impexp/file_format.h"
#include "main_window.h"

namespace ui {
namespace {

// One menu row; an empty command is a separator.
struct menu_row {
    const char *label;
    const char *command;
};

// One top-level menu.
struct menu_group {
    const char *title;
    std::vector<menu_row> rows;
};

// The menu bar, following AbiWord's layout.
const std::vector<menu_group> &menu_groups() {
    static const std::vector<menu_group> groups = {
        {"&File",
         {{"&New\tCtrl+N", "fileNew"},
          {"&Open...\tCtrl+O", "fileOpen"},
          {"&Save\tCtrl+S", "fileSave"},
          {"Save &As...\tCtrl+Shift+S", "fileSaveAs"},
          {"", ""},
          {"E&xit\tCtrl+Q", "quit"}}},
        {"&Edit",
         {{"&Undo\tCtrl+Z", "undo"},
          {"&Redo\tCtrl+Y", "redo"},
          {"", ""},
          {"Cu&t\tCtrl+X", "cut"},
          {"&Copy\tCtrl+C", "copy"},
          {"&Paste\tCtrl+V", "paste"},
          {"", ""},
          {"Select &All\tCtrl+A", "selectAll"},
          {"&Find and Replace...\tCtrl+F", "find"}}},
        {"&View",
         {{"Zoom &In\tCtrl+=", "zoomIn"},
          {"Zoom &Out\tCtrl+-", "zoomOut"},
          {"&Actual Size\tCtrl+0", "zoomReset"}}},
        {"&Insert",
         {{"&Page Break", "insertPageBreak"},
          {"&Column Break", "insertColumnBreak"},
          {"&Line Break", "insertLineBreak"},
          {"", ""},
          {"&Image...", "insertImage"}}},
        {"F&ormat",
         {{"&Bold\tCtrl+B", "toggleBold"},
          {"&Italic\tCtrl+I", "toggleItalic"},
          {"&Underline\tCtrl+U", "toggleUline"},
          {"&Overline", "toggleOline"},
          {"&Strikethrough", "toggleStrike"},
          {"Su&perscript", "toggleSuperscript"},
          {"Su&bscript", "toggleSubscript"},
          {"", ""},
          {"Align &Left\tCtrl+L", "alignLeft"},
          {"Align Ce&nter\tCtrl+E", "alignCenter"},
          {"Align &Right\tCtrl+R", "alignRight"},
          {"&Justify\tCtrl+J", "alignJustify"},
          {"", ""},
          {"Single Spacing", "spacing1"},
          {"1.5 Spacing", "spacing15"},
          {"Double Spacing", "spacing2"},
          {"", ""},
          {"&Increase Indent", "marginIncrease"},
          {"&Decrease Indent", "marginDecrease"}}},
        {"&Help", {{"&About Writer", "about"}}},
    };
    return groups;
}

// Return a MIME type for an image file.
std::string_view mime_of(const std::filesystem::path &path) {
    return path.extension() == ".png" ? "image/png" : "image/jpeg";
}

} // namespace

void main_window::build_menu() {
    for (const menu_group &group : menu_groups()) {
        native::menu_items_proxy items;
        for (const menu_row &row : group.rows) {
            if (row.label[0] == '\0') {
                items << native::menu_separator;
                continue;
            }
            menu_commands_.emplace_back(row.command);
            items << std::make_pair(static_cast<int>(menu_commands_.size()),
                                    std::string(row.label));
        }
        menu << group.title << items;
    }
}

void main_window::run(std::string_view command) {
    message_.clear();
    if (run_window_command(command))
        return;
    if (edit::run_command(canvas_.view(), command))
        canvas_.refresh();
}

bool main_window::run_window_command(std::string_view command) {
    if (command == "fileNew") {
        new_document();
    } else if (command == "fileOpen") {
        open_requested();
    } else if (command == "fileSave") {
        save();
    } else if (command == "fileSaveAs") {
        save_as_requested();
    } else if (command == "quit") {
        request_close();
    } else if (command == "cut") {
        write_clipboard(canvas_.view().cut());
        canvas_.refresh();
    } else if (command == "copy") {
        write_clipboard(canvas_.view().copy());
    } else if (command == "paste") {
        paste();
    } else if (command == "find") {
        show_find();
    } else if (command == "insertImage") {
        image_dialog_.create();
        image_dialog_.show();
    } else if (command == "zoomIn") {
        set_zoom(canvas_.view().zoom() * 1.25);
    } else if (command == "zoomOut") {
        set_zoom(canvas_.view().zoom() / 1.25);
    } else if (command == "zoomReset") {
        set_zoom(1.0);
    } else if (command == "about") {
        show_about();
    } else {
        return false;
    }
    return true;
}

bool main_window::confirm_discard() {
    if (!canvas_.document().table().history().is_dirty())
        return true;
    switch (native::message_box::show(
        *this, "Save changes to this document?", "Writer",
        native::message_box_buttons::yes_no_cancel,
        native::message_box_icon::question)) {
    case native::message_box_result::yes:
        return save();
    case native::message_box_result::no:
        return true;
    default:
        return false;
    }
}

void main_window::request_close() {
    if (confirm_discard())
        native::app_wnd::request_close();
}

void main_window::new_document() {
    if (!confirm_discard())
        return;
    canvas_.open(std::make_unique<doc::document>());
}

void main_window::open_requested() {
    if (!confirm_discard())
        return;
    open_dialog_.create();
    open_dialog_.show();
}

void main_window::load(const std::filesystem::path &path) {
    auto source = std::make_unique<doc::document>();
    const impexp::import_result result = impexp::load_file(path, *source);
    if (!result.ok) {
        native::message_box::show(*this, result.error, "Could not open",
                                  native::message_box_buttons::ok,
                                  native::message_box_icon::error);
        return;
    }
    canvas_.open(std::move(source));
    if (!result.warnings.empty())
        set_message(result.warnings.front());
}

bool main_window::save() {
    const std::filesystem::path &path = canvas_.document().path();
    if (path.empty()) {
        save_as_requested();
        return false;
    }
    return save_to(path);
}

bool main_window::save_to(const std::filesystem::path &path) {
    std::string error;
    if (!impexp::save_file(path, canvas_.document(), error)) {
        native::message_box::show(*this, error, "Could not save",
                                  native::message_box_buttons::ok,
                                  native::message_box_icon::error);
        return false;
    }
    const impexp::file_format *format = impexp::format_for(path);
    // Text and HTML are exports: the document stays what it was.
    if (format && format->make_importer && format->extension != "txt") {
        canvas_.document().set_path(path);
        canvas_.document().table().history().mark_saved();
    }
    set_message("Saved " + path.filename().string());
    return true;
}

void main_window::save_as_requested() {
    const std::filesystem::path &path = canvas_.document().path();
    save_dialog_.set_suggested_name(
        path.empty() ? "Untitled.abw" : path.filename().string());
    save_dialog_.create();
    save_dialog_.show();
}

void main_window::insert_image_requested() {
    const std::filesystem::path &path = image_dialog_.get_path();
    std::ifstream stream(path, std::ios::binary);
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream),
                                    std::istreambuf_iterator<char>()};
    try {
        const native::img image = native::img::decode(bytes.data(),
                                                      bytes.size());
        // Screen pixels are 96 to the inch; a wide image fits the text.
        const double to_lu = util::layout_resolution / 96.0;
        const double limit = 6.0 * util::layout_resolution;
        const double scale = std::min(1.0, limit / (image.w() * to_lu));
        canvas_.view().insert_image(
            mime_of(path), std::move(bytes),
            static_cast<util::lu>(image.w() * to_lu * scale),
            static_cast<util::lu>(image.h() * to_lu * scale));
        canvas_.refresh();
    } catch (const std::exception &error) {
        native::message_box::show(*this, error.what(), "Could not insert",
                                  native::message_box_buttons::ok,
                                  native::message_box_icon::error);
    }
}

void main_window::write_clipboard(const std::string &text) {
    if (text.empty())
        return;
    native::clipboard board = native::clipboard::open_write();
    board.write_text(text);
    board.commit();
}

void main_window::paste() {
    const native::clipboard board = native::clipboard::open_read();
    if (!board.has(native::clipboard_format::text))
        return;
    canvas_.view().paste_text(board.read_text());
    canvas_.refresh();
}

void main_window::set_zoom(double zoom) {
    canvas_.view().set_zoom(zoom);
    canvas_.refresh();
}

void main_window::show_find() {
    if (!find_.get_created())
        find_.create();
    find_.show();
}

void main_window::show_about() {
    native::message_box::show(
        *this,
        "Writer " FYODOR_VERSION " (fyodor)\n\n"
        "A word processor rebuilt from AbiWord on the Native UI library.\n"
        "Licensed under the GNU GPL, version 2 or later.",
        "About Writer", native::message_box_buttons::ok,
        native::message_box_icon::information);
}

} // namespace ui
