//
// Plain text import and export. A line is a paragraph, as in AbiWord.
//
// Derived from AbiWord src/wp/impexp/xp/ie_imp_Text.cpp and
// ie_exp_Text.cpp, Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "impexp/file_format.h"
#include "util/utf8.h"

namespace impexp {
namespace {

using namespace doc;

// Reads plain text.
class text_importer final : public importer {
public:
    import_result read(std::string_view bytes,
                       document &target) override {
        if (bytes.starts_with("\xef\xbb\xbf"))
            bytes.remove_prefix(3);
        target.clear_content();
        piece_table &table = target.table();
        const attr_prop_index normal = table.formats().intern(
            attr_prop({{"style", "Normal"}}, {}));
        table.set_loading(true);
        table.insert_strux(0, strux_kind::section, empty_attr_prop);
        for (;;) {
            const std::size_t end = bytes.find('\n');
            std::string_view line = bytes.substr(0, end);
            if (line.ends_with('\r'))
                line.remove_suffix(1);
            table.insert_strux(table.end_position(), strux_kind::block,
                               normal);
            table.insert_text(table.end_position(), util::to_utf32(line),
                              empty_attr_prop);
            if (end == std::string_view::npos)
                break;
            bytes.remove_prefix(end + 1);
        }
        table.set_loading(false);
        return {true, {}, {}};
    }
};

// Writes plain text.
class text_exporter final : public exporter {
public:
    std::string write(const document &source) override {
        std::string out = util::to_utf8(source.table().text_in(
            0, source.table().end_position()));
        out.push_back('\n');
        return out;
    }
};

} // namespace

std::unique_ptr<importer> make_text_importer() {
    return std::make_unique<text_importer>();
}

std::unique_ptr<exporter> make_text_exporter() {
    return std::make_unique<text_exporter>();
}

} // namespace impexp
