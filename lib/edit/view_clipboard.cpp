//
// Clipboard and search. The system clipboard carries plain text, so a
// rich capture is kept beside it; pasting compares the two to tell an
// internal copy from text arriving from another program.
//
// Derived from AbiWord src/text/fmt/xp/fv_View.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>

#include "edit/view.h"
#include "util/utf8.h"

namespace edit {

using namespace doc;
using fmt::block_layout;

std::string view::selection_text() const {
    const range target = selection();
    return util::to_utf8(
        document_.table().text_in(target.begin, target.end));
}

std::string view::copy() {
    const range target = selection();
    const piece_table &table = document_.table();
    const fragments &frags = table.frags();
    clip_.clear();
    for (const frag *item = frags.find(target.begin);
         item && frags.position_of(item) < target.end;
         item = frags.next(item)) {
        const position start = frags.position_of(item);
        clip_item entry;
        entry.format = item->format();
        switch (item->kind()) {
        case frag_kind::text: {
            const position first = std::max(start, target.begin);
            const position last =
                std::min(start + item->length(), target.end);
            entry.text = std::u32string(
                table.text_of(*static_cast<const frag_text *>(item))
                    .substr(first - start, last - first));
            break;
        }
        case frag_kind::strux:
            if (static_cast<const frag_strux *>(item)->type() !=
                strux_kind::block)
                continue;
            if (start < target.begin)
                continue;
            entry.paragraph = true;
            break;
        case frag_kind::object:
            entry.object = true;
            entry.object_type =
                static_cast<const frag_object *>(item)->type();
            break;
        case frag_kind::end_of_doc:
            continue;
        }
        clip_.push_back(std::move(entry));
    }
    clip_text_ = selection_text();
    return clip_text_;
}

std::string view::cut() {
    const std::string text = copy();
    if (has_selection())
        delete_backward();
    return text;
}

void view::paste_plain(std::u32string_view text) {
    while (!text.empty()) {
        const std::size_t end = text.find(U'\n');
        std::u32string_view line = text.substr(0, end);
        if (!line.empty() && line.back() == U'\r')
            line.remove_suffix(1);
        insert_text(line);
        if (end == std::u32string_view::npos)
            break;
        insert_paragraph();
        text.remove_prefix(end + 1);
    }
}

void view::paste_text(std::string_view utf8) {
    const scoped_glob glob(document_);
    if (clip_.empty() || utf8 != clip_text_) {
        paste_plain(util::to_utf32(utf8));
        return;
    }
    delete_selection();
    piece_table &table = document_.table();
    for (const clip_item &entry : clip_) {
        if (entry.paragraph) {
            table.insert_strux(point_, strux_kind::block, entry.format);
            ++point_;
        } else if (entry.object) {
            table.insert_object(point_, entry.object_type, entry.format);
            ++point_;
        } else {
            table.insert_text(point_, entry.text, entry.format);
            point_ += static_cast<position>(entry.text.size());
        }
    }
    anchor_ = point_;
    changed();
}

namespace {

// Compare two characters under the search options.
bool same(char32_t left, char32_t right, const find_options &options) {
    return left == right ||
           (!options.match_case &&
            util::fold_case(left) == util::fold_case(right));
}

// Return where a needle matches in text at or after an index.
std::size_t search(std::u32string_view text, std::u32string_view needle,
                   std::size_t from, const find_options &options) {
    for (std::size_t index = from; index + needle.size() <= text.size();
         ++index) {
        bool found = true;
        for (std::size_t part = 0; found && part < needle.size(); ++part)
            found = same(text[index + part], needle[part], options);
        if (!found)
            continue;
        if (options.whole_word) {
            const std::size_t end = index + needle.size();
            if ((index > 0 && util::is_word_character(text[index - 1])) ||
                (end < text.size() && util::is_word_character(text[end])))
                continue;
        }
        return index;
    }
    return std::u32string_view::npos;
}

} // namespace

std::optional<range> view::find_from(position start,
                                     std::u32string_view needle,
                                     const find_options &options,
                                     position limit) const {
    const block_layout *block = layout_.block_at(start);
    while (block) {
        const position base = layout_.block_start(*block);
        if (base >= limit)
            return std::nullopt;
        const std::u32string text = block_text(*block);
        const std::size_t from = start > base ? start - base : 0;
        const std::size_t found = search(text, needle, from, options);
        if (found != std::u32string_view::npos) {
            const position begin = base + static_cast<position>(found);
            const position end =
                begin + static_cast<position>(needle.size());
            if (end > limit)
                return std::nullopt;
            return range{begin, end};
        }
        block = layout_.next_block(*block);
    }
    return std::nullopt;
}

bool view::find_next(std::u32string_view needle,
                     const find_options &options) {
    if (needle.empty())
        return false;
    const position start = selection().end;
    auto found = find_from(start, needle, options, document_end() + 1);
    if (!found)
        found = find_from(document_start(), needle, options, start);
    if (!found)
        return false;
    anchor_ = found->begin;
    point_ = found->end;
    sticky_x_.reset();
    return true;
}

bool view::matches_selection(std::u32string_view needle,
                             const find_options &options) const {
    const range target = selection();
    const std::u32string text =
        document_.table().text_in(target.begin, target.end);
    return text.size() == needle.size() &&
           search(text, needle, 0, options) == 0;
}

bool view::replace_next(std::u32string_view needle,
                        std::u32string_view replacement,
                        const find_options &options) {
    if (has_selection() && matches_selection(needle, options))
        insert_text(replacement);
    return find_next(needle, options);
}

int view::replace_all(std::u32string_view needle,
                      std::u32string_view replacement,
                      const find_options &options) {
    if (needle.empty())
        return 0;
    const scoped_glob glob(document_);
    int count = 0;
    position start = document_start();
    while (const auto found =
               find_from(start, needle, options, document_end() + 1)) {
        anchor_ = found->begin;
        point_ = found->end;
        const attr_prop_index format = typing_format();
        document_.table().delete_range(found->begin, found->end);
        if (!replacement.empty())
            document_.table().insert_text(found->begin, replacement, format);
        layout_.update();
        start = found->begin + static_cast<position>(replacement.size());
        point_ = anchor_ = start;
        ++count;
    }
    changed();
    return count;
}

} // namespace edit
