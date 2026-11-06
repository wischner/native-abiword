//
// File format plug points: an importer fills a document, an exporter
// writes one, and a registry picks by file extension. This replaces
// AbiWord's IE_Imp/IE_Exp sniffer classes and their plugin registration.
//
// Derived from AbiWord src/wp/impexp/xp/ie_imp.cpp and ie_exp.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef IMPEXP_FILE_FORMAT_H
#define IMPEXP_FILE_FORMAT_H

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "doc/document.h"

namespace impexp {

// The outcome of an import.
struct import_result {
    bool ok = false;

    // Why the import failed.
    std::string error;

    // What was read but cannot be kept faithfully.
    std::vector<std::string> warnings;
};

// Reads one format into a document.
class importer {
public:
    virtual ~importer() = default;

    // Replace a document's content from bytes.
    virtual import_result read(std::string_view bytes,
                               doc::document &target) = 0;
};

// Writes a document in one format.
class exporter {
public:
    virtual ~exporter() = default;

    // Serialize a document.
    virtual std::string write(const doc::document &source) = 0;
};

// One file format known to the application.
struct file_format {
    // Display name, e.g. "AbiWord Document".
    std::string name;

    // Extension without the dot.
    std::string extension;

    // Null when the format cannot be read.
    std::unique_ptr<importer> (*make_importer)();

    // Null when the format cannot be written.
    std::unique_ptr<exporter> (*make_exporter)();
};

// Return every registered format.
const std::vector<file_format> &file_formats();

// Return the format for a path's extension, or null.
const file_format *format_for(const std::filesystem::path &path);

//
// Load a file into a document, choosing the format by extension.
//
// Notes:
//      An unknown extension loads as plain text, as AbiWord does.
//
import_result load_file(const std::filesystem::path &path,
                        doc::document &target);

//
// Save a document to a file, choosing the format by extension.
//
// Parameters:
//      path        - Destination; written through a temporary file so
//                    a failure never destroys the previous contents.
//      source      - Document to write.
//      error       - Receives a description of the failure.
//
bool save_file(const std::filesystem::path &path,
               const doc::document &source, std::string &error);

// Make the AbiWord native format importer.
std::unique_ptr<importer> make_abw_importer();

// Make the AbiWord native format exporter.
std::unique_ptr<exporter> make_abw_exporter();

// Make the plain text importer.
std::unique_ptr<importer> make_text_importer();

// Make the plain text exporter.
std::unique_ptr<exporter> make_text_exporter();

// Make the HTML exporter.
std::unique_ptr<exporter> make_html_exporter();

// Encode bytes as base64.
std::string base64_encode(const std::vector<std::uint8_t> &bytes);

// Decode base64, ignoring whitespace.
std::vector<std::uint8_t> base64_decode(std::string_view text);

} // namespace impexp

#endif
