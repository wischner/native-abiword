# Native changes made for fyodor

These extend [Native](https://github.com/wischner/native) on its
`master` branch, which includes the former `fyodor-native-extensions` branch.
A contract added to Native has to work on
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

## GEM typing and repaint cost

Fyodor avoids calling unchanged toolbar, combo-box and ruler setters: those
Native setters request repaints even when their values have not changed. GEM
unions the requests into one dirty rectangle, so updating these controls on
every character repainted much of the window through gemd. Unchanged canvas
content bounds also invalidated the full canvas, including its rulers.

On 2026-11-06, the Release GEM/gemd desktop test measured approximately 97 ms
per character including event-loop painting before these application changes,
and 37 ms after them. Editing without painting took about 0.12 ms per character.
The painted benchmark uses 30 sequential edits and 5 ms polling, without a
Rasta viewer; these are local diagnostic measurements, not end-to-end keyboard
latency guarantees. Ordinary edits still rasterize and upload the viewport;
caret blinking now invalidates only the visible caret rectangle. No Native
source changes were needed for this fix.

The F5 `GEM, Rasta` profile uses Debug, whose generated flags include `-g`,
AddressSanitizer and UndefinedBehaviorSanitizer, with no optimization flag.
The updated Debug test measured about 146 ms per painted key and 2.7 ms for
editing alone. Debug rasterization and instrumentation remain a substantial
cost; the Release comparison above must not be read as F5 performance.

## Square icon tools

Fyodor's two toolbar rows use Native's toolbar metrics instead of an
application-defined 30-pixel band. GEM's metrics follow the same
`gemctl_chrome_layout` used by Gemscape and Ted. Native owns slot spacing,
field height, borders, clipping and placement; the application supplies only
control widths. Single-line GEM fields fit inside the band rather than
retaining the generic 24-pixel height.

Common command artwork comes from `native::standard_icon(standard_icon_id)`.
The shared GEM catalog contains Ted's unchanged 16×16 New, Open, Save,
Save As, Undo, Cut, Copy and Paste bitmaps. Missing standard artwork returns
null, preserving the application resource fallback. These shared changes
are available on Native's `master` branch. Run `make update-native` to refresh
the cached Git dependency, or select a checkout with `NATIVE_ROOT=/path/to/native`.

`toolbar` tools with an icon and no text are square, as long as the strip
is thick, in the shared `item_bounds` layout. `get_item_bounds(id)` exposes
a tool's rectangle.

## Resource integration

Fyodor now consumes Native's existing `resources` and `rc` implementation.
All 17 toolbar commands have color and monochrome representations in an
embedded pack. The latter are binary black/transparent masks, so Native's
existing toolbar painter tints them with the theme foreground, including
white selected ink in GEM/X11. Menus and toolbar backgrounds already use
Native themes; no application palette override or Native modification was
needed. See [resource sources](../../src/resources/README.md).

## Document selection

The document view's fixed pale-blue highlight has been removed. Fyodor's
graphics adapter now uses Native's existing `theme::draw_selection` and
selection foreground getter. Text, underlines and other decorations are
redrawn through a selection clip; document background colors cannot obscure
the selection. This is an application adapter change with no Native API or
backend modifications. Pixel tests pass on SDL2, X11 and GEM/gemd.

## GEM toolbar combo input

Native's GEM event loop previously offered pointer input to canvas capture
before checking an open combo popup. A dropdown painted above the document
therefore lost its row clicks to the canvas. The GEM backend now routes both
press and release to an open popup first, commits selection on release, and
consumes outside dismissal without clicking through. Selection callbacks are
protected against control/window destruction. No application workaround or
public Native API change is involved.

Native's `native_gemix_input_tests` now drives real Rasta/AES clicks through
font and size combos hosted in a toolbar over a document canvas. It reproduced
the failure before the fix and passes on both direct GEM and gemd, as do both
transports' existing runtime tests. Fyodor's desktop suite separately verifies
style, font, size, color and zoom changes reach the document/view.

## Status placement and disabled icons

Native adds `status_bar(owner, content_canvas, height)` to express a document
footer without application backend branches. GEM selects the canvas host;
its status strip stays inside the scrollbar edges. Other backends retain the
outer window footer. Fyodor sizes the strip from Native's active theme. The
original owner-only status-bar constructor keeps its existing behavior.
The GEM height now follows the sample applications Gemscape and Ted's
`gemctl_chrome_layout`: `max(12, char_h + 4, box_h - 2)`. This produces a
20-pixel status band in the pinned SDK, rather than the previous 22-pixel
AES-title metric.

Native also adds `app_wnd::set_scroll_content(canvas *)` to associate the
document before window creation. Fyodor uses this once, on every platform,
with no toolkit checks. GEM creates actual AES frame scrollbars and forwards
native arrow/page/slider messages to the canvas; slider ranges follow content,
viewport and programmatic scrolling. The canvas reserves no duplicate tracks,
so the status band meets the AES work boundary without the old bottom gap.
Other backends retain their existing canvas implementations through the
default adapter. Nested GEM canvases continue to scroll within their own host.

Toolbar icon state painting now delegates to `theme::draw_toolbar_icon`. The
common path preserves its previous mask tinting and color artwork. GEM draws
disabled monochrome masks with binary stippling, like disabled stock text,
because gray image ink was thresholding to white and hiding undo/redo. The
monochrome undo/redo artwork has thicker strokes to retain its shape in this
state; enabled commands retain solid ink.
