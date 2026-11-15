//
// Publishes complete files without clobbering a predictable temporary path.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//
#ifndef UTIL_FILE_H
#define UTIL_FILE_H

#include <filesystem>
#include <string>
#include <string_view>

namespace util {

// Create missing directories privately; leave existing permissions alone.
bool create_private_directories(const std::filesystem::path &path,
                                std::string &error);

// Stage beside the destination, then rename. Failures preserve the original.
// Private files use 0600; documents retain existing ordinary permissions.
bool write_file_atomic(const std::filesystem::path &path, std::string_view bytes,
                       std::string &error, bool private_file = false);

} // namespace util
#endif
