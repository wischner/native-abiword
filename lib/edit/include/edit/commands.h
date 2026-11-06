//
// Named editing commands. Menus, toolbars and key bindings all reach
// the view through these names, as AbiWord's edit methods do, so one
// action has one implementation however it is invoked.
//
// Derived from AbiWord src/wp/ap/xp/ap_EditMethods.cpp,
// Copyright (C) 1998 AbiSource, Inc.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef EDIT_COMMANDS_H
#define EDIT_COMMANDS_H

#include <string_view>

#include "edit/view.h"

namespace edit {

//
// Run a command by name.
//
// Parameters:
//      target      - View to act on.
//      name        - Command, e.g. "toggleBold".
//
// Returns:
//      Whether the name is known.
//
bool run_command(view &target, std::string_view name);

} // namespace edit

#endif
