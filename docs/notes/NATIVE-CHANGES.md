# Native changes made for fyodor

## Dialog review (2026-11-15)

The complete menu, toolbar and keyboard command inventory contains **nine
dialog uses**. There are no separate settings, properties, import-options,
export-options, directory-selection or advanced-search forms in this port.

| Dialog | Layout owner | Review and changes |
| --- | --- | --- |
| Find and Replace | Fyodor | Rebuilt with Native grids, measured labels, theme field/check/button heights, content-derived minimum dimensions, resize constraints, eight ordered stops and initial/reopen focus. Close returns focus to the document. |
| Open | Native | Filters/cancel callbacks reviewed; document focus restored; shared SDL chooser gets initial focus, traversal and button activation. |
| Save As / export | Native | Suggested name, extension, format filters and cancel callbacks reviewed; same shared SDL keyboard fixes. |
| Insert Image | Native | Image filters and cancellation reviewed; same chooser fixes and focus restoration. |
| Save changes confirmation | Native | Shared SDL button traversal, focused activation and content-based message sizing. |
| Could not open | Native | Same shared message-box improvements; diagnostics remain visible. |
| Could not save | Native | Same message improvements; atomic-save failures include their reason. |
| Could not insert | Native | Same shared message-box improvements. |
| About Fyodor | Native | Shared SDL message wrapping preserves paragraphs and the complete description. |

All nine were reviewed. One application-owned layout was rebuilt; the other
eight uses receive shared SDL dialog corrections, with three application
file-picker callbacks also restoring editing focus. The format toolbar and
rulers were reviewed separately: formatting remains document data, toolbar
fields retain Native's compact metrics, and the existing resize tests remain.

The reference is Native `426a42f`, specifically `src/vision_edit_form.cpp`,
`lib/native/input_geometry.cpp` and `lib/native/wnd_focus.cpp`. GEM separates
24-pixel standalone form inputs from 18-pixel toolbar inputs. Native's sample
explicitly orders controls, reads the theme after creation, excludes labels
and focuses the first field whenever opened. Fyodor follows that contract,
using the native grid manager and measured label widths rather than copying
the sample's absolute coordinates.

The review found additional dependency defects: opt-out document canvases lost
Tab to the new traversal dispatcher, and SDL's private file-picker/message-box
loops bypassed that dispatcher. SDL buttons/checks also lacked physical-key
activation, and message boxes ellipsized multi-paragraph text. The checked-in
`scripts/build/native-dialogs.patch` fixes these in Native's owning layers.
`patch_native.sh` checks applicability before changing any files and is
idempotent. Native commit `75cdcd488ac5baa2ab6bc426f4f9d498a11a2909`
published equivalent fixes upstream. CMake skips the compatibility patch on
that commit and its descendants, leaving their implementation and documentation
intact. Older checkouts still receive the patch; conflicting changes stop
configuration explicitly. The compatibility patch also updates Native's
architecture, custom control audit and feature matrix with its consumer-specific
scope. Failed platform stages print their log tail directly, including failures
in the host resource build used by cross-compilation.

The three application preferences (zoom and the two search flags) previously
had no persistence. They now load before the first refresh, validate values,
save immediately and affect the actual view/search behaviour after restart.
There is no preferences dialog and no uncommitted settings transaction.
Cancel on the three file pickers leaves the document unchanged. Document
formatting continues through the existing AbiWord serializer.

Linux configuration is `$XDG_CONFIG_HOME/fyodor/preferences.conf`, with the
standard `$HOME/.config` fallback. Relative XDG values are ignored, following
the [XDG specification](https://specifications.freedesktop.org/basedir/latest/).
Precedence is built-in defaults followed by the user file; this application
has no system-wide preference schema or command-line preference overrides.
There are no legacy settings files to migrate. macOS uses Application Support
when XDG_CONFIG_HOME is absent; Windows resolves APPDATA. Non-Linux runtime
behaviour was not validated in this review.

The common file writer stages in a unique private directory on the destination
filesystem, then renames. This replaces the unsafe predictable `document.tmp`
name, preserves existing document permissions and protects preferences with
0600. New configuration directories use 0700 without changing existing parent
permissions. Errors are reported rather than redirected to another location.
Atomic replacement protects against partial writes, but does not promise
power-loss durability (no fsync). Windows replacement semantics remain
unverified. User documents are not relocated into XDG storage.

Runtime diagnostics use stderr; Fyodor has no persistent log, cache, runtime
socket or additional application-data store. Build/test logs and screenshots
remain under ignored `build/`; developer session wrappers use temporary
directories with cleanup. No desktop writes to `/etc`, `/var`, or installation
directories were introduced. Log rotation and legacy migration are therefore
not applicable.

Validation commands and their limits are recorded in [TESTING.md](TESTING.md).

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
field height, borders, clipping and placement; the application supplies
control widths and derives separator lengths from the band metrics.
Single-line GEM fields fit inside the band rather than
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

Native commit `abdfcc7` adds `tool_item::control_fill_width`, allowing hosted
fields to touch without horizontal insets. Fyodor sets `border_sides::right`
on those fields. The text-color field touches Bold and the zoom field touches
Find, without an intervening group separator.

Native commit `4e6f805` makes `tool_item::length` apply to separators and adds
the shared `draw_toolbar_separator` theme hook. Fyodor sets every group gap to
one square button's width; GEM draws the divider at the gap's trailing edge.
This prevents half of a centered separator from visually extending Align Left
or another following button. Both commits are published on Native's `master`;
`make update-native` refreshes the Git-managed dependency used by F5.

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
