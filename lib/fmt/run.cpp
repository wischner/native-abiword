//
// Measures and draws runs. Text draws word by word: each word starts at
// its exact layout position, so hinted screen advances cannot pile up
// into visible drift across a line, at any zoom.
//
// Derived from AbiWord src/text/fmt/xp/fp_Run.cpp and fp_TextRun.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "fmt/run.h"

#include <algorithm>

#include "util/utf8.h"

namespace fmt {

int to_device(const gfx::painter &painter, lu value) {
    return util::lu_to_device(value, painter.device_per_inch());
}

run::run(std::uint32_t offset, std::uint32_t length, lu width, lu ascent,
         lu descent)
    : offset_(offset), length_(length), width_(width), ascent_(ascent),
      descent_(descent) {
}

lu run::x_of_offset(std::uint32_t offset) const {
    return offset <= offset_ ? 0 : width_;
}

std::uint32_t run::offset_at_x(lu x) const {
    return offset_ + (length_ > 0 && x * 2 >= width_ ? length_ : 0);
}

text_run::text_run(std::uint32_t offset, std::u32string text,
                   text_style style)
    : run(offset, static_cast<std::uint32_t>(text.size()), 0,
          style.face->ascent() + style.rise,
          style.face->descent() - style.rise),
      text_(std::move(text)), style_(std::move(style)) {
    measure();
}

void text_run::measure() {
    widths_.resize(text_.size() + 1);
    widths_[0] = 0;
    for (std::size_t index = 0; index < text_.size(); ++index)
        widths_[index + 1] =
            widths_[index] + style_.face->advance(text_[index]);
    width_ = widths_.back();
}

lu text_run::prefix_width(std::size_t count) const {
    return widths_[std::min(count, text_.size())];
}

std::unique_ptr<text_run> text_run::split(std::size_t count) {
    auto tail = std::make_unique<text_run>(
        offset_ + static_cast<std::uint32_t>(count), text_.substr(count),
        style_);
    text_.resize(count);
    length_ = static_cast<std::uint32_t>(count);
    measure();
    return tail;
}

std::size_t text_run::space_count() const {
    return static_cast<std::size_t>(
        std::ranges::count(text_, U' '));
}

lu text_run::trailing_space_width() const {
    std::size_t end = text_.size();
    while (end > 0 && util::is_space(text_[end - 1]))
        --end;
    return width_ - widths_[end];
}

void text_run::set_space_stretch(lu per_space, lu remainder) {
    measure();
    lu extra = 0;
    for (std::size_t index = 0; index < text_.size(); ++index) {
        if (text_[index] == U' ') {
            extra += per_space;
            if (remainder > 0) {
                ++extra;
                --remainder;
            }
        }
        widths_[index + 1] += extra;
    }
    width_ = widths_.back();
}

void text_run::draw(draw_context &context) const {
    gfx::painter &painter = context.painter;
    const int baseline =
        context.baseline - to_device(painter, style_.rise);
    const int device_width = to_device(painter, width_);
    if (style_.background) {
        const int top = baseline - to_device(painter, style_.face->ascent());
        const int bottom =
            baseline + to_device(painter, style_.face->descent());
        painter.fill_rect({context.x, top, device_width, bottom - top},
                          *style_.background);
    }
    std::size_t begin = 0;
    while (begin < text_.size()) {
        while (begin < text_.size() && text_[begin] == U' ')
            ++begin;
        std::size_t end = begin;
        while (end < text_.size() && text_[end] != U' ')
            ++end;
        if (end > begin)
            painter.draw_text(
                *style_.face,
                std::u32string_view(text_).substr(begin, end - begin),
                context.x + to_device(painter, widths_[begin]), baseline,
                style_.ink);
        begin = end;
    }
    const int thickness = std::max(1, to_device(painter, 20));
    const int right = context.x + device_width;
    const auto rule = [&](int y) {
        painter.draw_line(context.x, y, right, y, style_.ink, thickness);
    };
    const lu ascent = style_.face->ascent();
    if (style_.underline)
        rule(baseline + std::max(1, to_device(painter, ascent / 10)));
    if (style_.strike)
        rule(baseline - to_device(painter, ascent * 3 / 10));
    if (style_.overline)
        rule(baseline - to_device(painter, ascent));
}

lu text_run::x_of_offset(std::uint32_t offset) const {
    return prefix_width(offset > offset_ ? offset - offset_ : 0);
}

std::uint32_t text_run::offset_at_x(lu x) const {
    for (std::size_t index = 0; index < text_.size(); ++index) {
        if (x < (widths_[index] + widths_[index + 1]) / 2)
            return offset_ + static_cast<std::uint32_t>(index);
    }
    return offset_ + length_;
}

tab_run::tab_run(std::uint32_t offset, const text_style &style)
    : run(offset, 1, 0, style.face->ascent(), style.face->descent()) {
}

void tab_run::draw(draw_context &) const {
}

break_run::break_run(std::uint32_t offset, break_kind kind, lu ascent,
                     lu descent)
    : run(offset, 1, 0, ascent, descent), kind_(kind) {
}

void break_run::draw(draw_context &) const {
}

end_of_paragraph_run::end_of_paragraph_run(std::uint32_t offset, lu ascent,
                                           lu descent)
    : run(offset, 0, 0, ascent, descent) {
}

void end_of_paragraph_run::draw(draw_context &) const {
}

image_run::image_run(std::uint32_t offset, std::string data_name, lu width,
                     lu height)
    : run(offset, 1, width, height, 0), data_name_(std::move(data_name)) {
}

void image_run::draw(draw_context &context) const {
    gfx::painter &painter = context.painter;
    const int height = to_device(painter, ascent_);
    painter.draw_image(data_name_,
                       {context.x, context.baseline - height,
                        to_device(painter, width_), height});
}

field_run::field_run(std::uint32_t offset, const std::u32string &value,
                     text_style style)
    : run(offset, 1, 0, style.face->ascent(), style.face->descent()),
      value_(value), style_(std::move(style)) {
    for (const char32_t c : value_)
        width_ += style_.face->advance(c);
}

void field_run::draw(draw_context &context) const {
    context.painter.draw_text(*style_.face, value_, context.x,
                              context.baseline, style_.ink);
}

} // namespace fmt
