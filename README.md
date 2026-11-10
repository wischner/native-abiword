# Fyodor

Fyodor is a word processor rebuilt from the ground up from
[AbiWord](https://github.com/fork-vault/abiword), in modern C++20, on the
Native UI library. It reads and writes AbiWord's `.abw` files.

It is an early port. Paragraph and character formatting, styles, tabs, pages,
columns, images, undo, clipboard, find and replace all work. Tables, lists,
headers and footers do not yet: see [port status](docs/notes/PORT-STATUS.md)
before saving a file that uses them.

## Build and run

```sh
make                         # bin/fyodor: desktop
make run                     # build, then open it on your desktop
make headless                # bin/fyodor-headless: no display dependency
make debug                   # bin/fyodor-debug with ASan and UBSan
make clean                   # remove build/ and bin/
```

Dependencies: Linux with Docker, GNU make, Python 3 and an X11 or Xwayland
desktop. Every build goal runs inside the `wischner/gcc-x86_64-linux-sdl` image
(GCC 11, CMake, SDL2 and the other development packages), so the host needs no
compiler or headers. Objects go to `build/sdl2/`, binaries to `bin/`. `make run`
and the `SDL2, Local` F5 profile run the binary in the same image, drawing on
your desktop through its X11 socket.
[Native](https://github.com/wischner/native) (branch
`master`) is cloned into `build/deps/native` on first build;
`make update-native` moves it to the branch's newest commit. To build against
your own checkout, pass `NATIVE_ROOT=/path/to/native`.

**VS Code F5** builds and debugs the selected backend. Profiles are named
`<name>, <env>`: `SDL2, Local`, `X11, Xephyr`, `GEM, Rasta` and so on.

Toolbar graphics are embedded with Native's resource compiler. GEM and X11
automatically select black-and-white icons; Native supplies menu and toolbar
highlights. See [graphics resources](src/resources/README.md) for regeneration
and cross-build details.

Both GEM toolbar rows are 20 pixels high, with 20×20 command buttons,
20-pixel group separators and 16×16 icons. Hosted combo boxes touch their
neighbors and draw only their right border. These dimensions come from
Native's metrics. Common commands reuse Ted's icons through Native's shared
catalog. To update the cached Native source and test the GEM F5 Debug build:

```sh
docker pull wischner/gcc-x86_64-gemix:latest
make update-native
python3 -B scripts/build/platforms.py linux-gemix-gemd --mode debug
```

The Docker image supplies the GEM SDK and runtime; Native is built from the
Git checkout in `build/deps/native`, not taken from the image or automatically
from `../native`. Use Native commit `4e6f805` or a descendant for configurable
separator widths. Pulling the Docker image alone does not update Native.

The [GEM UI audit](docs/notes/GEM-UI.md) records the reference applications,
shared controls, icon sources and remaining custom document rendering.

All application UI goes through Native's public API. The default build selects
Native's SDL2 backend; `make platform-test` builds the others. See
[platform status](docs/notes/PLATFORM-STATUS.md).

## Tests

```sh
make test                    # unit tests under ASan and UBSan, no display
make ui-test                 # the real window, through Native's input hooks
make platform-test           # every backend SDK; PLATFORM=linux-x11 for one
make list-platforms
```

See [testing](docs/notes/TESTING.md). Tests run in Docker, so ASan works even
on hosts that preload a security agent; see [that note](docs/notes/HOST-ASAN.md).

## Desktop icon

Fyodor's desktop icon is [`assets/icons/FYODOR.PNG`](assets/icons/FYODOR.PNG): 32x32
pixels, black ink on white paper, transparent outside the icon. The build
copies it next to the executable: `make` writes `bin/FYODOR.PNG`, and every
release zip has `FYODOR.PNG` beside `fyodor`.

To build a GEMiX image, take `FYODOR.PNG` from the `linux-gemix-gemd` release
zip, convert it with GEM's `resgen` and install it under the lowercase program
name, matching the program entry `/opt/gem/Programs/fyodor`:

```sh
resgen icons -o fyodor.rsc FYODOR.PNG
install -m 0644 fyodor.rsc /opt/gem/share/gem/icons/fyodor.rsc
```

GEM Desktop then shows the icon for `fyodor` in the Programs folder. Without
the file, Desktop shows the standard program icon. GEM's
[program deployment guide](https://github.com/triglav-os/gem/blob/main/docs/guides/PROGRAM_DEPLOYMENT.md)
explains how Desktop finds icons.

## Releases

Pushing a `vX.Y.Z` tag builds every backend and publishes one zip per
platform on the GitHub release page. See [releasing](docs/notes/RELEASING.md).

## Documentation

- [Architecture](docs/ARCHITECTURE.md): layers and design decisions
- [Port status](docs/notes/PORT-STATUS.md): what has moved from AbiWord
- [Native changes](docs/notes/NATIVE-CHANGES.md): what this project added to Native
- [Licensing](docs/notes/LICENSING.md)
- [Coding standards](docs/standards/)

## License

GNU General Public License, version 2 or later, as AbiWord. See [LICENSE](LICENSE)
and [third-party notices](THIRD-PARTY-NOTICES.md).
