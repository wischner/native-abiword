//
// The Find and Replace window, built from Native controls.
//
// Derived from AbiWord src/wp/ap/xp/ap_Dialog_Replace.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef UI_FIND_WINDOW_H
#define UI_FIND_WINDOW_H

#include <functional>
#include <string>

#include <native.h>

#include "document_canvas.h"

class desktop_test;

namespace ui {

// A non-blocking search window over one canvas.
class find_window final : public native::modeless_wnd {
public:
    // Create the window; it stays hidden until shown.
    find_window(native::app_wnd &owner, document_canvas &target,
                std::function<void(const std::string &)> report);
    ~find_window() override;

    // Reopen with focus on the search field.
    void open();

    // Restore search preferences without emitting user changes.
    void set_options(edit::find_options options);

    // Report immediately applied search preferences.
    native::signal<edit::find_options> on_options_change;

private:
    // Drives the window through Native's public input entry points.
    friend class ::desktop_test;

    void attach(native::wnd &child);
    void build_layout();
    void paint_labels(native::wnd_paint_event event);
    native::rect constrain_bounds(const native::rect &bounds) const override;
    void on_destroy_completed() override;
    edit::find_options options() const;
    void find();
    void replace();
    void replace_all();

    document_canvas &target_;
    std::function<void(const std::string &)> report_;
    native::text_edit needle_;
    native::text_edit replacement_;
    native::check match_case_;
    native::check whole_word_;
    native::button find_button_;
    native::button replace_button_;
    native::button all_button_;
    native::button close_button_;
    native::size minimum_;
    int margin_ = 12;
    int label_width_ = 0;
};

} // namespace ui

#endif
