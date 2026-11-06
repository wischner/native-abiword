# Native changes made for fyodor

These extend the sibling `../native`. A contract added to Native has to work on
every backend, so nothing here is SDL2-only. Native's `FEATURE-MATRIX.md` carries
per-backend status.

| Change | Why | Backends |
| --- | --- | --- |
| `font_size_basis` on `font_t::from_file` and `from_memory` | Native sized by line height; a document's 12pt means one em | All, shared code |
| `canvas::on_text_input` | `key_event` is physical positions, so a canvas could not receive composed text | All |
| `canvas::focus()` | A document window must take the keyboard without a click | All |
| `mouse_event::clicks` and `modifiers` | Double-click and Shift-click | All, shared code in `canvas` |
| `tool_kind::control` | AbiWord's format toolbar holds font, size and style boxes | All; such a bar uses portable painting |
| `ruler::set_active_range` | Shows the text area between page margins | All, shared code |
| `ruler::set_markers`, `on_marker_move` | Draggable margins, indents and tabs | All, shared code |
| `non_client::pointer_click` | Lets a strip take button events before the client | All, shared code |

## Fixed along the way

- X11 destroyed a shell before its children. Outside an Xt dispatch that frees
  the tree at once, so destroying a window with controls from posted work crashed.
- X-family `code_edit` input passes `XLookupString` bytes on as UTF-8, although
  they are Latin-1. Canvases use a separate UTF-8 path; `code_edit` still has this
  bug.
- With a canvas focused, X-family menu shortcuts never fired, because keys taken
  by the canvas skip the toolkit's accelerators. `dispatch_x_keyboard` honors them.
- GEM's press path never focused a canvas, so nothing could be typed into one.

## Square icon tools

`toolbar` tools with an icon and no text are square, as long as the strip
is thick, in the shared `item_bounds` layout. `get_item_bounds(id)` exposes
a tool's rectangle.
