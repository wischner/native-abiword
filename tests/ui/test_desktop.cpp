//
// Drives the real application window through Native's public input
// entry points, then verifies the saved file, the view and the painted
// pixels. Nothing here names a toolkit, so one source runs on every
// Native backend; a backend supplies only its display.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stop_token>
#include <string>
#include <thread>

#include <unistd.h>

#include "check.h"
#include "impexp/file_format.h"
#include "main_window.h"

namespace fs = std::filesystem;
using native::key_code;
using native::key_modifiers;

// Runs the scenario against one window.
class desktop_test {
public:
    explicit desktop_test(ui::main_window &window)
        : window_(window), canvas_(window.canvas_),
          path_(fs::temp_directory_path() /
                ("fyodor-test-" + std::to_string(getpid()) + ".abw")) {
    }

    ~desktop_test() {
        std::error_code ignored;
        fs::remove(path_, ignored);
    }

    // Run everything that needs no fresh paint.
    void run_edits() {
        CHECK(canvas_.get_created());
        save_new_document();
        test_typing();
        test_formatting_keys();
        test_undo();
        test_selection_and_clipboard();
        test_toolbar_and_menu();
        test_format_bar();
        test_mouse();
        test_rulers();
        test_find();
        test_page_break();
        test_load_real_file();
        window_.run("zoomReset");
        window_.canvas_.view().set_point(
            window_.canvas_.view().document_start());
        canvas_.refresh();
    }

    // Verify what the last paint produced.
    void check_paint() {
        const native::img *buffer = canvas_.buffer_.get();
        CHECK(buffer != nullptr);
        if (!buffer)
            return;
        const native::rgba *pixels = buffer->pixels();
        int dark = 0;
        int white = 0;
        const int count = buffer->w() * buffer->h();
        for (int index = 0; index < count; ++index) {
            if (pixels[index].r < 90 && pixels[index].g < 90 &&
                pixels[index].b < 90)
                ++dark;
            else if (pixels[index].r == 255 && pixels[index].g == 255)
                ++white;
        }
        CHECK(white > count / 4);
        CHECK(dark > 200);
        std::cout << "painted " << buffer->w() << 'x' << buffer->h() << ", "
                  << dark << " ink pixels\n";
    }

    // Report editing speed; real fonts make this the useful number.
    void measure_typing() {
        edit::view &view = canvas_.view();
        view.move_document_end(false);
        const auto start = std::chrono::steady_clock::now();
        constexpr int keys = 300;
        for (int index = 0; index < keys; ++index)
            canvas_.on_native_text_input("x");
        const std::chrono::duration<double, std::milli> elapsed =
            std::chrono::steady_clock::now() - start;
        std::cout << "typing: " << elapsed.count() / keys
                  << " ms per key, without painting\n";
        window_.run("undo");
    }

private:
    void type(const std::string &utf8) {
        canvas_.on_native_text_input(utf8);
    }

    void press(key_code key, key_modifiers modifiers = key_modifiers::none) {
        native::key_event event;
        event.key = key;
        event.action = native::key_action::press;
        event.modifiers = modifiers;
        canvas_.on_native_key(event);
        event.action = native::key_action::release;
        canvas_.on_native_key(event);
    }

    void menu(std::string_view command) {
        const auto &commands = window_.menu_commands_;
        const auto found = std::ranges::find(commands, command);
        CHECK(found != commands.end());
        window_.on_native_menu(
            static_cast<int>(found - commands.begin()) + 1);
    }

    // Click at a view pixel the way a backend would deliver it.
    void click_view(int x, int y) {
        const native::rect client = canvas_.get_client_bounds();
        const auto scroll = canvas_.get_scroll_position();
        const native::point at(
            static_cast<native::coord>(x - scroll.x + client.p.x),
            static_cast<native::coord>(y - scroll.y + client.p.y));
        native::mouse_event event(native::mouse_button::left,
                                  native::mouse_action::press, at);
        canvas_.on_native_mouse_click(event);
        event.action = native::mouse_action::release;
        canvas_.on_native_mouse_click(event);
    }

    std::string saved() {
        menu("fileSave");
        std::ifstream stream(path_, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream),
                           std::istreambuf_iterator<char>());
    }

    bool file_has(const std::string &needle) {
        return saved().find(needle) != std::string::npos;
    }

    void save_new_document() {
        CHECK(window_.save_to(path_));
        CHECK(window_.get_title() == path_.filename().string() + " - Writer");
    }

    void test_typing() {
        type("Hello world");
        CHECK(window_.get_title().starts_with("*"));
        press(key_code::enter);
        type("Second žé€");
        CHECK(file_has("Hello world</p>"));
        CHECK(file_has("Second žé€</p>"));
        CHECK(!window_.get_title().starts_with("*"));
        press(key_code::backspace);
        CHECK(file_has("Second žé</p>"));
    }

    void test_formatting_keys() {
        menu("toggleBold");
        type(" bold");
        menu("toggleBold");
        type(" plain");
        CHECK(file_has("<c props=\"font-weight:bold\"> bold</c>"));
        CHECK(file_has("<c props=\"font-weight:normal\"> plain</c></p>"));
    }

    void test_undo() {
        const std::size_t before = saved().size();
        type("XYZ");
        menu("undo");
        CHECK(saved().size() == before);
        menu("redo");
        CHECK(file_has("XYZ</c></p>"));
        menu("undo");
    }

    void test_selection_and_clipboard() {
        const auto control = key_modifiers::ctrl;
        const auto extend = key_modifiers::shift;
        press(key_code::home, control);
        press(key_code::end, extend);
        CHECK(canvas_.view().selection_text() == "Hello world");
        menu("copy");
        press(key_code::end, control);
        press(key_code::enter);
        menu("paste");
        const std::string text = saved();
        std::size_t count = 0;
        for (std::size_t at = text.find("Hello world");
             at != std::string::npos;
             at = text.find("Hello world", at + 1))
            ++count;
        CHECK(count == 2);
    }

    void test_toolbar_and_menu() {
        press(key_code::home, key_modifiers::ctrl);
        window_.format_bar_.on_native_command("alignCenter");
        CHECK(file_has("text-align:center"));
        CHECK(window_.format_bar_.get_checked("alignCenter"));
        CHECK(!window_.format_bar_.get_checked("alignLeft"));
        menu("alignRight");
        CHECK(file_has("text-align:right"));
        CHECK(window_.format_bar_.get_checked("alignRight"));
        menu("alignLeft");
    }

    void test_format_bar() {
        native::combo_box &styles = window_.style_box_;
        const auto &names = styles.get_items();
        const auto heading = std::ranges::find(names, "Heading 1");
        CHECK(heading != names.end());
        styles.on_native_selection(
            static_cast<int>(heading - names.begin()));
        CHECK(canvas_.view().block_style() == "Heading 1");
        CHECK(window_.size_box_.get_text() == "17");
        CHECK(file_has("style=\"Heading 1\""));
        CHECK(window_.font_box_.get_text() == "Arial");
    }

    void test_mouse() {
        edit::view &view = canvas_.view();
        const edit::position start = view.document_start();
        view.set_point(start + 2);
        const gfx::device_rect caret = view.caret_rect();
        press(key_code::end);
        click_view(caret.x, caret.y + 2);
        CHECK(view.point() == start + 2);
        // Native counts quick presses into a double and a triple click.
        click_view(caret.x, caret.y + 2);
        CHECK(view.selection_text() == "Hello");
        click_view(caret.x, caret.y + 2);
        CHECK(view.selection_text() == "Hello world");
        // Shift held through a click, far enough away to be a new gesture.
        view.set_point(start);
        native::key_event shift;
        shift.key = key_code::left_shift;
        shift.action = native::key_action::press;
        shift.modifiers = key_modifiers::shift;
        canvas_.on_native_key(shift);
        click_view(caret.x + 40, caret.y + 2);
        shift.action = native::key_action::release;
        shift.modifiers = key_modifiers::none;
        canvas_.on_native_key(shift);
        CHECK(view.selection().begin == start);
        CHECK(view.has_selection());
        view.set_point(start);
    }

    // Drag a marker through the same pointer path a backend uses.
    void drag_marker(native::ruler &strip, int id, int pixels, int across) {
        const auto &markers = strip.get_markers();
        const auto marker = std::ranges::find(
            markers, id, &native::ruler_marker::id);
        CHECK(marker != markers.end());
        const native::rect bounds = strip.get_bounds();
        const double from =
            (marker->value - strip.get_origin()) / strip.get_units_per_pixel();
        const native::point start(
            static_cast<native::coord>(bounds.x1() + from),
            static_cast<native::coord>(bounds.y1() + across));
        const native::point end(
            static_cast<native::coord>(bounds.x1() + from + pixels),
            static_cast<native::coord>(bounds.y1() + across));
        canvas_.on_native_mouse_click(native::mouse_event(
            native::mouse_button::left, native::mouse_action::press, start));
        canvas_.on_native_mouse_move(end);
        canvas_.on_native_mouse_click(native::mouse_event(
            native::mouse_button::left, native::mouse_action::release, end));
    }

    void test_rulers() {
        edit::view &view = canvas_.view();
        // The strip's bottom third grabs the block-left marker, its middle
        // the page margin; 96 pixels is one inch at 100%.
        const int bottom = window_.top_ruler_.get_bounds().h() - 3;
        const int middle = window_.top_ruler_.get_bounds().h() / 2;
        drag_marker(window_.top_ruler_, 5, 96, bottom);
        CHECK(view.block_property("margin-left").starts_with("1.0000"));
        drag_marker(window_.top_ruler_, 5, -96, bottom);
        CHECK(view.block_property("margin-left").starts_with("0.0000"));
        // A page margin reflows the section.
        drag_marker(window_.top_ruler_, 1, 48, middle);
        CHECK(view.section_property("page-margin-left").starts_with("1.5000"));
        CHECK(view.layout().pages()[0].margin_left == 2160);
        drag_marker(window_.top_ruler_, 1, -48, middle);
    }

    void test_find() {
        menu("find");
        ui::find_window &finder = window_.find_;
        CHECK(finder.get_created());
        finder.needle_.set_text("hello");
        finder.replacement_.set_text("Howdy");
        finder.find_button_.on_native_click();
        CHECK(canvas_.view().selection_text() == "Hello");
        finder.all_button_.on_native_click();
        CHECK(file_has("Howdy world"));
        CHECK(!file_has("Hello world"));
        finder.destroy();
    }

    void test_page_break() {
        const std::size_t pages = canvas_.view().layout().pages().size();
        press(key_code::end, key_modifiers::ctrl);
        press(key_code::enter, key_modifiers::ctrl);
        CHECK(canvas_.view().layout().pages().size() == pages + 1);
        CHECK(canvas_.view().current_page() == 2);
        CHECK(file_has("<pbr/>"));
    }

    void test_load_real_file() {
        // A cross build runs elsewhere, so the fixture can be redirected.
        const char *folder = std::getenv("FYODOR_FIXTURES");
        window_.load(fs::path(folder ? folder : FIXTURES) / "Gettysburg.abw");
        CHECK(canvas_.document().path().filename() == "Gettysburg.abw");
        CHECK(canvas_.view().layout().pages().size() >= 1);
        const auto lines = canvas_.view().layout().blocks()[0]->lines().size();
        menu("zoomIn");
        menu("zoomIn");
        CHECK(canvas_.view().zoom() > 1.5);
        CHECK(canvas_.view().layout().blocks()[0]->lines().size() == lines);
    }

    ui::main_window &window_;
    ui::document_canvas &canvas_;
    fs::path path_;
};

namespace {

// Longest wait for the backend's first paint.
constexpr auto paint_timeout = std::chrono::seconds(10);

// Time between paint checks. Checks come from a timer, because work
// that keeps reposting itself can starve a backend's own event queue.
constexpr auto paint_poll = std::chrono::milliseconds(50);

// Finish once the canvas has painted past a count, or at the deadline.
class paint_waiter {
public:
    paint_waiter(desktop_test &test, ui::main_window &window,
                 const int &paints)
        : test_(test), window_(window), paints_(paints) {
    }

    // Start waiting for a paint after the current one.
    void start() {
        seen_ = paints_;
        deadline_ = std::chrono::steady_clock::now() + paint_timeout;
        timer_ = std::jthread([this](std::stop_token stop) {
            while (!stop.stop_requested()) {
                std::this_thread::sleep_for(paint_poll);
                native::app::post([this] { poll(); });
            }
        });
    }

    // Stop the timer.
    void stop() {
        if (timer_.joinable()) {
            timer_.request_stop();
            timer_.join();
        }
    }

private:
    void poll() {
        if (finished_ || (paints_ == seen_ &&
                          std::chrono::steady_clock::now() < deadline_))
            return;
        finished_ = true;
        CHECK(paints_ != seen_);
        test_.check_paint();
        test_.measure_typing();
        window_.destroy();
    }

    desktop_test &test_;
    ui::main_window &window_;
    const int &paints_;
    int seen_ = 0;
    bool finished_ = false;
    std::chrono::steady_clock::time_point deadline_;
    std::jthread timer_;
};

} // namespace

int program(int, char **) {
    app::options settings;
    ui::main_window window(settings);
    desktop_test test(window);
    int paints = 0;
    paint_waiter waiter(test, window, paints);
    window.canvas_probe().on_wnd_paint.connect(
        [&paints](native::wnd_paint_event) {
            ++paints;
            return false;
        });
    window.on_wnd_create.connect([&] {
        native::app::post([&] {
            test.run_edits();
            window.canvas_probe().invalidate();
            waiter.start();
        });
        return false;
    });
    const int status = native::app::run(window);
    waiter.stop();
    return status ? status : testing::summary("desktop");
}
