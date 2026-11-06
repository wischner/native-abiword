# Platform status

Recorded 2026-11-06. "End to end" means real input went through the toolkit into
the running application.

| Backend | F5 profile | Builds | Desktop test | Typing verified by |
| --- | --- | --- | --- | --- |
| SDL2 | SDL2, Local | Yes | 79/79 | Native routing test |
| X11 | X11, Xephyr | Yes | 79/79 | End to end: click, type, Ctrl+S, saved file |
| Motif | Motif, Xephyr | Yes | 79/79 | End to end |
| OPEN LOOK | OPEN LOOK, Xephyr | Yes | 79/79 | End to end |
| Window Maker | Window Maker, Xephyr | Yes | 79/79 | End to end |
| GEM (gemd) | GEM, Rasta | Yes | 79/79 | Real Rasta key packets in Native's input test |
| Haiku | Haiku, VM | Cross-build | 75/77 | **Not verified** |
| Wayland | None | Yes | Not run | **Not verified** |
| elementary | None | Yes | Not run | **Not verified** |
| Windows | None | Cross-build | Not run | **Not verified** |
| macOS | None | Not built | Not run | **Not verified** |

## Known limits

- **GEM** types ASCII only, because its key packets carry nothing more.
- **Haiku:** the two failing checks are the paint check. `hotel` captured a solid
  black screen over SSH, so the display was not rendering in that session.
  Compare by running `Haiku, VM` from the desktop.
- **Windows, Wayland, elementary, macOS:** text routing is written, and all but
  macOS compile, but none has been run. macOS code has not been compiled.

The end-to-end runs used a throwaway XTest probe, which is not in the repository.
`tests/ui/test_desktop.cpp` remains the portable suite and enters above the
backend, so see [TESTING.md](TESTING.md) for what it cannot show.
