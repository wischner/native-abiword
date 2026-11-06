//
// Parses arguments and runs batch work. Batch mode links no UI code, so
// conversions work on a machine without a display, like AbiWord's --to.
//
// Derived from AbiWord src/wp/ap/xp/ap_Args.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "app/options.h"

#include <iostream>
#include <string_view>

#include "fmt/doc_layout.h"
#include "impexp/file_format.h"

namespace app {

options parse_options(int argc, char **argv) {
    options result;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            result.show_help = true;
        } else if (argument == "--version") {
            result.show_version = true;
        } else if (argument == "--dump-layout") {
            result.dump_layout = true;
        } else if (argument == "--to" || argument == "-t") {
            if (++index >= argc) {
                result.error = "--to needs a file name";
                return result;
            }
            result.convert_to = argv[index];
        } else if (argument.starts_with("-")) {
            result.error = "unknown option " + std::string(argument);
            return result;
        } else if (!result.file) {
            result.file = argument;
        } else {
            result.error = "only one document can be named";
            return result;
        }
    }
    return result;
}

std::string usage_text() {
    return "Usage: fyodor [options] [document]\n"
           "  --to FILE       convert the document to FILE and exit\n"
           "  --dump-layout   print pages and lines and exit\n"
           "  --version       print the version and exit\n"
           "  --help          print this text and exit\n";
}

int run_batch(const options &settings) {
    if (!settings.file) {
        std::cerr << "fyodor: batch work needs a document\n";
        return 2;
    }
    doc::document source;
    const impexp::import_result loaded =
        impexp::load_file(*settings.file, source);
    if (!loaded.ok) {
        std::cerr << "fyodor: " << loaded.error << '\n';
        return 1;
    }
    for (const std::string &warning : loaded.warnings)
        std::cerr << "fyodor: warning: " << warning << '\n';
    if (settings.convert_to) {
        std::string error;
        if (!impexp::save_file(*settings.convert_to, source, error)) {
            std::cerr << "fyodor: " << error << '\n';
            return 1;
        }
    }
    if (settings.dump_layout) {
        gfx::fixed_font_provider fonts;
        const fmt::doc_layout layout(source, fonts);
        std::cout << layout.pages().size() << " pages\n";
        std::size_t number = 0;
        for (const fmt::page &sheet : layout.pages())
            std::cout << "page " << ++number << ": " << sheet.lines.size()
                      << " lines\n";
    }
    return 0;
}

} // namespace app
