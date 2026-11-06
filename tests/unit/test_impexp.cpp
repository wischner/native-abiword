//
// Exercises file formats: real AbiWord files load, and a native
// round trip keeps text, formatting, styles and page setup.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "check.h"
#include "impexp/file_format.h"
#include "impexp/xml_reader.h"
#include "util/utf8.h"

namespace {

using namespace doc;
using namespace impexp;

// Return a whole document's text.
std::u32string text_of(const document &target) {
    return target.table().text_in(0, target.table().end_position());
}

// Records XML events as a flat string.
class recorder final : public xml_handler {
public:
    std::string events;
    void start_element(std::string_view name,
                       const xml_attributes &attributes) override {
        events += '<' + std::string(name);
        for (const auto &[key, value] : attributes)
            events += ' ' + key + '=' + value;
        events += '>';
    }
    void end_element(std::string_view name) override {
        events += "</" + std::string(name) + '>';
    }
    void characters(std::string_view text) override {
        events += text;
    }
};

void test_xml() {
    recorder events;
    std::string error;
    CHECK(parse_xml("<?xml version='1.0'?><!DOCTYPE a [<!ENTITY x 'y'>]>"
                    "<a b=\"1&amp;2\"><!-- no --><c/>t&#233;&lt;"
                    "<![CDATA[<raw>]]></a>",
                    events, error));
    CHECK(events.events == "<a b=1&2><c></c>té<<raw></a>");
    recorder bad;
    CHECK(!parse_xml("<a><b></a>", bad, error));
    CHECK(!error.empty());
    CHECK(!parse_xml("<a>", bad, error));
}

void test_real_abiword_file() {
    document target;
    const import_result result =
        load_file(FIXTURES "/Gettysburg.abw", target);
    CHECK(result.ok);
    const std::string text = util::to_utf8(text_of(target));
    CHECK(text.find("Four score") != std::string::npos ||
          text.find("Four") != std::string::npos);
    CHECK(!target.table().history().is_dirty());
    CHECK(!target.table().history().can_undo());
    CHECK(target.table().frags().check_invariants());
}

void test_round_trip() {
    document source;
    auto &table = source.table();
    table.insert_text(2, U"plain <&> žluťoučký ", empty_attr_prop);
    const attr_prop_index bold = table.formats().intern(
        attr_prop({}, {{"font-weight", "bold"}}));
    table.insert_text(table.end_position(), U"bold", bold);
    table.insert_strux(table.end_position(), strux_kind::block,
                       table.formats().intern(
                           attr_prop({{"style", "Heading 1"}},
                                     {{"text-align", "center"}})));
    table.insert_text(table.end_position(), U"Title", empty_attr_prop);
    table.insert_object(table.end_position(),
                        object_kind::forced_line_break, empty_attr_prop);
    table.insert_text(table.end_position(), U"\tNext", empty_attr_prop);
    source.page().name = "A4";
    source.page().width_inches = 8.27;
    source.metadata()["dc.title"] = "A & B";
    source.set_data("blob", {"image/png", {0, 1, 2, 250, 255}});

    const std::string first = make_abw_exporter()->write(source);
    document copy;
    const import_result result =
        make_abw_importer()->read(first, copy);
    CHECK(result.ok);
    CHECK(result.warnings.empty());
    CHECK(text_of(copy) == text_of(source));
    CHECK(copy.page().name == "A4");
    CHECK(copy.metadata()["dc.title"] == "A & B");
    CHECK(copy.data().at("blob").bytes == source.data().at("blob").bytes);
    const frag_strux *heading = copy.table().strux_at(
        copy.table().end_position(), strux_kind::block);
    CHECK(copy.block_property("font-size", *heading) == "17pt");
    CHECK(copy.block_property("text-align", *heading) == "center");
    // Byte-stable: a second round trip changes nothing.
    CHECK(make_abw_exporter()->write(copy) == first);
}

void test_text_format() {
    document target;
    CHECK(make_text_importer()->read("one\r\ntwo\n\nthree", target).ok);
    CHECK(text_of(target) == U"one\ntwo\n\nthree");
    CHECK(make_text_exporter()->write(target) == "one\ntwo\n\nthree\n");
}

void test_html() {
    document target;
    auto &table = target.table();
    table.insert_text(2, U"a<b", empty_attr_prop);
    const std::string html = make_html_exporter()->write(target);
    CHECK(html.find("<p>a&lt;b</p>") != std::string::npos);
}

void test_base64() {
    const std::vector<std::uint8_t> bytes{'M', 'a', 'n', 0, 255};
    CHECK(base64_encode({'M', 'a', 'n'}) == "TWFu");
    CHECK(base64_decode(base64_encode(bytes)) == bytes);
    CHECK(base64_decode("TW\nFu") ==
          (std::vector<std::uint8_t>{'M', 'a', 'n'}));
}

void test_malformed_input_keeps_structure() {
    document target;
    const import_result result =
        make_abw_importer()->read("<abiword><section><p>x", target);
    CHECK(!result.ok);
    CHECK(target.table().strux_at(target.table().end_position(),
                                  strux_kind::block) != nullptr);
}

} // namespace

int main() {
    test_xml();
    test_real_abiword_file();
    test_round_trip();
    test_text_format();
    test_html();
    test_base64();
    test_malformed_input_keeps_structure();
    return testing::summary("impexp");
}
