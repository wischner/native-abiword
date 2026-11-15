//
// Checks XDG resolution, validation, private atomic saves and write failures.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//
#include "app/preferences.h"
#include "check.h"
#include "util/file.h"

#include <cstdlib>
#include <fstream>
#include <limits>
#include <unistd.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

int main() {
    std::string pattern = (fs::temp_directory_path() / "fyodor-prefs-XXXXXX").string();
    const char *directory = mkdtemp(pattern.data());
    CHECK(directory != nullptr);
    if (!directory) return testing::summary("preferences");
    const fs::path root(directory);
    std::string error;
    setenv("XDG_CONFIG_HOME", (root / "config").c_str(), 1);
    const auto path = app::preferences_path(error);
    CHECK(error.empty());
    CHECK(path == root / "config/fyodor/preferences.conf");
    CHECK(app::load_preferences(path, error).zoom == 1.0);
    CHECK(error.empty());
    CHECK(!fs::exists(path.parent_path()));
    app::preferences values{1.5625, true, true};
    const auto original_mask = umask(0);
    CHECK(app::save_preferences(path, values, error));
    umask(original_mask);
    CHECK(error.empty());
    const auto copy = app::load_preferences(path, error);
    CHECK(error.empty());
    CHECK(copy.zoom == values.zoom && copy.match_case && copy.whole_word);
    CHECK(fs::status(path).permissions() ==
          (fs::perms::owner_read | fs::perms::owner_write));
    CHECK(fs::status(path.parent_path()).permissions() == fs::perms::owner_all);
    CHECK(fs::status(path.parent_path().parent_path()).permissions() == fs::perms::owner_all);
    CHECK(app::save_preferences(path, {0.75, false, false}, error));
    CHECK(app::load_preferences(path, error).zoom == 0.75);
    CHECK(!app::load_preferences(path, error).match_case);
    CHECK(!app::save_preferences(path, {std::numeric_limits<double>::infinity()}, error));
    CHECK(!error.empty());
    CHECK(app::load_preferences(path, error).zoom == 0.75);

    // A relative XDG path is invalid and ignored, per the specification.
    const char *home = std::getenv("HOME");
    setenv("XDG_CONFIG_HOME", "relative", 1);
    if (home && fs::path(home).is_absolute())
        CHECK(app::preferences_path(error) == fs::path(home) / ".config/fyodor/preferences.conf");
    setenv("XDG_CONFIG_HOME", "", 1);
    if (home && fs::path(home).is_absolute())
        CHECK(app::preferences_path(error) == fs::path(home) / ".config/fyodor/preferences.conf");

    CHECK(util::write_file_atomic(path, "version=1\nzoom=nan\nmatch_case=maybe\nwhole_word=1\n", error));
    const auto invalid = app::load_preferences(path, error);
    CHECK(!error.empty());
    CHECK(invalid.zoom == 1.0 && !invalid.match_case && invalid.whole_word);
    CHECK(util::write_file_atomic(path, "zoom=99\n", error));
    CHECK(app::load_preferences(path, error).zoom == 1.0);
    CHECK(!error.empty());
    CHECK(util::write_file_atomic(path, "zoom=1.5junk\n", error));
    CHECK(app::load_preferences(path, error).zoom == 1.0);
    CHECK(!error.empty());

    // An inaccessible directory must not cause a fallback or damage a file.
    CHECK(app::save_preferences(path, values, error));
    fs::permissions(path.parent_path(), fs::perms::owner_read | fs::perms::owner_exec);
    if (geteuid() != 0) {
        CHECK(!app::save_preferences(path, {2.0}, error));
        CHECK(!error.empty());
        CHECK(app::load_preferences(path, error).zoom == values.zoom);
    }
    fs::permissions(path.parent_path(), fs::perms::owner_all);
    fs::permissions(path, fs::perms::none);
    if (geteuid() != 0) {
        app::load_preferences(path, error);
        CHECK(!error.empty());
    }
    fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write);
    CHECK(!app::save_preferences(path / "child/preferences.conf", values, error));
    CHECK(!error.empty());
    CHECK(!app::save_preferences({}, values, error));
    CHECK(!error.empty());
    fs::remove_all(root);
    return testing::summary("preferences");
}
