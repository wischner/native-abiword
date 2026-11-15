//
// Reads versioned key/value preferences and resolves desktop configuration.
// Search text and document contents are never stored in this file.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//
#include "app/preferences.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

#include "util/file.h"

namespace app {
namespace {
namespace fs = std::filesystem;

fs::path absolute_environment(const char *name) {
    const char *value = std::getenv(name);
    const fs::path path(value ? value : "");
    return path.is_absolute() ? path : fs::path{};
}

bool valid_zoom(double zoom) {
    return std::isfinite(zoom) && zoom >= 0.25 && zoom <= 5.0;
}
} // namespace

fs::path preferences_path(std::string &error) {
    error.clear();
#ifdef _WIN32
    auto base = absolute_environment("APPDATA");
#else
    auto base = absolute_environment("XDG_CONFIG_HOME");
    if (base.empty()) {
        base = absolute_environment("HOME");
        if (!base.empty()) {
#ifdef __APPLE__
            base /= "Library/Application Support";
#else
            base /= ".config";
#endif
        }
    }
#endif
    if (base.empty()) {
        error = "Cannot locate a user configuration directory";
        return {};
    }
    return base / "fyodor" / "preferences.conf";
}

preferences load_preferences(const fs::path &path, std::string &error) {
    error.clear();
    preferences values;
    if (path.empty()) {
        error = "No user configuration path";
        return values;
    }
    try {
        if (!fs::exists(path)) return values;
        std::ifstream stream(path);
        if (!stream) throw std::runtime_error("cannot open preferences");
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty() || line[0] == '#') continue;
            const auto equal = line.find('=');
            const auto key = line.substr(0, equal);
            const auto value = equal == std::string::npos ? "" : line.substr(equal + 1);
            bool valid = true;
            if (key == "zoom") {
                std::istringstream number(value);
                number.imbue(std::locale::classic());
                double zoom = 0;
                valid = bool(number >> zoom) && number.eof() && valid_zoom(zoom);
                if (valid) values.zoom = zoom;
            } else if (key == "match_case" || key == "whole_word") {
                valid = value == "0" || value == "1";
                if (valid)
                    (key == "match_case" ? values.match_case : values.whole_word) = value == "1";
            } else if (key == "version") {
                valid = value == "1";
            }
            if (!valid)
                error = "Invalid preference '" + key + "' in " + path.string();
        }
        if (stream.bad()) throw std::runtime_error("cannot read preferences");
    } catch (const std::exception &failure) {
        error = "Cannot load " + path.string() + ": " + failure.what();
    }
    return values;
}

bool save_preferences(const fs::path &path, const preferences &values,
                      std::string &error) {
    error.clear();
    if (path.empty() || !valid_zoom(values.zoom)) {
        error = "Cannot save preferences: invalid path or zoom";
        return false;
    }
    if (!util::create_private_directories(path.parent_path(), error))
        return false;
    std::ostringstream content;
    content.imbue(std::locale::classic());
    content << "version=1\nzoom=" << std::setprecision(17) << values.zoom
            << "\nmatch_case=" << values.match_case
            << "\nwhole_word=" << values.whole_word << '\n';
    return util::write_file_atomic(path, content.str(), error, true);
}
} // namespace app
