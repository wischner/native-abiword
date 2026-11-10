//
// Prefers Native's standard command icons, then the embedded resource pack.
// Backend selection and state feedback remain Native's responsibility.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "tool_icons.h"

#include <array>
#include <stdexcept>
#include <string>

namespace native_embedded {
// Defined by Native rc in the generated resource translation unit.
std::span<const std::uint8_t> resource_data();
}

namespace ui {

const native::resources &graphics_resources() {
    static const auto pack =
        native::resources::decode(native_embedded::resource_data());
    return pack;
}

std::shared_ptr<const native::img> tool_icon(std::string_view command) {
    using icon_id = native::standard_icon_id;
    static constexpr std::array standard{
        std::pair{"fileNew", icon_id::file_new},
        std::pair{"fileOpen", icon_id::file_open},
        std::pair{"fileSave", icon_id::file_save},
        std::pair{"fileSaveAs", icon_id::file_save_as},
        std::pair{"fileClose", icon_id::file_close},
        std::pair{"cut", icon_id::cut},
        std::pair{"copy", icon_id::copy},
        std::pair{"paste", icon_id::paste},
        std::pair{"undo", icon_id::undo},
        std::pair{"redo", icon_id::redo},
        std::pair{"print", icon_id::print},
        std::pair{"find", icon_id::find}};
    for (const auto &[name, id] : standard)
        if (command == name)
            if (auto icon = native::standard_icon(id))
                return icon;
    const auto &pack = graphics_resources();
    const native::resource_variant *entry = nullptr;
    try {
        entry = &pack.get("icons.toolbar." + std::string(command));
    } catch (const std::out_of_range &) {
        return nullptr;
    }
    if (entry->type != "icon")
        throw std::runtime_error("Toolbar resource is not an icon.");
    // img is immovable; direct initialization uses guaranteed copy elision.
    return std::shared_ptr<const native::img>(new native::img(
        native::img::decode(entry->data.data(), entry->data.size())));
}

} // namespace ui
