# Testing

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

It names no toolkit. A backend contributes only a display: SDL's dummy driver,
Xvfb from that backend's Docker image, or a private `gemd` server for GEM, as
Native's own tests do.

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

F5 profiles are named `<name>, <env>`, such as `GEM, Rasta`. Each builds through
the same script and debugs through `scripts/debug/linux.py`.
