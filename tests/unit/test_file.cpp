//
// Protects document publication against temporary-file collisions and errors.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//
#include "check.h"
#include "impexp/file_format.h"
#include "util/file.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <unistd.h>

namespace fs = std::filesystem;

std::string read(const fs::path &path) {
    std::ifstream stream(path);
    return {std::istreambuf_iterator<char>(stream), {}};
}

int main() {
    std::string pattern = (fs::temp_directory_path() / "fyodor-file-XXXXXX").string();
    const char *directory = mkdtemp(pattern.data());
    CHECK(directory != nullptr);
    if (!directory) return testing::summary("file");
    const fs::path root(directory);
    const auto path = root / "document.abw";
    std::string error;
    doc::document source;
    source.table().insert_text(2, U"saved", doc::empty_attr_prop);
    CHECK(impexp::save_file(path, source, error));
    const auto original = read(path);
    fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write |
                           fs::perms::group_read);
    std::ofstream(root / "document.abw.tmp") << "unrelated";
    CHECK(impexp::save_file(path, source, error));
    CHECK(read(root / "document.abw.tmp") == "unrelated");
    CHECK(fs::status(path).permissions() ==
          (fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read));
    fs::remove(root / "document.abw.tmp");
    fs::create_symlink(path, root / "document.abw.tmp");
    CHECK(impexp::save_file(path, source, error));
    CHECK(read(path) == original);
    doc::document copy;
    CHECK(impexp::load_file(path, copy).ok);
    CHECK(copy.table().text_in(0, copy.table().end_position()) == U"saved");
    fs::create_directory(root / "directory.abw");
    CHECK(!impexp::save_file(root / "directory.abw", source, error));
    CHECK(!error.empty());
    CHECK(!impexp::save_file(root / "missing/document.abw", source, error));
    CHECK(!error.empty());
    fs::permissions(root, fs::perms::owner_read | fs::perms::owner_exec);
    if (geteuid() != 0) {
        CHECK(!impexp::save_file(path, source, error));
        CHECK(!error.empty());
        CHECK(read(path) == original);
    }
    fs::permissions(root, fs::perms::owner_all);
    for (const auto &entry : fs::directory_iterator(root))
        CHECK(!entry.path().filename().string().starts_with(".fyodor-save-"));
    fs::remove_all(root);
    return testing::summary("file");
}
