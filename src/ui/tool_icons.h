//
// Toolbar artwork: 16x16 icons drawn through Native's graphics, so the
// application ships no image files and no format converters.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef UI_TOOL_ICONS_H
#define UI_TOOL_ICONS_H

#include <memory>
#include <string_view>

#include <native.h>

namespace ui {

// Return the icon of a toolbar command, or null when it has none.
std::shared_ptr<const native::img> tool_icon(std::string_view command);

} // namespace ui

#endif
