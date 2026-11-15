//
// Uses a unique, private sibling directory for atomic file publication.
// The staging directory is cleaned on success and on reported failures.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//
#include "util/file.h"

#include <fstream>
#include <random>
#include <stdexcept>
#ifndef _WIN32
#include <cerrno>
#include <sys/stat.h>
#endif

namespace util {
namespace {
namespace fs = std::filesystem;

// Set Unix access at creation, even when the caller has a permissive umask.
bool make_private_directory(const fs::path &path) {
#ifdef _WIN32
    return fs::create_directory(path);
#else
    if (::mkdir(path.c_str(), 0700) == 0) return true;
    const int failure = errno;
    if (failure == EEXIST) return false;
    throw fs::filesystem_error("cannot create private directory", path,
                               std::error_code(failure, std::generic_category()));
#endif
}

// Remove only the staging files this operation owns.
struct staged_file {
    fs::path directory;
    ~staged_file() {
        if (directory.empty()) return;
        std::error_code ignored;
        fs::remove(directory / "content", ignored);
        fs::remove(directory, ignored);
    }
};

void create_private(const fs::path &path) {
    if (path.empty() || fs::is_directory(path)) return;
    create_private(path.parent_path());
    if (!make_private_directory(path) && !fs::is_directory(path))
        throw std::runtime_error("path exists and is not a directory");
}
} // namespace

bool create_private_directories(const fs::path &path, std::string &error) {
    error.clear();
    try {
        create_private(path);
        return true;
    } catch (const std::exception &failure) {
        error = "Cannot create " + path.string() + ": " + failure.what();
        return false;
    }
}

bool write_file_atomic(const fs::path &path, std::string_view bytes,
                       std::string &error, bool private_file) {
    error.clear();
    staged_file staged;
    try {
        const auto parent = path.has_parent_path() ? path.parent_path() : fs::path(".");
        std::random_device random;
        for (int attempt = 0; attempt != 100; ++attempt) {
            const auto candidate = parent / (".fyodor-save-" +
                std::to_string(random()) + "-" + std::to_string(random()));
            if (make_private_directory(candidate)) {
                staged.directory = candidate;
                break;
            }
        }
        if (staged.directory.empty())
            throw std::runtime_error("cannot reserve a temporary directory");
        const auto temporary = staged.directory / "content";
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream.exceptions(std::ios::badbit | std::ios::failbit);
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        stream.close();
        auto permissions = fs::perms::owner_read | fs::perms::owner_write;
        if (!private_file) {
            std::error_code failure;
            const auto status = fs::status(path, failure);
            if (failure && failure != std::errc::no_such_file_or_directory)
                throw fs::filesystem_error("cannot inspect destination", path, failure);
            if (fs::is_regular_file(status))
                permissions = status.permissions() & fs::perms::mask &
                    ~(fs::perms::set_uid | fs::perms::set_gid | fs::perms::sticky_bit);
        }
        fs::permissions(temporary, permissions);
        fs::rename(temporary, path);
        return true;
    } catch (const std::exception &failure) {
        error = "Cannot save " + path.string() + ": " + failure.what();
        return false;
    }
}
} // namespace util
