//
// Stores immediately applied desktop preferences, separate from documents.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//
#ifndef APP_PREFERENCES_H
#define APP_PREFERENCES_H

#include <filesystem>
#include <string>

namespace app {

struct preferences {
    double zoom = 1.0;
    bool match_case = false;
    bool whole_word = false;
};

// Resolve the user configuration file. Never falls back to the working folder.
std::filesystem::path preferences_path(std::string &error);

// Missing files use defaults; malformed values retain defaults and report why.
preferences load_preferences(const std::filesystem::path &path,
                             std::string &error);

// Validate and atomically save private configuration, creating its directory.
bool save_preferences(const std::filesystem::path &path,
                      const preferences &values, std::string &error);

} // namespace app
#endif
