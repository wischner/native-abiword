//
// The application window: menus, toolbar, format bar, rulers, status
// bar and the editing canvas, all Native controls.
//
// Derived from AbiWord src/wp/ap/xp/ap_Frame.cpp, ap_EditMethods.cpp,
// ap_Menu_Layouts.cpp, ap_Toolbar_Layouts.cpp and ap_StatusBar.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef UI_MAIN_WINDOW_H
#define UI_MAIN_WINDOW_H

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <native.h>

#include "app/options.h"
#include "document_canvas.h"
#include "find_window.h"
#include "impexp/file_format.h"
#include "native_graphics.h"

class desktop_test;

namespace ui {

// One document window.
class main_window final : public native::app_wnd {
public:
    // Build the window for the given command line.
    explicit main_window(const app::options &settings);

    ~main_window() override;

    void request_close() override;

    // Return the editing surface.
    native::canvas &canvas_probe() { return canvas_; }

private:
    // Drives the window through Native's public input entry points.
    friend class ::desktop_test;

    void build_menu();
    void build_toolbar();
    void build_format_bar();
    void place_children();
    void update_rulers();
    void ruler_marker_moved(native::ruler_marker_move move);
    void run(std::string_view command);
    bool run_window_command(std::string_view command);
    void update_state();
    void update_title();
    void set_message(const std::string &text);

    void new_document();
    void load(const std::filesystem::path &path);
    bool save();
    bool save_to(const std::filesystem::path &path);
    void open_requested();
    void save_as_requested();
    void insert_image_requested();
    bool confirm_discard();
    void write_clipboard(const std::string &text);
    void paste();
    void set_zoom(double zoom);
    void show_find();
    void show_about();

    app::options settings_;
    font_catalog fonts_;

    native::combo_box style_box_;
    native::combo_box font_box_;
    native::combo_box size_box_;
    native::combo_box color_box_;
    native::combo_box zoom_box_;
    document_canvas canvas_;
    native::toolbar toolbar_;
    native::toolbar format_bar_;
    native::ruler top_ruler_;
    native::ruler left_ruler_;
    native::status_bar status_;
    native::open_file_dialog open_dialog_;
    native::save_file_dialog save_dialog_;
    native::open_file_dialog image_dialog_;
    find_window find_;

    // Command name of each menu id; ids are indices plus one.
    std::vector<std::string> menu_commands_;

    std::string message_;
    bool updating_ = false;
};

} // namespace ui

#endif
