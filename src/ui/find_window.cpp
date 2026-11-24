//
// Search commands forwarded to the view.
//
// Derived from AbiWord src/wp/ap/xp/ap_Dialog_Replace.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "find_window.h"

#include <algorithm>
#include <array>

#include "util/utf8.h"

namespace ui {

find_window::find_window(native::app_wnd &owner, document_canvas &target,
                         std::function<void(const std::string &)> report)
    : native::modeless_wnd(owner, "Find and Replace", 160, 160, 480, 200),
      target_(target), report_(std::move(report)),
      match_case_("Match case"), whole_word_("Whole word"),
      find_button_("Find"), replace_button_("Replace"),
      all_button_("Replace All"), close_button_("Close") {
    needle_.set_enter_as_tab(false);
    replacement_.set_enter_as_tab(false);
    unsigned order = 1;
    for (auto *control : std::array<native::wnd *, 8>{
             &needle_, &replacement_, &match_case_, &whole_word_,
             &find_button_, &replace_button_, &all_button_, &close_button_}) {
        control->set_tab_order(order++);
        control->on_key.connect([this, control](native::key_event event) {
            if ((event.key != native::key_code::enter &&
                 event.key != native::key_code::keypad_enter) ||
                (event.modifiers & (native::key_modifiers::ctrl |
                    native::key_modifiers::alt | native::key_modifiers::meta)) !=
                    native::key_modifiers::none)
                return false;
            if (event.action == native::key_action::press && !event.repeat) {
                if (auto *button = dynamic_cast<native::button *>(control))
                    button->on_native_click();
                else
                    find_button_.on_native_click();
            }
            return true;
        });
    }
    on_wnd_create.connect([this] {
        build_layout();
        attach(needle_);
        attach(replacement_);
        attach(match_case_);
        attach(whole_word_);
        attach(find_button_);
        attach(replace_button_);
        attach(all_button_);
        attach(close_button_);
        return true;
    });
    on_wnd_resize.connect([this](native::size size) {
        if (size.w < minimum_.w || size.h < minimum_.h)
            set_dimensions({std::max(size.w, minimum_.w),
                            std::max(size.h, minimum_.h)});
        invalidate();
        return true;
    });
    on_wnd_paint.connect([this](native::wnd_paint_event event) {
        paint_labels(event);
        return true;
    });
    for (auto *control : {&match_case_, &whole_word_})
        control->on_change.connect([this](bool) {
            on_options_change.emit(options());
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
    close_button_.on_click.connect([this] { destroy(); return true; });
}

find_window::~find_window() { destroy(); }

void find_window::on_destroy_completed() {
    target_.focus();
}

void find_window::open() {
    if (!get_created())
        create();
    if (const auto *owner = get_owner()) {
        const auto bounds = owner->get_bounds();
        const auto dimensions = get_dimensions();
        set_position({static_cast<native::coord>(
                          bounds.x1() + (int(bounds.w()) - int(dimensions.w)) / 2),
                      static_cast<native::coord>(
                          bounds.y1() + (int(bounds.h()) - int(dimensions.h)) / 2)});
    }
    show();
    needle_.focus();
}

void find_window::set_options(edit::find_options value) {
    match_case_.set_checked(value.match_case);
    whole_word_.set_checked(value.whole_word);
}

native::rect find_window::constrain_bounds(const native::rect &bounds) const {
    return {bounds.p, {std::max(bounds.w(), minimum_.w),
                       std::max(bounds.h(), minimum_.h)}};
}

void find_window::build_layout() {
    const auto appearance = native::theme::create(get_gpx());
    const auto &font = native::font_t::stock(native::font_role::control);
    const int text_height = font.measure_text("Replace with:").height;
    margin_ = std::max(12, text_height);
    const int gap = std::max(6, margin_ / 2);
    label_width_ = font.measure_text("Replace with:").width;
    const int field_height = std::max(appearance->get_form_control_height(),
                                      appearance->get_text_edit_height());
    const int check_height = std::max(appearance->get_check_height(),
                                      text_height + gap);
    const int button_height = std::max(appearance->get_button_height(),
                                       text_height + gap);
    const int button_width = font.measure_text("Replace All").width +
                             2 * margin_;
    const int content_width = std::max({4 * button_width + 3 * gap,
        label_width_ + gap + font.measure_text(std::string(24, 'M')).width,
        2 * (font.measure_text("Whole word").width + check_height) + gap});
    minimum_ = native::size(content_width + 2 * margin_,
        2 * margin_ + 2 * field_height + check_height + button_height + 3 * gap);

    auto grid = std::make_unique<native::grid_layout_manager>();
    for (int height : {margin_, field_height, gap, field_height, gap,
                       check_height, gap, button_height, margin_})
        grid->add_row(native::pixels(height));
    grid->add_row(native::star());
    grid->add_column(native::pixels(margin_));
    grid->add_column(native::star());
    grid->add_column(native::pixels(margin_));
    auto fields = std::make_unique<native::grid_layout_manager>();
    fields->add_row(native::pixels(field_height));
    fields->add_row(native::pixels(gap));
    fields->add_row(native::pixels(field_height));
    fields->add_column(native::pixels(label_width_ + gap));
    fields->add_column(native::star());
    fields->add(needle_, 0, 1).add(replacement_, 2, 1);
    grid->add_child_grid(std::move(fields), 1, 1, 3);
    auto checks = std::make_unique<native::grid_layout_manager>();
    checks->add_row(native::star());
    checks->add_column(native::star()).add_column(native::pixels(gap))
          .add_column(native::star());
    checks->add(match_case_, 0, 0).add(whole_word_, 0, 2);
    grid->add_child_grid(std::move(checks), 5, 1);
    auto buttons = std::make_unique<native::grid_layout_manager>();
    buttons->add_row(native::star());
    int column = 0;
    for (auto *button : {&find_button_, &replace_button_, &all_button_,
                         &close_button_}) {
        if (column) buttons->add_column(native::pixels(gap));
        buttons->add_column(native::star());
        buttons->add(*button, 0, column);
        column += 2;
    }
    grid->add_child_grid(std::move(buttons), 7, 1);
    set_layout(std::move(grid));
    set_dimensions(minimum_);
}

void find_window::paint_labels(native::wnd_paint_event event) {
    auto saved = event.g.save_state();
    event.g.set_font(native::font_t::stock(native::font_role::control));
    event.g.set_ink(native::theme::create(event.g)->native_palette().content_text);
    const auto label = [&](const char *text, const native::text_edit &field) {
        const auto bounds = field.get_bounds();
        event.g.draw_text(text,
            native::rect(get_client_bounds().x1() + margin_, bounds.y1(),
                         label_width_, bounds.h()),
            {native::text_align::start, native::text_valign::center,
             native::text_overflow::clip, true});
    };
    label("Find:", needle_);
    label("Replace with:", replacement_);
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
