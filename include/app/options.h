//
// Command line handling shared by the desktop and headless programs.
//
// Derived from AbiWord src/wp/ap/xp/ap_Args.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef APP_OPTIONS_H
#define APP_OPTIONS_H

#include <filesystem>
#include <optional>
#include <string>

namespace app {

// What the user asked for on the command line.
struct options {
    // Document to open.
    std::optional<std::filesystem::path> file;

    // Convert the document to this file, then exit.
    std::optional<std::filesystem::path> convert_to;

    // Print pagination, then exit.
    bool dump_layout = false;

    bool show_version = false;
    bool show_help = false;

    // Why the arguments were rejected.
    std::string error;
};

// Parse the command line.
options parse_options(int argc, char **argv);

// Return the usage text.
std::string usage_text();

//
// Run a conversion or layout dump without a display.
//
// Returns:
//      The process exit status.
//
int run_batch(const options &settings);

} // namespace app

#endif
