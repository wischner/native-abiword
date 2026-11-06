//
// Input and painting for the editing surface. Key combinations map to
// AbiWord's edit method names; composed characters arrive separately
// through Native's text input path, so every layout and dead key works.
//
// Derived from AbiWord src/wp/ap/xp/ap_LB_Default.cpp and the
// EV_Mouse/EV_Keyboard dispatch in src/af/ev/xp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "document_canvas.h"

#include <algorithm>
#include <array>
#include <chrono>

#include "edit/commands.h"
#include "util/utf8.h"

namespace ui {
namespace {

using native::key_code;
using native::key_modifiers;

constexpr auto caret_interval = std::chrono::milliseconds(530);
constexpr native::rgba desk(136, 136, 136, 255);
constexpr int drag_scroll_step = 24;

// One entry of the default key binding table.
struct key_binding {
    key_code key;
    bool ctrl;
    bool shift;
    std::string_view command;
};

// Keys the menus do not already claim, from AbiWord's default bindings.
constexpr key_binding bindings[] = {
    {key_code::left, false, false, "warpInsPtLeft"},
    {key_code::left, false, true, "extSelLeft"},
    {key_code::left, true, false, "warpInsPtBOW"},
    {key_code::left, true, true, "extSelBOW"},
    {key_code::right, false, false, "warpInsPtRight"},
    {key_code::right, false, true, "extSelRight"},
    {key_code::right, true, false, "warpInsPtEOW"},
    {key_code::right, true, true, "extSelEOW"},
    {key_code::up, false, false, "warpInsPtLineUp"},
    {key_code::up, false, true, "extSelLineUp"},
    {key_code::down, false, false, "warpInsPtLineDown"},
    {key_code::down, false, true, "extSelLineDown"},
    {key_code::home, false, false, "warpInsPtBOL"},
    {key_code::home, false, true, "extSelBOL"},
    {key_code::home, true, false, "warpInsPtBOD"},
    {key_code::home, true, true, "extSelBOD"},
    {key_code::end, false, false, "warpInsPtEOL"},
    {key_code::end, false, true, "extSelEOL"},
    {key_code::end, true, false, "warpInsPtEOD"},
    {key_code::end, true, true, "extSelEOD"},
    {key_code::backspace, false, false, "delLeft"},
    {key_code::backspace, false, true, "delLeft"},
    {key_code::backspace, true, false, "delBOW"},
    {key_code::delete_key, false, false, "delRight"},
    {key_code::delete_key, true, false, "delEOW"},
    {key_code::enter, false, false, "insertParagraphBreak"},
    {key_code::keypad_enter, false, false, "insertParagraphBreak"},
    {key_code::enter, false, true, "insertLineBreak"},
    {key_code::enter, true, false, "insertPageBreak"},
    {key_code::tab, false, false, "insertTab"},
};

// Report whether a modifier bit is set.
bool held(key_modifiers state, key_modifiers bit) {
    return (state & bit) == bit;
}

} // namespace

document_canvas::document_canvas(font_catalog &fonts)
    : native::canvas(0, 0, 640, 480), fonts_(fonts) {
    set_cursor(native::mouse_cursor::ibeam);
    set_horizontal_scrollbar_policy(native::scrollbar_policy::automatic);
    set_vertical_scrollbar_policy(native::scrollbar_policy::always);
    on_wnd_paint.connect([this](native::wnd_paint_event event) {
        paint(event);
        return true;
    });
    on_mouse_click.connect([this](native::mouse_event event) {
        click(event);
        return true;
    });
    on_mouse_move.connect([this](native::point position) {
        drag(position);
        return true;
    });
    on_key.connect([this](native::key_event event) { return key(event); });
    on_text_input.connect(
        [this](const std::string &utf8) { return text(utf8); });
    on_scroll.connect([this](native::canvas_scroll_position) {
        scrolled();
        return true;
    });
    on_wnd_resize.connect([this](native::size) {
        sync_viewport();
        return true;
    });
    open(std::make_unique<doc::document>());
}

document_canvas::~document_canvas() {
    stop_caret();
}

void document_canvas::open(std::unique_ptr<doc::document> source) {
    view_.reset();
    document_ = std::move(source);
    view_ = std::make_unique<edit::view>(*document_, fonts_);
    images_.clear();
    set_scroll_position({0, 0});
    refresh();
}

void document_canvas::set_change_handler(std::function<void()> handler) {
    on_change_ = std::move(handler);
}

void document_canvas::set_command_handler(
    std::function<void(std::string_view)> handler) {
    on_command_ = std::move(handler);
}

native::point document_canvas::viewport_origin() const {
    return get_client_bounds().p;
}

void document_canvas::sync_viewport() {
    if (!view_)
        return;
    const native::rect client = get_client_bounds();
    view_->set_viewport_width(client.d.w);
    set_content_bounds(
        {0, 0, static_cast<std::uint32_t>(view_->content_width()),
         static_cast<std::uint32_t>(view_->content_height())});
    buffer_dirty_ = true;
    invalidate();
}

void document_canvas::refresh() {
    sync_viewport();
    reveal_caret();
    show_caret_now();
    if (on_change_)
        on_change_();
}

void document_canvas::scrolled() {
    buffer_dirty_ = true;
    invalidate();
    if (on_change_)
        on_change_();
}

void document_canvas::reveal_caret() {
    const gfx::device_rect caret = view_->caret_rect();
    const native::rect client = get_client_bounds();
    native::canvas_scroll_position scroll = get_scroll_position();
    const int margin = caret.height;
    if (caret.y < scroll.y + margin)
        scroll.y = caret.y - margin;
    else if (caret.y + caret.height + margin > scroll.y + client.d.h)
        scroll.y = caret.y + caret.height + margin - client.d.h;
    if (caret.x < scroll.x)
        scroll.x = caret.x - margin;
    else if (caret.x > scroll.x + client.d.w)
        scroll.x = caret.x + margin - client.d.w;
    set_scroll_position(scroll);
}

void document_canvas::show_caret_now() {
    caret_on_ = true;
}

void document_canvas::toggle_caret() {
    caret_on_ = !caret_on_;
    invalidate();
}

void document_canvas::start_caret(native::wnd &owner) {
    caret_scope_ = std::make_unique<native::ui_dispatch_scope>(owner);
    const native::ui_sender sender = caret_scope_->sender();
    caret_timer_ = std::jthread([this, sender](std::stop_token stop) {
        while (!stop.stop_requested()) {
            std::this_thread::sleep_for(caret_interval);
            sender.post_latest([this](native::wnd &) { toggle_caret(); });
        }
    });
}

void document_canvas::stop_caret() {
    if (caret_scope_)
        caret_scope_->close();
    if (caret_timer_.joinable()) {
        caret_timer_.request_stop();
        caret_timer_.join();
    }
    caret_scope_.reset();
}

void document_canvas::paint(native::wnd_paint_event event) {
    const native::rect client = get_client_bounds();
    if (client.d.w == 0 || client.d.h == 0)
        return;
    if (!buffer_ || buffer_->w() != static_cast<int>(client.d.w) ||
        buffer_->h() != static_cast<int>(client.d.h)) {
        buffer_ = std::make_unique<native::img>(client.d.w, client.d.h);
        buffer_dirty_ = true;
    }
    const native::canvas_scroll_position scroll = get_scroll_position();
    if (buffer_dirty_) {
        native::gpx &target = buffer_->get_gpx();
        target.set_clip(native::rect(0, 0, client.d.w, client.d.h));
        target.clear(desk);
        native_painter painter(target, view_->device_per_inch(), *document_,
                               images_);
        view_->draw(painter, scroll.x, scroll.y, client.d.w, client.d.h);
        buffer_dirty_ = false;
    }
    event.g.draw_img(*buffer_, client.p);
    if (!caret_on_ || view_->has_selection() || !get_focused())
        return;
    const gfx::device_rect caret = view_->caret_rect();
    const int x = caret.x - scroll.x + client.p.x;
    const int y = caret.y - scroll.y + client.p.y;
    if (y + caret.height < client.p.y || y > client.p.y + client.d.h)
        return;
    event.g.set_ink(native::rgba(0, 0, 0, 255)).set_pen(1);
    event.g.draw_line(
        native::point(static_cast<native::coord>(x),
                      static_cast<native::coord>(y)),
        native::point(static_cast<native::coord>(x),
                      static_cast<native::coord>(y + caret.height)));
}

edit::position document_canvas::hit(native::point position) const {
    const native::point origin = viewport_origin();
    const native::canvas_scroll_position scroll = get_scroll_position();
    return view_->position_at(position.x - origin.x + scroll.x,
                              position.y - origin.y + scroll.y);
}

void document_canvas::click(native::mouse_event event) {
    if (event.button != native::mouse_button::left)
        return;
    if (event.action == native::mouse_action::release) {
        dragging_ = false;
        return;
    }
    const edit::position pos = hit(event.position);
    dragging_ = true;
    drag_clicks_ = event.clicks;
    if (held(event.modifiers, key_modifiers::shift))
        view_->extend_to(pos);
    else if (event.clicks == 2)
        view_->select_word(pos);
    else if (event.clicks >= 3)
        view_->select_block(pos);
    else
        view_->set_point(pos);
    refresh();
}

void document_canvas::drag(native::point position) {
    if (!dragging_ || drag_clicks_ > 1)
        return;
    const native::rect client = get_client_bounds();
    native::canvas_scroll_position scroll = get_scroll_position();
    if (position.y < client.p.y)
        scroll.y -= drag_scroll_step;
    else if (position.y > client.p.y + client.d.h)
        scroll.y += drag_scroll_step;
    set_scroll_position(scroll);
    view_->extend_to(hit(position));
    buffer_dirty_ = true;
    show_caret_now();
    invalidate();
    if (on_change_)
        on_change_();
}

bool document_canvas::key(native::key_event event) {
    if (event.action != native::key_action::press)
        return false;
    const bool ctrl = held(event.modifiers, key_modifiers::ctrl);
    const bool shift = held(event.modifiers, key_modifiers::shift);
    if (event.key == key_code::page_up ||
        event.key == key_code::page_down) {
        const int height = get_client_bounds().d.h;
        view_->move_page(event.key == key_code::page_up ? -height : height,
                         shift);
        refresh();
        return true;
    }
    for (const key_binding &binding : bindings) {
        if (binding.key == event.key && binding.ctrl == ctrl &&
            binding.shift == shift) {
            if (on_command_)
                on_command_(binding.command);
            return true;
        }
    }
    return false;
}

bool document_canvas::text(const std::string &utf8) {
    std::u32string typed = util::to_utf32(utf8);
    std::erase_if(typed, [](char32_t c) { return c < 0x20 || c == 0x7f; });
    if (typed.empty())
        return false;
    view_->insert_text(typed);
    refresh();
    return true;
}

} // namespace ui
