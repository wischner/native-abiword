//
// The format registry, file access and base64. Saving goes through a
// sibling temporary file and a rename: a failed write leaves the
// user's existing document untouched.
//
// Derived from AbiWord src/wp/impexp/xp/ie_imp.cpp and ie_exp.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "impexp/file_format.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iterator>

namespace impexp {
namespace {

constexpr std::string_view base64_alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Return a lowercase extension without its dot.
std::string extension_of(const std::filesystem::path &path) {
    std::string result = path.extension().string();
    if (!result.empty())
        result.erase(0, 1);
    std::ranges::transform(result, result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

} // namespace

const std::vector<file_format> &file_formats() {
    static const std::vector<file_format> formats = {
        {"AbiWord Document", "abw", make_abw_importer, make_abw_exporter},
        {"AbiWord Template", "awt", make_abw_importer, make_abw_exporter},
        {"Text", "txt", make_text_importer, make_text_exporter},
        {"HTML", "html", nullptr, make_html_exporter},
    };
    return formats;
}

const file_format *format_for(const std::filesystem::path &path) {
    const std::string extension = extension_of(path);
    for (const file_format &format : file_formats())
        if (format.extension == extension)
            return &format;
    return nullptr;
}

import_result load_file(const std::filesystem::path &path,
                        doc::document &target) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        return {false, "cannot open " + path.string(), {}};
    const std::string bytes{std::istreambuf_iterator<char>(stream),
                            std::istreambuf_iterator<char>()};
    if (stream.bad())
        return {false, "cannot read " + path.string(), {}};
    const file_format *format = format_for(path);
    const auto reader = format && format->make_importer
                            ? format->make_importer()
                            : make_text_importer();
    import_result result = reader->read(bytes, target);
    if (result.ok) {
        target.table().history().clear();
        target.set_path(path);
    }
    return result;
}

bool save_file(const std::filesystem::path &path,
               const doc::document &source, std::string &error) {
    const file_format *format = format_for(path);
    if (!format || !format->make_exporter) {
        error = "no way to write " + path.extension().string() + " files";
        return false;
    }
    const std::string bytes = format->make_exporter()->write(source);
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream stream(temporary,
                             std::ios::binary | std::ios::trunc);
        stream.write(bytes.data(),
                     static_cast<std::streamsize>(bytes.size()));
        stream.close();
        if (!stream) {
            error = "cannot write " + temporary.string();
            return false;
        }
    }
    std::error_code failure;
    std::filesystem::rename(temporary, path, failure);
    if (failure) {
        std::filesystem::remove(temporary, failure);
        error = "cannot replace " + path.string();
        return false;
    }
    return true;
}

std::string base64_encode(const std::vector<std::uint8_t> &bytes) {
    std::string result;
    result.reserve((bytes.size() + 2) / 3 * 4);
    for (std::size_t index = 0; index < bytes.size(); index += 3) {
        const std::size_t left = bytes.size() - index;
        std::uint32_t chunk = static_cast<std::uint32_t>(bytes[index]) << 16;
        if (left > 1) chunk |= static_cast<std::uint32_t>(bytes[index + 1]) << 8;
        if (left > 2) chunk |= bytes[index + 2];
        result.push_back(base64_alphabet[(chunk >> 18) & 63]);
        result.push_back(base64_alphabet[(chunk >> 12) & 63]);
        result.push_back(left > 1 ? base64_alphabet[(chunk >> 6) & 63] : '=');
        result.push_back(left > 2 ? base64_alphabet[chunk & 63] : '=');
    }
    return result;
}

std::vector<std::uint8_t> base64_decode(std::string_view text) {
    std::vector<std::uint8_t> result;
    result.reserve(text.size() / 4 * 3);
    std::uint32_t chunk = 0;
    int bits = 0;
    for (const char c : text) {
        const std::size_t value = base64_alphabet.find(c);
        if (value == std::string_view::npos)
            continue;
        chunk = (chunk << 6) | static_cast<std::uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            result.push_back(
                static_cast<std::uint8_t>((chunk >> bits) & 0xff));
        }
    }
    return result;
}

} // namespace impexp
