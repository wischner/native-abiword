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

private:
    // Drives the window through Native's public input entry points.
    friend class ::desktop_test;

    void attach(native::wnd &child);
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
};

} // namespace ui

#endif
