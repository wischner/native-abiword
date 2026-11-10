# GEM UI consistency audit

Reviewed 2026-11-10 against the local GEM sources and the published
`wischner/gcc-x86_64-gemix:latest` SDK. The initial audit used image digest
`sha256:14c2c1638d545a175a9e16489f32a47a392c2b1ce44e140f1fffba1a96b6b423`;
its recorded GEM revision is `d940cf31ee9e85af0264a83abd1526d4475ffb0c`.
Final toolbar and F5 validation used the refreshed image digest
`sha256:891f7938bf7b468f8c2342fb2bb66d0dd1c2cce91ae10a6026b8c92628be6c09`.

## Authoritative references

- GEM `lib/gemctl/gemctl.c`: `gemctl_chrome_layout`, toolbar band drawing,
  bitmap centering and separators.
- GEM `src/apps/ted/toolbar.c`: square full-band slots, hidden incomplete
  buttons, unchanged 16×16 command bitmap rows, disabled stippling.
- GEM `src/apps/gemscape/gemscape_ui.c`: shared chrome layout, square
  navigation slots, a single-line address editor inside the toolbar border.
- Native `include/native/{toolbar,theme,combo_box,standard_icon}.h` and
  `lib/native/toolkits/gemix/`: public composition and GEM backend policy.

Gemscape has an address editor, not a second row of font combo boxes. Its
single-line placement and shared band geometry are the relevant references;
Fyodor retains Native combo boxes for its choice controls.

## Layout changes

Fyodor forced both rows to 30 pixels and supplied redundant absolute combo
positions/heights even though Native's toolbar places hosted controls. Native
also inherited the generic 24-pixel single-line field height on GEM. Reducing
only the application strip would therefore leave native fields overflowing.

Both rows now use the active Native toolbar and field metrics, including
Native's insets. The GEM theme calls the same `gemctl_chrome_layout` as the
reference applications. With the audited SDK, this gives 20-pixel bands,
18-pixel fields and 20-pixel square command slots. Toolbar icons remain
16×16. Other backends reserve their own field heights and insets.

Each command-group separator reserves one square button's width, calculated
from the configured band extent minus Native's toolbar padding. Native's GEM
toolbar separator draws its trailing edge, so no part of the empty slot appears
to belong to the following button (notably Align Left). This uses Native's
separator-length and `draw_toolbar_separator` changes, published in commit
`4e6f805` and fetched into the normal Git-managed F5 dependency.

Native owns field placement, full-slot clipping, borders, disabled feedback
and separators. Shared metric lookup uses the root window's graphics context:
SDL2 child canvases have no native window graphics context of their own.

Toolbar combo boxes use `border_sides::right` and
`tool_item::control_fill_width`: adjacent fields touch and each draws only
its trailing divider. The toolbar supplies the horizontal rule, avoiding
double top/bottom borders and paired side borders. This also applies to zoom.
The zoom field directly touches the Find button, with no left border or
intervening toolbar separator.
The text-color field keeps its right border and directly touches the Bold button,
with no intervening toolbar separator.
Vertical field sizing and dropdown behavior are unchanged. This configuration
requires the shared Native `control_fill_width` addition, included in Native
commit `abdfcc7` and the current Git-managed dependency checkout.

## Standard artwork

| Operation | GEM source and use |
| --- | --- |
| New, Open, Save | Shared Native catalog of Ted's original bitmaps; used in row one |
| Undo, Cut, Copy, Paste | Same shared catalog; used in row one |
| Save As | Same catalog; supported by icon lookup, existing menu action retained |
| Redo, Find | No entry in the audited Ted catalog; retain existing Fyodor fallback |
| Close, Print | No entry in the audited catalog; no new commands or buttons added |
| Bold, Italic, Underline, Strike, Alignment | Document-specific existing Fyodor artwork retained |

Fyodor adds no bitmap copies. The shared catalog resides in Native, with Ted's
upstream provenance and license. Existing embedded fallbacks remain necessary
for other backends. A future backend icon can replace a fallback through the
same semantic lookup without application drawing code.

## Native controls and remaining custom content

Native already provides `tree_view` (hierarchy, selection, expansion and
navigation), `table_view` and `table_model` (columns, virtual rows, selection,
scrolling and column resizing), `list`, `list_box`, `combo_box`,
`button`, `split_view`, `scrollbar`, `toolbar`, `ruler`, `status_bar`, text
editors and file dialogs. GEM adapters and shared Native behavior own these
controls; applications should use the public interfaces.

Fyodor already uses Native toolbars, combos, rulers, status, file dialogs,
find/replace text fields and buttons, and canvas scrolling. It has no custom
tree or grid UI to replace. Its document model's tables are not UI table
widgets; document-table support remains outside this change.

`document_canvas` remains custom because it renders paginated rich text,
selection, caret, images and document layout. Neither a tree nor a table
control supplies these semantics. The find/replace window remains an
application-specific composition of Native controls. No command handlers,
shortcuts or document behavior were replaced.

## Validation

The shared changes are available on Native's `master` branch. Update the
cached Git checkout and build with:

```sh
docker pull wischner/gcc-x86_64-gemix:latest
make update-native
python3 -B scripts/build/platforms.py linux-gemix-gemd --mode debug
```

The desktop suite checks both bands, field alignment, square icon slots,
resize down and back, shared icon identity and fallback rendering. Existing
checks exercise editing, command/selection callbacks, clipboard, undo/redo,
formatting, find/replace, rulers, document loading and painting. Raw operating
system input routing is a separate Native test responsibility; public callback
coverage alone does not establish keyboard or mouse transport behavior.

Verified on 2026-11-10:

| Check | Result and scope |
| --- | --- |
| GEM/gemd F5 Debug | 819/819; normal Git-managed Native checkout at `4e6f805` |
| Direct GEM Release | 819/819; sibling Native checkout containing the same separator changes |
| SDL2 Release | 668/668 on the earlier joined-field revision; final separator changes have not been rerun on SDL2 |
| Document/editing unit suites | 353/353 under ASan/UBSan during the initial UI audit |
| Shared icon provenance | All 128 bitmap rows in the eight shared icons match Ted's source |
| GEM visual comparison | 20-pixel rows and square buttons, 20-pixel group gaps, 18-pixel fields and 16×16 icons |
| Debug session shutdown | Real X11 close click exits Fyodor normally; closing Rasta ends the private GDB/server/inferior session; command status is preserved |

Native's toolbar lifecycle and real Rasta input suites passed during the
shared-control work, including combo selection over a canvas, dismissal and
resumed input. The final application suite verifies square command bounds,
separator lengths and actual spacing before Undo, Cut and Align Left at normal
and reduced window widths.

Logs for the final F5 run are in
`build/platforms/linux-gemix-gemd/debug/{configure,build,test}.log`.
Direct GEM captures are in `build/toolbar-borders/`; shutdown diagnostics are
in `build/shutdown-check/`. These are local build artifacts, not repository
fixtures. Other backends were not rebuilt for the final separator revision,
and no interactive human acceptance session was run.

The initial SDK image rejected Fyodor's complete menu through gemd. The final
GEM/gemd validation uses the refreshed SDK listed above and succeeds with the
complete application menu. Both the GEM SDK image and the separate Native Git
dependency must be updated; replacing one does not refresh the other.
