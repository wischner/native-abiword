//
// The editing surface: a Native canvas showing an edit::view. It turns
// pointer and keyboard input into view operations and paints through a
// back buffer, so a blinking caret never re-rasterizes text.
//
// Derived from AbiWord src/wp/ap/xp/ap_FrameData and the
// EV_Mouse/EV_Keyboard dispatch in src/af/ev/xp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef UI_DOCUMENT_CANVAS_H
#define UI_DOCUMENT_CANVAS_H

#include <functional>
#include <memory>
#include <stop_token>
#include <thread>

#include <native.h>

#include "edit/view.h"
#include "native_graphics.h"

class desktop_test;

namespace ui {

// Shows and edits one document.
class document_canvas final : public native::canvas {
public:
    // Create a canvas drawing with a catalog's fonts.
    explicit document_canvas(font_catalog &fonts);

    // Stop the caret timer.
    ~document_canvas() override;

    // Show a document, taking ownership.
    void open(std::unique_ptr<doc::document> source);

    // Return the view being edited.
    edit::view &view() { return *view_; }

    // Return the document being edited.
    doc::document &document() { return *document_; }

    // Bring scrolling and painting up to date after an edit.
    void refresh();

    // Set what runs after anything the user changes.
    void set_change_handler(std::function<void()> handler);

    // Set what runs when a key combination names a command.
    void set_command_handler(std::function<void(std::string_view)> handler);

    // Start the caret timer once the window exists.
    void start_caret(native::wnd &owner);

    // Stop the caret timer before the window goes away.
    void stop_caret();

private:
    // Drives the window through Native's public input entry points.
    friend class ::desktop_test;

    void paint(native::wnd_paint_event event);
    void click(native::mouse_event event);
    void drag(native::point position);
    bool key(native::key_event event);
    bool text(const std::string &utf8);
    void scrolled();
    void reveal_caret();
    void show_caret_now();
    void toggle_caret();
    void sync_viewport();
    edit::position hit(native::point position) const;
    native::point viewport_origin() const;

    font_catalog &fonts_;
    image_cache images_;
    std::unique_ptr<doc::document> document_;
    std::unique_ptr<edit::view> view_;
    std::unique_ptr<native::img> buffer_;
    bool buffer_dirty_ = true;
    bool caret_on_ = true;
    bool dragging_ = false;
    int drag_clicks_ = 1;
    std::function<void()> on_change_;
    std::function<void(std::string_view)> on_command_;
    std::unique_ptr<native::ui_dispatch_scope> caret_scope_;
    std::jthread caret_timer_;
};

} // namespace ui

#endif
