//
// Keeps blocks in step with the piece table and answers position
// queries. Pagination itself lives in column_breaker.cpp.
//
// Sections carrying a "type" attribute are headers and footers. They
// are kept in the document but are not laid out yet.
//
// Derived from AbiWord src/text/fmt/xp/fl_DocLayout.cpp,
// fl_DocListener.cpp and fl_SectionLayout.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "fmt/doc_layout.h"

#include <algorithm>

namespace fmt {

using namespace doc;

lu section_geometry::column_width() const {
    const lu text = page_width - margin_left - margin_right;
    return std::max<lu>((text - column_gap * (columns - 1)) / columns, 1);
}

doc_layout::doc_layout(document &source, gfx::font_provider &fonts)
    : document_(source), fonts_(fonts) {
    document_.table().add_listener(this);
    update();
}

doc_layout::~doc_layout() {
    document_.table().remove_listener(this);
}

void doc_layout::build_blocks() {
    blocks_.clear();
    const fragments &frags = document_.table().frags();
    bool visible = true;
    for (const frag *item = frags.first(); item; item = frags.next(item)) {
        if (item->kind() != frag_kind::strux)
            continue;
        const auto *strux = static_cast<const frag_strux *>(item);
        if (strux->type() == strux_kind::section) {
            visible = !document_.table()
                           .formats()
                           .get(strux->format())
                           .attribute("type");
        } else if (visible) {
            blocks_.push_back(std::make_unique<block_layout>(strux));
        }
    }
    structure_dirty_ = false;
}

const frag_strux *
doc_layout::section_of(const block_layout &block) const {
    return document_.table().strux_at(
        document_.table().frags().position_of(block.strux()),
        strux_kind::section);
}

section_geometry
doc_layout::geometry_for(const frag_strux *section) const {
    const piece_table &table = document_.table();
    const attr_prop *format =
        section ? &table.formats().get(section->format()) : nullptr;
    const auto value = [&](std::string_view name) {
        return document_.property(name, nullptr, nullptr, format);
    };
    const page_setup &setup = document_.page();
    section_geometry result;
    const double width =
        setup.portrait ? setup.width_inches : setup.height_inches;
    const double height =
        setup.portrait ? setup.height_inches : setup.width_inches;
    result.page_width =
        static_cast<lu>(width * util::layout_resolution);
    result.page_height =
        static_cast<lu>(height * util::layout_resolution);
    result.margin_left = util::to_lu(value("page-margin-left"));
    result.margin_right = util::to_lu(value("page-margin-right"));
    result.margin_top = util::to_lu(value("page-margin-top"));
    result.margin_bottom = util::to_lu(value("page-margin-bottom"));
    result.columns = std::max(
        1, static_cast<int>(util::leading_number(value("columns"))));
    result.column_gap = util::to_lu(value("column-gap"));
    return result;
}

format_context
doc_layout::context_for(const frag_strux *section) const {
    format_context context{document_, fonts_, section, 0};
    context.column_width = geometry_for(section).column_width();
    return context;
}

void doc_layout::format_dirty_blocks() {
    for (const auto &block : blocks_) {
        if (block->dirty())
            block->format(context_for(section_of(*block)));
    }
}

void doc_layout::update() {
    if (structure_dirty_)
        build_blocks();
    format_dirty_blocks();
    flow();
}

void doc_layout::rebuild() {
    structure_dirty_ = true;
    update();
}

void doc_layout::on_change(const change_record &record) {
    switch (record.type) {
    case change_type::insert_strux:
        if (record.strux == strux_kind::section) {
            structure_dirty_ = true;
            return;
        }
        {
            // The new block splits off the end of the previous one. It
            // is not in blocks_ yet, so the block before it is the last
            // one starting before its position, empty or not.
            const auto *added = static_cast<const frag_strux *>(
                document_.table().frags().find(record.pos));
            const std::size_t index = block_index_at(record.pos);
            if (index < blocks_.size())
                blocks_[index]->mark_dirty();
            blocks_.insert(
                blocks_.begin() +
                    static_cast<std::ptrdiff_t>(
                        std::min(index + 1, blocks_.size())),
                std::make_unique<block_layout>(added));
        }
        return;
    case change_type::delete_strux:
        if (record.strux == strux_kind::section)
            structure_dirty_ = true;
        else if (!blocks_.empty())
            blocks_[block_index_at(record.pos)]->mark_dirty();
        return;
    case change_type::change_strux:
        if (record.strux == strux_kind::section) {
            structure_dirty_ = true;
            return;
        }
        [[fallthrough]];
    default:
        if (!blocks_.empty())
            blocks_[block_index_at(record.pos)]->mark_dirty();
        return;
    }
}

void doc_layout::on_strux_removed(const frag_strux &strux) {
    if (strux.type() != strux_kind::block)
        return;
    const std::size_t index = block_index_of(&strux);
    if (index < blocks_.size())
        blocks_.erase(blocks_.begin() +
                      static_cast<std::ptrdiff_t>(index));
}

std::size_t doc_layout::block_index_of(const frag_strux *strux) const {
    for (std::size_t index = 0; index < blocks_.size(); ++index)
        if (blocks_[index]->strux() == strux)
            return index;
    return blocks_.size();
}

std::size_t doc_layout::block_index_at(position pos) const {
    const fragments &frags = document_.table().frags();
    std::size_t low = 0;
    std::size_t high = blocks_.size();
    // Blocks are in document order: find the last one starting at or
    // before the position.
    while (high - low > 1) {
        const std::size_t middle = (low + high) / 2;
        if (frags.position_of(blocks_[middle]->strux()) < pos)
            low = middle;
        else
            high = middle;
    }
    return low;
}

position doc_layout::block_start(const block_layout &block) const {
    return document_.table().frags().position_of(block.strux()) + 1;
}

position doc_layout::block_end(const block_layout &block) const {
    return block_start(block) + block.lines().back()->end_offset();
}

const block_layout *
doc_layout::next_block(const block_layout &block) const {
    const std::size_t index = block_index_of(block.strux());
    return index + 1 < blocks_.size() ? blocks_[index + 1].get() : nullptr;
}

const block_layout *
doc_layout::previous_block(const block_layout &block) const {
    const std::size_t index = block_index_of(block.strux());
    return index > 0 && index < blocks_.size() ? blocks_[index - 1].get()
                                               : nullptr;
}

const block_layout *doc_layout::block_at(position pos) const {
    if (blocks_.empty())
        return nullptr;
    return blocks_[block_index_at(pos)].get();
}

doc_layout::line_address doc_layout::address_of(position pos) const {
    const block_layout *block = block_at(pos);
    const std::size_t index = block_index_at(pos);
    const std::uint32_t offset = pos - block_start(*block);
    // Pages are ordered, so bisect to the first page holding the block.
    std::size_t low = 0;
    std::size_t high = pages_.size();
    while (low < high) {
        const std::size_t middle = (low + high) / 2;
        const auto &lines = pages_[middle].lines;
        if (lines.empty() || lines.back().block_index < index)
            low = middle + 1;
        else
            high = middle;
    }
    line_address fallback{low, 0};
    for (std::size_t p = low; p < pages_.size(); ++p) {
        const auto &lines = pages_[p].lines;
        for (std::size_t l = 0; l < lines.size(); ++l) {
            const placed_line &placed = lines[l];
            if (placed.block_index < index)
                continue;
            if (placed.block_index > index)
                return fallback;
            fallback = {p, l};
            if (offset < placed.item->end_offset())
                return fallback;
            // The end of a line that ended by wrapping belongs to the
            // next line; the block's end has nowhere else to go.
        }
    }
    return fallback;
}

caret_location doc_layout::locate(position pos) const {
    caret_location result;
    if (pages_.empty())
        return result;
    const line_address address = address_of(pos);
    if (address.page >= pages_.size() ||
        pages_[address.page].lines.empty())
        return result;
    const placed_line &placed = pages_[address.page].lines[address.line];
    const std::uint32_t offset = pos - block_start(*placed.block);
    result.page = address.page;
    result.x = placed.column_x + placed.item->x_of_offset(offset);
    result.y = placed.y;
    result.height = placed.item->height();
    return result;
}

position doc_layout::position_at(std::size_t page_index, lu x, lu y) const {
    const page &target = pages_.at(page_index);
    if (target.lines.empty())
        return 2;
    const placed_line *best = nullptr;
    lu best_distance = 0;
    for (const placed_line &placed : target.lines) {
        const lu top = placed.y;
        const lu bottom = top + placed.item->height();
        lu distance = y < top ? top - y : y >= bottom ? y - bottom + 1 : 0;
        // Between columns, the nearer column wins.
        if (distance == 0 && x < placed.column_x)
            distance = placed.column_x - x;
        if (!best || distance < best_distance) {
            best = &placed;
            best_distance = distance;
        }
    }
    return block_start(*best->block) +
           best->item->offset_at_x(x - best->column_x);
}

std::pair<position, position> doc_layout::line_bounds(position pos) const {
    const line_address address = address_of(pos);
    const placed_line &placed = pages_.at(address.page).lines.at(address.line);
    const position start = block_start(*placed.block);
    return {start + placed.item->first_offset(),
            start + placed.item->end_offset()};
}

position doc_layout::move_lines(position pos, lu x, int lines) const {
    line_address address = address_of(pos);
    for (; lines != 0; lines += lines > 0 ? -1 : 1) {
        if (lines > 0) {
            if (++address.line >= pages_[address.page].lines.size()) {
                std::size_t next = address.page + 1;
                while (next < pages_.size() && pages_[next].lines.empty())
                    ++next;
                if (next >= pages_.size()) {
                    address.line = pages_[address.page].lines.size() - 1;
                    break;
                }
                address = {next, 0};
            }
        } else if (address.line == 0) {
            if (address.page == 0)
                break;
            --address.page;
            address.line = pages_[address.page].lines.size() - 1;
        } else {
            --address.line;
        }
    }
    const placed_line &placed = pages_[address.page].lines[address.line];
    return block_start(*placed.block) +
           placed.item->offset_at_x(x - placed.column_x);
}

} // namespace fmt
