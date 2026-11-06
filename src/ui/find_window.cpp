//
// Search commands forwarded to the view.
//
// Derived from AbiWord src/wp/ap/xp/ap_Dialog_Replace.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "find_window.h"

#include "util/utf8.h"

namespace ui {

find_window::find_window(native::app_wnd &owner, document_canvas &target,
                         std::function<void(const std::string &)> report)
    : native::modeless_wnd(owner, "Find and Replace", 160, 160, 420, 150),
      target_(target), report_(std::move(report)), needle_("", native::text_edit_mode::single_line, 12, 12, 280, 26),
      replacement_("", native::text_edit_mode::single_line, 12, 44, 280, 26),
      match_case_("Match case", 12, 80, 130, 24),
      whole_word_("Whole word", 150, 80, 130, 24),
      find_button_("Find", 304, 12, 104, 28),
      replace_button_("Replace", 304, 46, 104, 28),
      all_button_("Replace All", 304, 80, 104, 28) {
    on_wnd_create.connect([this] {
        attach(needle_);
        attach(replacement_);
        attach(match_case_);
        attach(whole_word_);
        attach(find_button_);
        attach(replace_button_);
        attach(all_button_);
        return true;
    });
    find_button_.on_click.connect([this] {
        find();
        return true;
    });
    replace_button_.on_click.connect([this] {
        replace();
        return true;
    });
    all_button_.on_click.connect([this] {
        replace_all();
        return true;
    });
}

void find_window::attach(native::wnd &child) {
    child.set_parent(this);
    child.create();
    child.show();
}

edit::find_options find_window::options() const {
    return {match_case_.get_checked(), whole_word_.get_checked()};
}

void find_window::find() {
    if (!target_.view().find_next(util::to_utf32(needle_.get_text()),
                                  options()))
        report_("Not found: " + needle_.get_text());
    target_.refresh();
}

void find_window::replace() {
    if (!target_.view().replace_next(
            util::to_utf32(needle_.get_text()),
            util::to_utf32(replacement_.get_text()), options()))
        report_("Not found: " + needle_.get_text());
    target_.refresh();
}

void find_window::replace_all() {
    const int count = target_.view().replace_all(
        util::to_utf32(needle_.get_text()),
        util::to_utf32(replacement_.get_text()), options());
    report_("Replaced " + std::to_string(count) + " occurrences");
    target_.refresh();
}

} // namespace ui
