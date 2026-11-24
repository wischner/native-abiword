# Testing

## OPEN LOOK popup separator spacing (2026-11-24)

Builds and F5 now share `native-openlook-sdk:1`, automatically built from
Native's OPEN LOOK Docker context when absent. Its XView patch makes blank
popup separators half an OLGX button high, matching olwm desktop menus.
Sizing, repaint offsets and pointer hit testing use the same row geometry.

The Debug build against the cached Native dependency passed 1117/1117 desktop
checks. Native's existing `native_openlook_runtime_tests --desktop-controls`
executable from the sibling checkout also passed under Xvfb/olwm in this SDK;
it checks command and toggle menu gap heights, blank repainting, reopening,
and pointer/keyboard navigation across separators. Restart an existing F5
session to load the patched XView library.

## OPEN LOOK desktop review (2026-11-24)

Native backend repairs live in the sibling `../native` repository, with no
new consumer compatibility patch. The cached dependency remains the default.
`NATIVE_ROOT="$PWD/../native" python3 -B scripts/build/platforms.py linux-openlook --mode debug` rebuilds
Fyodor and runs its desktop checks with SDK 1.4.0. The checks cover the gap
before zoom, hosted-control layout, centered Find bounds and Enter actions.
Open Look and SDL Debug builds each passed 1117/1117 desktop checks against
the sibling checkout. The SDL physical keyboard suite also passed at 96 DPI.

`tests/ui/test_openlook_keyboard.py` drives real XTest keys under Xvfb/olwm.
It checks forward traversal through both text fields, both checkboxes and
all four buttons, reverse wrapping, default Find, focused Replace/Replace All
and Close, and the resulting saved document. Opening and reopening Find
must focus its first tab stop without a pointer click; the test types directly
after opening and checks clean application exit. It uses a temporary document
and preference directory. Captures go to `build/openlook-screenshots/`.
The scenario also clicks a toolbar combo and captures it after dismissal to
check that no keyboard-focus arrow remains on an unordered control.
The physical keyboard scenario passed; toolbar, centered-dialog and Format
menu captures were visually inspected. These runs use ASan/UBSan with leak
detection disabled, matching the platform runner.
Run it in the Open Look SDK with an X display and olwm:

```sh
python3 -B tests/ui/test_openlook_keyboard.py build/platforms/linux-openlook/debug/bin/fyodor
```

## Dialog and persistence review (2026-11-15)

The inventory and filesystem decisions are in
[NATIVE-CHANGES.md](NATIVE-CHANGES.md#dialog-review-2026-11-15).

`make test` runs six suites with ASan/UBSan: 411 checks passed, including
39 preference checks and 19 file-publication checks. They cover missing
configuration directories, XDG overrides and relative/empty fallback,
invalid values, private permissions even with umask 000, unreadable files, unwritable directories,
failed replacement, preservation of an existing document, unrelated `.tmp`
files and symlink collisions. Permission tests run as the non-root Docker user.

`make MODE=debug ui-test` builds the complete SDL desktop with sanitizers and
runs three isolated processes. The first changes zoom and both search flags,
uses the resulting search behaviour and saves formatted document data. The
second restores the preferences, checks the displayed zoom and case/whole-word
behaviour, then loads the saved paragraph/style/tab/page settings. The ordinary
desktop scenario follows with a clean preference state. It checks all eight
Find stops in both directions, wrapping, Enter activation, uncreated/hidden stops,
reopening focus, native field heights, non-overlap and containment at normal,
enlarged and undersized requested bounds. All three picker Cancel callbacks
leave the document unchanged. A forced preference-save error reaches the
status bar and stderr. The expected `Cannot create …preferences.conf`
diagnostic in the test log is part of that negative test.

`make ui-keyboard-test` adds real XTest keyboard/pointer events against the SDL
application under Xvfb at 96 and 144 DPI (100% and 150%). It checks forward and
reverse Find navigation, checkbox Space activation and persistence, keyboard
Close, chooser Cancel, save-prompt Cancel selected with Tab, About dismissal,
and Save As. The saved document must contain the physically typed tab between
`a` and `b`. Screenshots of Find, Open, Save As, About, the confirmation and
the document go to `build/dialog-screenshots/`. These captures were visually
inspected: labels, fields, checkboxes, buttons and multiline text are visible
without overlaps at both tested scales. The script uses X11/XTest, Xvfb, xrdb
and ImageMagick already present in the existing SDL SDK image.

The GEM/gemd and X11 Debug platform builds and desktop scenarios also passed.
GEM retains 20-pixel toolbar bands and 18-pixel hosted fields, while its Find
fields use the new 24-pixel form height. The Native compatibility patch was
also applied twice to a pristine `426a42f` source snapshot to verify clean
application and idempotence.

Limits: these checks are automated plus screenshot inspection, not a manual
session on the user's display. Insert Image and the three error-message uses
were reviewed through their shared dialog implementation and application
callbacks, rather than each receiving a separate screenshot. Other Native
backends, extreme DPI/font overrides and non-Linux preference paths were not
runtime-tested here. Atomic rename is tested for ordinary failures, not power
loss. Tests use fresh temporary configuration, never the user's preferences.

UI tests use Native and nothing else, so one test source covers every backend.

| Layer | Runs | How |
| --- | --- | --- |
| `tests/unit/` | `make test` | Headless, with `fixed_font_provider` for exact metrics |
| `tests/ui/test_desktop.cpp` | `make ui-test`, `make platform-test` | The real `main_window` |

## The desktop test

The test links the application's window and enters through Native's public
`on_native_*` entry points: `canvas::on_native_text_input`,
`on_native_key`, `on_native_mouse_click`, `app_wnd::on_native_menu`,
`toolbar::on_native_command` and `combo_box::on_native_selection`. It verifies
results in the saved `.abw`, the view, and the pixels of the painted back buffer.
It reaches private members through `friend class ::desktop_test`, which keeps
test accessors out of the public interface.

Committed test text is preceded by a fresh unhandled physical key and followed
by release. This matches Native's shortcut-text suppression contract: release
of a handled key does not clear the suppression for that key's text event.

`test_resources.cpp` also checks all 17 embedded icon pairs, explicit GEM/X11
selection, fallback, decoded dimensions, binary alpha/ink, active toolbar
lookup and Native theme feedback. On monochrome backends it checks the
selected background is black and the selected foreground is white. These
checks run within the desktop test after window creation.

Toolbar layout checks cover both rows, native field heights and centering,
square command slots, separator lengths and actual gaps between command groups,
and resizing down to 640 pixels and back. Resource checks
verify shared standard-icon identity for GEM commands and preserve embedded
fallback checks for other tools/backends. The desktop test prints the actual
band, field and icon dimensions alongside its results.
Joined-field checks require right-only combo borders
and contiguous style, font, size, color and Bold rectangles,
including after window resizing.

`test_selection.cpp` paints actual document text through `native_painter`.
At 75%, 100% and 150% zoom with scrolling, it selects part of a word across
colored text with a background and decorations. It checks Native selection
background/foreground pixels, unchanged pixels outside the selection, intact
stored formatting, and restoration of graphics state when a callback throws.
On GEM/X11 it additionally requires black selection background and white ink.

The editing scenario names no toolkit. A backend contributes only a display: SDL's dummy driver,
Xvfb from that backend's Docker image, or a private `gemd` server for GEM, as
Native's own tests do.

Typing timings include both a 300-key edit-only loop and 30 edits that each
wait for an event-loop paint. The latter includes backend scheduling and a
5 ms polling timer; it is not an OS-keyboard or Rasta-viewer latency test.
The caret timer is stopped during the scenario so blinking cannot satisfy a
paint wait. Paint timeouts fail the test, and the final back buffer is checked
again after the painted edits. Timings are diagnostic, not pass/fail limits.

**What it does not cover.** Those entry points sit just above each backend, so
the backend's own step from OS event to `on_native_*` is outside this test. That
step belongs to Native and is covered by Native's tests, such as
`native_menu_runtime_tests` and `native_gemix_input_tests`. Passing here on a
backend therefore does not prove a user can type there; see
[PLATFORM-STATUS.md](PLATFORM-STATUS.md).

## Platforms

`make platform-test` runs `scripts/build/platforms.py`, which configures, builds
and runs the desktop test in each image from `scripts/build/platforms.json`.
Logs land in `build/platforms/<name>/<mode>/`.

To reproduce the verified GEM F5 Debug build and its 819 desktop checks:

```sh
docker pull wischner/gcc-x86_64-gemix:latest
make update-native
python3 -B scripts/build/platforms.py linux-gemix-gemd --mode debug
```

`--build-only` skips the test, as the F5 prelaunch task does. The command above
also runs it, using a private gemd server. It builds Native from
`build/deps/native`; commit `4e6f805` or a descendant is required for the
separator spacing checks. Docker supplies the SDK/runtime separately.

F5 profiles are named `<name>, <env>`, such as `GEM, Rasta`. Each builds through
the same script and debugs through `scripts/debug/linux.py`.
The GEM session wrapper monitors both Rasta and the launched command, following
Native's Rasta session cleanup pattern. Closing the outer viewer ends its
private debugger/server/inferior session; command exit removes the viewer and
preserves the command's status. GDB/MI keeps its input pipe open and session
logs go to stderr. The viewer-close regression was reproduced with the old
wrapper and passes with this cleanup using `gcc-x86_64-gemix:latest`, Xvfb,
and the existing F5 Debug executable. Diagnostic scripts and captures are in
`build/shutdown-check/`. A real X11 mouse click on Fyodor's own close button
also reports `exited-normally` through GDB/MI; command-exit status preservation
and removal of the viewer pass in both checks.

The GEM toolbar-popup regression is in Native's existing
`tests/native_gemix_control_checks.cpp`, executed by
`native_gemix_input_tests`. It uses real Rasta packets, checks font/size row
selection over a canvas in the standard compact toolbar band,
dropdown/change notifications, outside dismissal,
and resumed document input without a leaked click. Both direct GEM and gemd
pass. Fyodor's `test_format_bar` checks the application callbacks for style,
font, size, color and zoom; it does not substitute for that backend test.

Status placement is checked both in Fyodor and Native. The application's
status extent must equal the active theme height, and the selected host must
match the platform convention. Native's `native_ruler_toolbar_tests` covers
resizing and both scrollbar visibility policies. GEM resource tests render
undo/redo through the real theme in enabled and disabled states and require
visible, reduced disabled ink. A live GEM framebuffer was also inspected with
both scrollbars visible.
The GEM runtime test checks the 20-pixel Gemscape/Ted band height for the
pinned SDK. Fyodor checks that GEM uses frame scrollbars and that the status
strip ends exactly at the canvas's bottom and right edges. Native's real
Rasta input test checks AES frame flags/dimensions, arrow and page clicks,
both thumb drags, content shrink, resize and destruction. This verifies the
controls themselves, including their mouse routing, not just drawn replicas.
