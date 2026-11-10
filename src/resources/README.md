# Fyodor graphics resources

`fyodor.json` declares 17 toolbar icons under `icons.toolbar.<command>`.
Each has an unqualified color PNG and a `color: monochrome` PNG. Native's
`resources::get()` selects the latter automatically on GEMix and X11.
These are fallbacks: `ui::tool_icon()` first asks Native's `standard_icon()`
for common commands. GEM supplies Ted's original 16×16 New, Open, Save,
Save As, Undo, Cut, Copy and Paste bitmaps from one shared Native catalog.
Fyodor contains no copies of that artwork. Save As remains a menu command;
the lookup supports its icon without adding a new toolbar action.

The audited Ted catalog has no Redo, Close, Print or Find icon. Redo, Find
and document-formatting tools retain their existing fallback drawings.
Other backends keep their existing resources when Native returns no icon.

Both variants preserve Fyodor's existing 16×16 drawings. These are original
project artwork, copyright 2026 Tomaz Stih, GPL-2.0-or-later, not downloaded
icons or copies of AbiWord's XPMs. `export_icons.cpp` retains the original
Native drawing instructions. The monochrome adaptation removes paper and
folder fills and changes the remaining strokes to solid black. Undo/redo
use two-pixel strokes in the monochrome variants so their shapes survive
Native's disabled stipple. Pixels are
black with either zero or full alpha; there are no gray pixels, partial alpha,
or antialiased edges. Transparent interiors let Native tint the complete mask
white on a black selected button. Opaque white backgrounds would defeat
Native's mask detection and selected-state tinting.

## Module transfer

AbiWord's `ap_Toolbar_Icons`/`xap_Toolbar_Icons` platform and XPM lookup tables
are replaced by this manifest and the public Native resource API.

| Entry point | Responsibility |
| --- | --- |
| `ui::graphics_resources()` | Decode the embedded bytes once into an immutable owning `native::resources` container |
| `ui::tool_icon(command)` | Prefer Native's shared standard command icon; otherwise resolve and decode `icons.toolbar.<command>`; unknown commands return null |
| `export_icons` | Reproduce both asset sets from the original drawing functions, outside normal application builds |
| Native `rc` | Validate the manifest and generate the pack and embedded translation unit |

The decoded images are owned by Native toolbar items. Malformed resources
fail explicitly; only a command with no icon returns null.

## Build and deployment

`make` builds Native's `rc`, compiles the manifest, and links the generated C++
into Fyodor. `make resources` produces just the pack and generated source:

- `build/<mode>/src/fyodor.resources`
- `build/<mode>/src/ui/resources.cpp`

CMake SDK builds generate `build/platforms/<backend>/<mode>/resources/`.
The platform driver prepares resources in the Linux SDL2 SDK before Windows
or Haiku cross-compilation and passes `FYODOR_RESOURCE_CPP` to the destination
build. A manual cross-build must supply that generated file explicitly.
Never run a cross-compiled `rc` on the build host.

The executable embeds all fallback variants and needs no asset files at runtime.
Headless builds have no resource or Native dependency. New graphics belong
in the same manifest, because Native rc emits one fixed accessor per program.
PNG and manifest changes rebuild the pack and relink its consumers.

## Regenerate the artwork

From the repository root, after a normal Make build:

```sh
cmake --build build/native-release --target export_icons -j8
build/native-release/export_icons src/resources/icons
make resources
```

The exporter uses only Native's public graphics/image API and opens no window.
PNG artwork is checked in; normal builds do not run the exporter.

## Highlighting

Fyodor constructs `native::main_menu` and `native::toolbar` controls and does
not paint their highlights. Native supplies the toolbar button background and
text/mask colors from its theme. GEM uses reverse video; X11 obtains its
foreground/background from Athena resources. Color backends keep their own
native feedback. Fyodor supplies artwork; Native supplies control state rendering.

Native's `theme::draw_toolbar_icon` owns icon state painting. GEM renders
disabled masks with black/transparent stippling; a gray image tint would
threshold to white and disappear. Undo and redo therefore remain visible when
unavailable, while Native still prevents disabled commands from activating.

The document's text colors are separate from menu and toolbar chrome.
Document selection uses Native's selection painter through `native_painter`;
it does not use this resource pack.
