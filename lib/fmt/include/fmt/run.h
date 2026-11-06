//
// Runs: the atoms of a formatted line. Each subclass knows how to
// measure and draw itself and how to map between character offsets and
// horizontal positions, which replaces AbiWord's run type switches.
//
// Derived from AbiWord src/text/fmt/xp/fp_Run.cpp and fp_TextRun.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef FMT_RUN_H
#define FMT_RUN_H

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gfx/graphics.h"

namespace fmt {

using util::lu;

// Where a line stops short of the block's end.
enum class break_kind { none, line, column, page };

// Shared values while drawing one line.
struct draw_context {
    gfx::painter &painter;

    // Device position of the run's left edge.
    int x = 0;

    // Device position of the line's baseline.
    int baseline = 0;
};

// Convert layout units to device pixels at a painter's resolution.
int to_device(const gfx::painter &painter, lu value);

// One atom of a line.
class run {
public:
    virtual ~run() = default;

    // Return the first character offset within the block.
    std::uint32_t offset() const { return offset_; }

    // Return how many document positions this run covers.
    std::uint32_t length() const { return length_; }

    // Return the width in layout units.
    lu width() const { return width_; }

    // Set the width; tabs learn theirs from their line position.
    void set_width(lu width) { width_ = width; }

    // Return the left edge within the line.
    lu x() const { return x_; }

    // Set the left edge within the line.
    void set_x(lu x) { x_ = x; }

    // Return the height above the baseline.
    lu ascent() const { return ascent_; }

    // Return the height below the baseline.
    lu descent() const { return descent_; }

    // Report what line break this run forces.
    virtual break_kind forced_break() const { return break_kind::none; }

    // Draw the run at the context's position.
    virtual void draw(draw_context &context) const = 0;

    // Return the x within the run of a character offset.
    virtual lu x_of_offset(std::uint32_t offset) const;

    // Return the character offset nearest an x within the run.
    virtual std::uint32_t offset_at_x(lu x) const;

protected:
    run(std::uint32_t offset, std::uint32_t length, lu width, lu ascent,
        lu descent);

    std::uint32_t offset_;
    std::uint32_t length_;
    lu width_;
    lu x_ = 0;
    lu ascent_;
    lu descent_;
};

// Inline text appearance, resolved from the property cascade.
struct text_style {
    std::shared_ptr<const gfx::font> face;
    util::color ink;
    std::optional<util::color> background;
    bool underline = false;
    bool overline = false;
    bool strike = false;

    // Baseline shift upward in layout units; negative is subscript.
    lu rise = 0;
};

// A span of characters in one style.
class text_run final : public run {
public:
    // Create a run measuring its characters.
    text_run(std::uint32_t offset, std::u32string text, text_style style);

    // Return the characters.
    const std::u32string &text() const { return text_; }

    // Return the style.
    const text_style &style() const { return style_; }

    // Return the width of a prefix of this run.
    lu prefix_width(std::size_t count) const;

    //
    // Split this run, keeping the head.
    //
    // Parameters:
    //      count       - Characters to keep.
    //
    // Returns:
    //      A new run holding the tail.
    //
    std::unique_ptr<text_run> split(std::size_t count);

    // Add width to every space of this run, for justification.
    void set_space_stretch(lu per_space, lu remainder);

    // Count the spaces of this run.
    std::size_t space_count() const;

    // Return the width of trailing spaces.
    lu trailing_space_width() const;

    void draw(draw_context &context) const override;
    lu x_of_offset(std::uint32_t offset) const override;
    std::uint32_t offset_at_x(lu x) const override;

private:
    void measure();

    std::u32string text_;
    text_style style_;

    // Width of each prefix; widths_[n] covers n characters.
    std::vector<lu> widths_;
};

// A tab character; its line position decides its width.
class tab_run final : public run {
public:
    // Create a tab of zero width.
    tab_run(std::uint32_t offset, const text_style &style);

    void draw(draw_context &context) const override;
};

// A one-position forced break, drawn as nothing.
class break_run final : public run {
public:
    // Create a break of one kind.
    break_run(std::uint32_t offset, break_kind kind, lu ascent,
              lu descent);

    break_kind forced_break() const override { return kind_; }
    void draw(draw_context &context) const override;

private:
    break_kind kind_;
};

// The zero-length end of a paragraph; gives empty lines their height.
class end_of_paragraph_run final : public run {
public:
    // Create the end mark.
    end_of_paragraph_run(std::uint32_t offset, lu ascent, lu descent);

    void draw(draw_context &context) const override;
};

// An inline image.
class image_run final : public run {
public:
    // Create a run sized by the image's properties.
    image_run(std::uint32_t offset, std::string data_name, lu width,
              lu height);

    void draw(draw_context &context) const override;

private:
    std::string data_name_;
};

// A computed text field such as a date or page number.
class field_run final : public run {
public:
    // Create a field showing a value.
    field_run(std::uint32_t offset, const std::u32string &value,
              text_style style);

    void draw(draw_context &context) const override;

private:
    std::u32string value_;
    text_style style_;
};

} // namespace fmt

#endif
