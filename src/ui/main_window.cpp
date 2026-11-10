//
// Window construction and state display. After every change the
// controls are re-read from the view, so toolbar, format bar and status
// bar cannot disagree with the document.
//
// Derived from AbiWord src/wp/ap/xp/ap_Frame.cpp, ap_Menu_Layouts.cpp,
// ap_Toolbar_Layouts.cpp and ap_StatusBar.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "main_window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "tool_icons.h"
#include "util/units.h"

namespace ui {
namespace {

constexpr int ruler_extent = 22;

// A label and its document value for the color choices.
struct color_choice {
    const char *label;
    const char *value;
};

constexpr std::array<color_choice, 8> colors{{{"Black", "000000"},
                                              {"Gray", "808080"},
                                              {"Red", "c0392b"},
                                              {"Orange", "d35400"},
                                              {"Green", "1e8449"},
                                              {"Blue", "1f4e9e"},
                                              {"Purple", "76448a"},
                                              {"White", "ffffff"}}};

constexpr std::array<int, 16> font_sizes{8,  9,  10, 11, 12, 14, 16, 18,
                                         20, 22, 24, 28, 32, 36, 48, 72};

constexpr std::array<int, 6> zoom_levels{50, 75, 100, 125, 150, 200};

// One toolbar entry; an empty command is a separator.
struct tool_entry {
    const char *command;
    native::tool_kind kind;
    const char *group;
};

constexpr std::array<tool_entry, 11> tools{{
    {"fileNew", native::tool_kind::button, ""},
    {"fileOpen", native::tool_kind::button, ""},
    {"fileSave", native::tool_kind::button, ""},
    {"", native::tool_kind::separator, ""},
    {"undo", native::tool_kind::button, ""},
    {"redo", native::tool_kind::button, ""},
    {"", native::tool_kind::separator, ""},
    {"cut", native::tool_kind::button, ""},
    {"copy", native::tool_kind::button, ""},
    {"paste", native::tool_kind::button, ""},
    {"find", native::tool_kind::button, ""},
}};

// The format bar's plain tools, after its hosted combo boxes.
constexpr std::array<tool_entry, 9> format_tools{{
    {"toggleBold", native::tool_kind::toggle, ""},
    {"toggleItalic", native::tool_kind::toggle, ""},
    {"toggleUline", native::tool_kind::toggle, ""},
    {"toggleStrike", native::tool_kind::toggle, ""},
    {"", native::tool_kind::separator, ""},
    {"alignLeft", native::tool_kind::exclusive, "align"},
    {"alignCenter", native::tool_kind::exclusive, "align"},
    {"alignRight", native::tool_kind::exclusive, "align"},
    {"alignJustify", native::tool_kind::exclusive, "align"},
}};

// Ruler marker identities; tab stops follow from first_tab_marker.
enum ruler_marker_id {
    marker_page_left = 1,
    marker_page_right,
    marker_page_top,
    marker_page_bottom,
    marker_block_left,
    marker_block_first,
    marker_block_right,
    first_tab_marker = 100
};

// Marker drags land on sixteenths of an inch.
constexpr double marker_snap = 1.0 / 16.0;

// Return a tool item for one table entry.
native::tool_item make_tool(const tool_entry &entry) {
    native::tool_item item;
    item.id = entry.command;
    item.kind = entry.kind;
    item.group = entry.group;
    item.icon = tool_icon(entry.command);
    return item;
}

// Join hosted fields into one strip with a single divider per control.
native::tool_item make_control(const char *id, native::wnd &control,
                               int length) {
    native::tool_item item;
    item.id = id;
    item.kind = native::tool_kind::control;
    item.control = &control;
    item.length = length;
    item.control_fill_width = true;
    control.set_border_sides(native::border_sides::right);
    return item;
}

// Reserve one square tool slot between command groups.
void size_toolbar_separators(native::toolbar &bar, int padding) {
    auto items = bar.get_items();
    for (auto &item : items) {
        if (item.kind == native::tool_kind::separator)
            item.length = bar.get_extent() - 2 * padding;
    }
    bar.set_items(std::move(items));
}

// Return layout units as inches.
double inches(util::lu value) {
    return static_cast<double>(value) / util::layout_resolution;
}

// These Native setters repaint even when the value is unchanged. Keep
// ordinary typing from repainting the surrounding controls and rulers.
void update_checked(native::toolbar &bar, const std::string &id, bool value) {
    if (bar.get_checked(id) != value)
        bar.set_checked(id, value);
}

void update_enabled(native::toolbar &bar, const std::string &id, bool value) {
    const auto &items = bar.get_items();
    const auto item = std::ranges::find(items, id, &native::tool_item::id);
    if (item != items.end() && item->enabled != value)
        bar.set_enabled(id, value);
}

// Synchronize ruler state without invalidating unchanged decorations.
void update_ruler(native::ruler &ruler,
                  std::optional<std::pair<double, double>> &range,
                  double scale, double origin, double first, double last,
                  std::vector<native::ruler_marker> markers) {
    if (ruler.get_units_per_pixel() != scale)
        ruler.set_units_per_pixel(scale);
    if (ruler.get_origin() != origin)
        ruler.set_origin(origin);
    const auto next_range = std::make_pair(first, last);
    if (range != next_range) {
        ruler.set_active_range(first, last);
        range = next_range;
    }
    if (!std::ranges::equal(ruler.get_markers(), markers,
                           [](const auto &left, const auto &right) {
                               return left.id == right.id &&
                                      left.value == right.value &&
                                      left.shape == right.shape &&
                                      left.movable == right.movable;
                           }))
        ruler.set_markers(std::move(markers));
}

// Return the items of the zoom box.
std::vector<std::string> zoom_labels() {
    std::vector<std::string> result;
    for (const int level : zoom_levels)
        result.push_back(std::to_string(level) + "%");
    return result;
}

} // namespace

main_window::main_window(const app::options &settings)
    : native::app_wnd("Fyodor", 60, 40, 1000, 760), settings_(settings),
      style_box_(doc::builtin_style_names(),
                 native::combo_box_style::drop_down_list),
      font_box_(fonts_.families(), native::combo_box_style::drop_down_list),
      size_box_({}, native::combo_box_style::drop_down_list),
      color_box_({}, native::combo_box_style::drop_down_list),
      zoom_box_(zoom_labels(), native::combo_box_style::drop_down_list),
      canvas_(fonts_), toolbar_(*this), format_bar_(*this),
      top_ruler_(canvas_, native::window_edge::top, ruler_extent),
      left_ruler_(canvas_, native::window_edge::left, ruler_extent),
      status_(*this, canvas_), open_dialog_(*this, "Open"),
      save_dialog_(*this, "Save As"),
      image_dialog_(*this, "Insert Image"),
      find_(*this, canvas_, [this](const std::string &text) {
          set_message(text);
      }) {
    canvas_.set_parent(this);
    set_scroll_content(&canvas_);
    build_menu();
    build_toolbar();
    build_format_bar();
    canvas_.set_change_handler([this] { update_state(); });
    canvas_.set_command_handler(
        [this](std::string_view command) { run(command); });

    for (const impexp::file_format &format : impexp::file_formats())
        if (format.make_importer)
            open_dialog_.add_filter(
                {format.name, {"*." + format.extension}});
    open_dialog_.add_filter({"All files", {"*"}});
    for (const impexp::file_format &format : impexp::file_formats())
        if (format.make_exporter)
            save_dialog_.add_filter(
                {format.name, {"*." + format.extension}});
    save_dialog_.set_default_extension("abw");
    image_dialog_.add_filter({"Images", {"*.png", "*.jpg", "*.jpeg"}});
    open_dialog_.on_modal_close.connect([this](native::dialog_result result) {
        if (result == native::dialog_result::accepted)
            load(open_dialog_.get_path());
        return true;
    });
    save_dialog_.on_modal_close.connect([this](native::dialog_result result) {
        if (result == native::dialog_result::accepted)
            save_to(save_dialog_.get_path());
        return true;
    });
    image_dialog_.on_modal_close.connect(
        [this](native::dialog_result result) {
            if (result != native::dialog_result::accepted)
                return true;
            insert_image_requested();
            return true;
        });

    on_wnd_create.connect([this] {
        const auto appearance = native::theme::create(get_gpx());
        // Native also reserves the hosted fields' natural height/insets.
        toolbar_.set_extent(appearance->get_toolbar_height());
        format_bar_.set_extent(appearance->get_toolbar_height());
        size_toolbar_separators(toolbar_, appearance->defaults().toolbar_padding);
        size_toolbar_separators(format_bar_, appearance->defaults().toolbar_padding);
        status_.set_extent(appearance->get_status_bar_height());
        canvas_.create();
        canvas_.show();
        place_children();
        canvas_.start_caret(*this);
        if (settings_.file)
            load(*settings_.file);
        canvas_.refresh();
        canvas_.focus();
        return true;
    });
    on_wnd_resize.connect([this](native::size) {
        place_children();
        return true;
    });
    on_menu.connect([this](int id) {
        const auto index = static_cast<std::size_t>(id - 1);
        if (index < menu_commands_.size())
            run(menu_commands_[index]);
        return true;
    });
}

main_window::~main_window() {
    canvas_.stop_caret();
}

void main_window::build_toolbar() {
    for (const tool_entry &entry : tools)
        toolbar_.add_item(make_tool(entry));
    toolbar_.add_item(make_control("zoomBox", zoom_box_, 80));
    const auto run_tool = [this](native::tool_command command) {
        run(command.id);
        return true;
    };
    toolbar_.on_command.connect(run_tool);
    format_bar_.on_command.connect(run_tool);
}

void main_window::build_format_bar() {
    format_bar_.add_item(make_control("styleBox", style_box_, 150));
    format_bar_.add_item(make_control("fontBox", font_box_, 200));
    format_bar_.add_item(make_control("sizeBox", size_box_, 64));
    format_bar_.add_item(make_control("colorBox", color_box_, 90));
    for (const tool_entry &entry : format_tools)
        format_bar_.add_item(make_tool(entry));
    top_ruler_.set_minor_tick(0.125).set_major_tick(1.0);
    left_ruler_.set_minor_tick(0.125).set_major_tick(1.0);
    top_ruler_.set_marker_snap(marker_snap);
    left_ruler_.set_marker_snap(marker_snap);
    const auto moved = [this](native::ruler_marker_move move) {
        if (move.finished)
            ruler_marker_moved(move);
        return true;
    };
    top_ruler_.on_marker_move.connect(moved);
    left_ruler_.on_marker_move.connect(moved);

    for (const int size : font_sizes)
        size_box_.add_item(std::to_string(size));
    for (const color_choice &choice : colors)
        color_box_.add_item(choice.label);
    color_box_.set_selected_index(0);

    style_box_.on_selection_change.connect([this](int index) {
        if (updating_ || index < 0)
            return true;
        canvas_.view().set_block_style(style_box_.get_items()[index]);
        canvas_.refresh();
        return true;
    });
    font_box_.on_selection_change.connect([this](int index) {
        if (updating_ || index < 0)
            return true;
        canvas_.view().set_char_format(
            {{"font-family", font_box_.get_items()[index]}});
        canvas_.refresh();
        return true;
    });
    size_box_.on_selection_change.connect([this](int index) {
        if (updating_ || index < 0)
            return true;
        canvas_.view().set_char_format(
            {{"font-size", size_box_.get_items()[index] + "pt"}});
        canvas_.refresh();
        return true;
    });
    color_box_.on_selection_change.connect([this](int index) {
        if (updating_ || index < 0)
            return true;
        canvas_.view().set_char_format(
            {{"color", colors[static_cast<std::size_t>(index)].value}});
        canvas_.refresh();
        return true;
    });
    zoom_box_.on_selection_change.connect([this](int index) {
        if (updating_ || index < 0)
            return true;
        set_zoom(zoom_levels[static_cast<std::size_t>(index)] / 100.0);
        return true;
    });
}

void main_window::place_children() {
    canvas_.set_bounds(get_client_bounds());
}

void main_window::ruler_marker_moved(native::ruler_marker_move move) {
    edit::view &view = canvas_.view();
    const fmt::page &sheet =
        view.layout().pages()[static_cast<std::size_t>(view.current_page() - 1)];
    const double page_width = inches(sheet.width);
    const double page_height = inches(sheet.height);
    const double text_left = inches(sheet.margin_left);
    const double text_right = page_width - inches(sheet.margin_right);
    const double indent_left =
        util::to_inches(view.block_property("margin-left"));
    const auto length = [](double value) {
        return util::format_inches(std::max(0.0, value));
    };
    switch (move.id) {
    case marker_page_left:
        view.set_section_format({{"page-margin-left", length(move.value)}});
        break;
    case marker_page_right:
        view.set_section_format(
            {{"page-margin-right", length(page_width - move.value)}});
        break;
    case marker_page_top:
        view.set_section_format({{"page-margin-top", length(move.value)}});
        break;
    case marker_page_bottom:
        view.set_section_format(
            {{"page-margin-bottom", length(page_height - move.value)}});
        break;
    case marker_block_left:
        view.set_block_format(
            {{"margin-left", length(move.value - text_left)}});
        break;
    case marker_block_first:
        // Negative for a hanging indent, so no clamping here.
        view.set_block_format(
            {{"text-indent",
              util::format_inches(move.value - text_left - indent_left)}});
        break;
    case marker_block_right:
        view.set_block_format(
            {{"margin-right", length(text_right - move.value)}});
        break;
    default: {
        auto stops = fmt::parse_tab_stops(view.block_property("tabstops"));
        const auto index =
            static_cast<std::size_t>(move.id - first_tab_marker);
        if (index >= stops.size())
            break;
        stops[index].position = static_cast<util::lu>(std::lround(
            (move.value - text_left) * util::layout_resolution));
        std::string value;
        for (const fmt::tab_stop &stop : stops) {
            if (!value.empty())
                value.push_back(',');
            value += util::format_lu(stop.position) + '/';
            value.push_back(stop.kind == fmt::tab_kind::right    ? 'R'
                            : stop.kind == fmt::tab_kind::center ? 'C'
                                                                 : 'L');
            value.push_back('0');
        }
        view.set_block_format({{"tabstops", value}});
        break;
    }
    }
    canvas_.refresh();
}

void main_window::update_rulers() {
    edit::view &view = canvas_.view();
    const fmt::page &sheet =
        view.layout().pages()[static_cast<std::size_t>(view.current_page() - 1)];
    const double dpi = view.device_per_inch();
    const native::canvas_scroll_position scroll =
        canvas_.get_scroll_position();
    const double page_width = inches(sheet.width);
    const double page_height = inches(sheet.height);
    const double text_left = inches(sheet.margin_left);
    const double text_right = page_width - inches(sheet.margin_right);
    const double text_top = util::to_inches(
        view.section_property("page-margin-top"));
    const double text_bottom = page_height -
        util::to_inches(view.section_property("page-margin-bottom"));
    const int page_y = view.caret_rect().y - scroll.y -
        util::lu_to_device(view.layout().locate(view.point()).y, dpi);

    using shape = native::ruler_marker_shape;
    const double indent_left =
        text_left + util::to_inches(view.block_property("margin-left"));
    const double indent_first =
        indent_left + util::to_inches(view.block_property("text-indent"));
    const double indent_right =
        text_right - util::to_inches(view.block_property("margin-right"));
    std::vector<native::ruler_marker> markers{
        {marker_page_left, text_left, shape::block, true},
        {marker_page_right, text_right, shape::block, true},
        {marker_block_left, indent_left, shape::down_triangle, true},
        {marker_block_first, indent_first, shape::up_triangle, true},
        {marker_block_right, indent_right, shape::down_triangle, true}};
    int tab_id = first_tab_marker;
    for (const fmt::tab_stop &stop :
         fmt::parse_tab_stops(view.block_property("tabstops")))
        markers.push_back({tab_id++, text_left + inches(stop.position),
                           shape::tab, true});
    update_ruler(top_ruler_, top_ruler_range_, 1.0 / dpi,
                 -(view.page_left() - scroll.x) / dpi, text_left, text_right,
                 std::move(markers));
    update_ruler(left_ruler_, left_ruler_range_, 1.0 / dpi, -page_y / dpi,
                 text_top, text_bottom,
                 {{marker_page_top, text_top, shape::block, true},
                  {marker_page_bottom, text_bottom, shape::block, true}});
}

void main_window::set_message(const std::string &text) {
    message_ = text;
    update_state();
}

void main_window::update_title() {
    doc::document &document = canvas_.document();
    const std::string name = document.path().empty()
                                 ? std::string("Untitled")
                                 : document.path().filename().string();
    const bool dirty = document.table().history().is_dirty();
    set_title((dirty ? "*" : "") + name + " - Fyodor");
}

void main_window::update_state() {
    updating_ = true;
    edit::view &view = canvas_.view();

    update_checked(format_bar_, "toggleBold",
                   view.char_property("font-weight") == "bold");
    update_checked(format_bar_, "toggleItalic",
                   view.char_property("font-style") == "italic");
    const std::string decoration = view.char_property("text-decoration");
    update_checked(format_bar_, "toggleUline",
                   decoration.find("underline") != std::string::npos);
    update_checked(format_bar_, "toggleStrike",
                   decoration.find("line-through") != std::string::npos);
    const std::string align = view.block_property("text-align");
    update_checked(format_bar_, "alignLeft", align == "left");
    update_checked(format_bar_, "alignCenter", align == "center");
    update_checked(format_bar_, "alignRight", align == "right");
    update_checked(format_bar_, "alignJustify", align == "justify");
    auto &history = view.document().table().history();
    update_enabled(toolbar_, "undo", history.can_undo());
    update_enabled(toolbar_, "redo", history.can_redo());

    // A document may hold a value the box does not offer, such as a
    // missing font or a 17pt heading: list it, so the box shows what the
    // document asks for rather than going blank.
    const auto select = [](native::combo_box &box, const std::string &text) {
        if (std::ranges::find(box.get_items(), text) == box.get_items().end())
            box.add_item(text);
        const auto &items = box.get_items();
        const auto index = static_cast<int>(
            std::ranges::find(items, text) - items.begin());
        if (box.get_selected_index() != index)
            box.set_selected_index(index);
    };
    select(style_box_, view.block_style());
    select(font_box_, view.char_property("font-family"));
    char size[16];
    std::snprintf(size, sizeof size, "%g",
                  util::to_points(view.char_property("font-size")));
    select(size_box_, size);
    select(zoom_box_, std::to_string(static_cast<int>(
                          std::lround(view.zoom() * 100))) + "%");
    updating_ = false;

    update_rulers();
    const auto &pages = view.layout().pages();

    status_.set_parts(
        {{"Page " + std::to_string(view.current_page()) + " of " +
              std::to_string(pages.size()),
          130},
         {view.block_style(), 150},
         {std::to_string(static_cast<int>(std::lround(view.zoom() * 100))) +
              "%",
          70},
         {message_, 0}});
    update_title();
}

} // namespace ui
