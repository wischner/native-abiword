//
// Exercises the piece table: fragment tree invariants under randomized
// edits, undo/redo fidelity against a model string, and the cascade.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <random>
#include <string>

#include "check.h"
#include "doc/document.h"
#include "util/utf8.h"

namespace {

using namespace doc;

constexpr position first_text = 2;

// Return the document text after the first block strux.
std::u32string all_text(const document &target) {
    return target.table().text_in(first_text,
                                  target.table().end_position());
}

void test_basic_editing() {
    document target;
    auto &table = target.table();
    CHECK(table.end_position() == first_text);
    table.insert_text(2, U"Hello world", empty_attr_prop);
    CHECK(all_text(target) == U"Hello world");
    table.insert_text(8, U"big ", empty_attr_prop);
    CHECK(all_text(target) == U"Hello big world");
    table.delete_range(8, 12);
    CHECK(all_text(target) == U"Hello world");
    CHECK(table.frags().check_invariants());
}

void test_paragraph_split_and_join() {
    document target;
    auto &table = target.table();
    table.insert_text(2, U"onetwo", empty_attr_prop);
    table.insert_strux(5, strux_kind::block, empty_attr_prop);
    CHECK(all_text(target) == U"one\ntwo");
    CHECK(table.strux_at(4, strux_kind::block) != nullptr);
    CHECK(table.strux_at(4, strux_kind::block) !=
          table.strux_at(7, strux_kind::block));
    // Deleting the paragraph mark joins the paragraphs.
    table.delete_range(5, 6);
    CHECK(all_text(target) == U"onetwo");
    CHECK(table.undo());
    CHECK(all_text(target) == U"one\ntwo");
}

void test_first_structure_survives() {
    document target;
    auto &table = target.table();
    table.insert_text(2, U"text", empty_attr_prop);
    table.delete_range(0, table.end_position());
    CHECK(table.end_position() == first_text);
    CHECK(table.strux_at(1, strux_kind::block) != nullptr);
    CHECK(table.strux_at(0, strux_kind::section) != nullptr);
}

void test_typing_undoes_as_one_action() {
    document target;
    auto &table = target.table();
    position pos = first_text;
    for (const char32_t c : std::u32string(U"typed")) {
        table.insert_text(pos, std::u32string(1, c), empty_attr_prop);
        ++pos;
    }
    CHECK(all_text(target) == U"typed");
    CHECK(table.undo());
    CHECK(all_text(target).empty());
    CHECK(!table.history().can_undo());
    CHECK(table.redo());
    CHECK(all_text(target) == U"typed");
}

void test_format_changes_split_and_undo() {
    document target;
    auto &table = target.table();
    table.insert_text(2, U"abcdef", empty_attr_prop);
    const name_values bold{{"font-weight", "bold"}};
    table.change_span_format(4, 6, format_op::add, {}, bold);
    const frag *middle = table.frags().find(4);
    CHECK(table.formats()
              .get(middle->format())
              .property("font-weight") == "bold");
    CHECK(table.frags().find(2)->format() == empty_attr_prop);
    CHECK(table.frags().find(6)->format() == empty_attr_prop);
    CHECK(table.undo());
    // Undo restores the format and coalesces the fragments again.
    CHECK(table.frags().find(2) == table.frags().find(7));
}

void test_dirty_tracking() {
    document target;
    auto &table = target.table();
    CHECK(!table.history().is_dirty());
    table.insert_text(2, U"a", empty_attr_prop);
    CHECK(table.history().is_dirty());
    table.history().mark_saved();
    CHECK(!table.history().is_dirty());
    table.insert_text(3, U"b", empty_attr_prop);
    CHECK(table.history().is_dirty());
    table.undo();
    CHECK(!table.history().is_dirty());
}

void test_property_cascade() {
    document target;
    auto &table = target.table();
    const attr_prop block({{"style", "Heading 1"}}, {});
    CHECK(target.property("font-size", nullptr, &block, nullptr) ==
          "17pt");
    // Heading 1 inherits its color from Normal through based-on.
    CHECK(target.property("line-height", nullptr, &block, nullptr) ==
          "1.0");
    const attr_prop span({}, {{"font-size", "9pt"}});
    CHECK(target.property("font-size", &span, &block, nullptr) == "9pt");
    // margin-top does not inherit: a span's chain must not reach it.
    CHECK(target.property("margin-top", &span, &block, nullptr) == "0in");
    CHECK(target.property("no-such-property", nullptr, nullptr,
                          nullptr)
              .empty());
    // Interning gives equal sets equal indices.
    CHECK(table.formats().intern(block) == table.formats().intern(block));
}

void test_style_cycle_terminates() {
    document target;
    style loop;
    loop.name = "A";
    loop.based_on = "A";
    target.define_style(loop, {});
    const attr_prop block({{"style", "A"}}, {});
    CHECK(target.property("font-weight", nullptr, &block, nullptr) ==
          "normal");
}

// Mirrors random edits in a plain string, undoing everything at the
// end. Any drift between tree and model shows up as a text mismatch.
void test_random_edits_against_model() {
    std::mt19937 random(20261106);
    document target;
    auto &table = target.table();
    std::u32string model;
    std::vector<std::u32string> states{model};
    for (int step = 0; step < 4000; ++step) {
        const std::size_t size = model.size();
        const auto pick = [&](std::size_t limit) {
            return std::uniform_int_distribution<std::size_t>(
                0, limit)(random);
        };
        switch (pick(3)) {
        case 0: {
            std::u32string text(pick(6) + 1, U'a' + pick(25));
            const std::size_t at = pick(size);
            table.insert_text(first_text + at, text, empty_attr_prop);
            model.insert(at, text);
            break;
        }
        case 1:
            if (size > 0) {
                const std::size_t at = pick(size - 1);
                const std::size_t count =
                    std::min(pick(12) + 1, size - at);
                table.delete_range(first_text + at,
                                   first_text + at + count);
                model.erase(at, count);
            }
            break;
        case 2:
            if (size > 0) {
                const std::size_t at = pick(size - 1);
                const std::size_t count =
                    std::min(pick(12) + 1, size - at);
                table.change_span_format(
                    first_text + at, first_text + at + count,
                    format_op::add, {},
                    {{"color", std::to_string(pick(5))}});
            }
            break;
        default:
            table.undo();
            table.redo();
            break;
        }
        if (step % 97 == 0) {
            CHECK(table.frags().check_invariants());
            CHECK(all_text(target) == model);
        }
    }
    CHECK(all_text(target) == model);
    while (table.undo()) {
    }
    CHECK(all_text(target).empty());
    CHECK(table.frags().check_invariants());
    while (table.redo()) {
    }
    CHECK(all_text(target) == model);
    CHECK(table.frags().check_invariants());
}

void test_utf8() {
    CHECK(util::to_utf32("žé€𝄞") == U"žé€𝄞");
    CHECK(util::to_utf8(U"žé€𝄞") == "žé€𝄞");
    CHECK(util::to_utf32("\xff").front() == 0xfffd);
}

} // namespace

int main() {
    test_basic_editing();
    test_paragraph_split_and_join();
    test_first_structure_survives();
    test_typing_undoes_as_one_action();
    test_format_changes_split_and_undo();
    test_dirty_tracking();
    test_property_cascade();
    test_style_cycle_terminates();
    test_random_edits_against_model();
    test_utf8();
    return testing::summary("piece_table");
}
