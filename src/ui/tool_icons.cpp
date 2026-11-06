//
// Draws each icon with lines, rectangles and polygons on a transparent
// 16x16 image. Everything is a small function of one gpx.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "tool_icons.h"

#include <array>
#include <functional>
#include <utility>

namespace ui {
namespace {

using native::gpx;
using native::point;
using native::rect;
using native::rgba;

constexpr rgba ink(40, 44, 52, 255);
constexpr rgba paper(255, 255, 255, 255);
constexpr rgba folder(226, 180, 72, 255);
constexpr rgba folder_edge(150, 110, 30, 255);
constexpr rgba disk(62, 104, 170, 255);
constexpr rgba accent(198, 60, 48, 255);
constexpr rgba arrow(46, 125, 80, 255);

// Draw a line between two points in one color.
void line(gpx &g, rgba color, int x1, int y1, int x2, int y2) {
    g.set_ink(color).set_pen(1);
    g.draw_line(point(static_cast<native::coord>(x1),
                      static_cast<native::coord>(y1)),
                point(static_cast<native::coord>(x2),
                      static_cast<native::coord>(y2)));
}

// Fill a rectangle.
void fill(gpx &g, rgba color, int x, int y, int width, int height) {
    g.set_ink(color).draw_rect(
        rect(static_cast<native::coord>(x), static_cast<native::coord>(y),
             static_cast<native::dim>(width),
             static_cast<native::dim>(height)),
        true);
}

// Outline a rectangle.
void frame(gpx &g, rgba color, int x, int y, int width, int height) {
    g.set_ink(color).set_pen(1);
    g.draw_rect(
        rect(static_cast<native::coord>(x), static_cast<native::coord>(y),
             static_cast<native::dim>(width),
             static_cast<native::dim>(height)),
        false);
}

// Draw a sheet of paper with a folded corner.
void sheet(gpx &g, int x, int y, int width, int height) {
    fill(g, paper, x, y, width, height);
    frame(g, ink, x, y, width, height);
    line(g, ink, x + width - 4, y, x + width - 1, y + 3);
}

void draw_new(gpx &g) {
    sheet(g, 3, 1, 10, 14);
}

void draw_open(gpx &g) {
    fill(g, folder, 1, 4, 13, 10);
    frame(g, folder_edge, 1, 4, 13, 10);
    fill(g, folder, 2, 2, 5, 2);
    line(g, folder_edge, 2, 2, 6, 2);
}

void draw_save(gpx &g) {
    fill(g, disk, 2, 2, 12, 12);
    fill(g, paper, 4, 3, 8, 5);
    fill(g, ink, 5, 10, 6, 4);
    fill(g, paper, 9, 11, 1, 2);
}

// Draw a curved arrow; direction is -1 for undo, 1 for redo.
void draw_turn(gpx &g, int direction) {
    const int middle = 8;
    const auto x = [&](int offset) { return middle + direction * offset; };
    line(g, arrow, x(-4), 11, x(2), 11);
    line(g, arrow, x(2), 11, x(4), 9);
    line(g, arrow, x(4), 9, x(4), 7);
    line(g, arrow, x(4), 7, x(2), 5);
    line(g, arrow, x(2), 5, x(-4), 5);
    line(g, arrow, x(-4), 5, x(-2), 3);
    line(g, arrow, x(-4), 5, x(-2), 7);
}

void draw_undo(gpx &g) {
    draw_turn(g, 1);
}

void draw_redo(gpx &g) {
    draw_turn(g, -1);
}

void draw_cut(gpx &g) {
    line(g, ink, 4, 2, 10, 11);
    line(g, ink, 11, 2, 5, 11);
    frame(g, accent, 2, 11, 4, 4);
    frame(g, accent, 9, 11, 4, 4);
}

void draw_copy(gpx &g) {
    sheet(g, 2, 1, 8, 10);
    sheet(g, 6, 5, 8, 10);
}

void draw_paste(gpx &g) {
    fill(g, folder, 2, 3, 11, 12);
    frame(g, folder_edge, 2, 3, 11, 12);
    fill(g, ink, 5, 1, 5, 3);
    sheet(g, 6, 7, 8, 8);
}

void draw_find(gpx &g) {
    g.set_ink(ink).set_pen(2);
    g.draw_ellipse(rect(2, 2, 9, 9), false);
    g.set_pen(1);
    line(g, ink, 10, 10, 14, 14);
    line(g, ink, 11, 10, 14, 13);
}

void draw_bold(gpx &g) {
    fill(g, ink, 4, 2, 3, 12);
    fill(g, ink, 4, 2, 7, 2);
    fill(g, ink, 4, 7, 7, 2);
    fill(g, ink, 4, 12, 8, 2);
    fill(g, ink, 10, 3, 2, 4);
    fill(g, ink, 11, 8, 2, 5);
}

void draw_italic(gpx &g) {
    line(g, ink, 7, 2, 12, 2);
    line(g, ink, 4, 13, 9, 13);
    line(g, ink, 10, 2, 6, 13);
    line(g, ink, 9, 2, 5, 13);
}

void draw_underline(gpx &g) {
    fill(g, ink, 4, 2, 2, 8);
    fill(g, ink, 10, 2, 2, 8);
    fill(g, ink, 5, 10, 6, 2);
    fill(g, accent, 3, 13, 10, 2);
}

void draw_strike(gpx &g) {
    line(g, ink, 11, 3, 6, 3);
    line(g, ink, 6, 3, 4, 5);
    line(g, ink, 4, 5, 6, 7);
    line(g, ink, 6, 7, 10, 8);
    line(g, ink, 10, 8, 12, 10);
    line(g, ink, 12, 10, 10, 12);
    line(g, ink, 10, 12, 5, 12);
    fill(g, accent, 2, 7, 12, 1);
}

// Draw four text lines given each one's start and length.
void text_lines(gpx &g, const std::array<std::pair<int, int>, 4> &lines) {
    int y = 3;
    for (const auto &[x, length] : lines) {
        line(g, ink, x, y, x + length, y);
        y += 3;
    }
}

void draw_left(gpx &g) {
    text_lines(g, {{{2, 11}, {2, 7}, {2, 11}, {2, 6}}});
}

void draw_center(gpx &g) {
    text_lines(g, {{{2, 11}, {4, 7}, {2, 11}, {5, 5}}});
}

void draw_right(gpx &g) {
    text_lines(g, {{{2, 11}, {6, 7}, {2, 11}, {7, 6}}});
}

void draw_justify(gpx &g) {
    text_lines(g, {{{2, 11}, {2, 11}, {2, 11}, {2, 11}}});
}

using painter = void (*)(gpx &);

// Command names and their drawings.
constexpr std::array<std::pair<std::string_view, painter>, 17> icons{{
    {"fileNew", draw_new},
    {"fileOpen", draw_open},
    {"fileSave", draw_save},
    {"undo", draw_undo},
    {"redo", draw_redo},
    {"cut", draw_cut},
    {"copy", draw_copy},
    {"paste", draw_paste},
    {"find", draw_find},
    {"toggleBold", draw_bold},
    {"toggleItalic", draw_italic},
    {"toggleUline", draw_underline},
    {"toggleStrike", draw_strike},
    {"alignLeft", draw_left},
    {"alignCenter", draw_center},
    {"alignRight", draw_right},
    {"alignJustify", draw_justify},
}};

} // namespace

std::shared_ptr<const native::img> tool_icon(std::string_view command) {
    for (const auto &[name, draw] : icons) {
        if (name != command)
            continue;
        auto image = std::make_shared<native::img>(16, 16);
        gpx &g = image->get_gpx();
        g.set_clip(rect(0, 0, 16, 16));
        g.clear(rgba(0, 0, 0, 0));
        draw(g);
        return image;
    }
    return nullptr;
}

} // namespace ui
