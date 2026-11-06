# Writer

Writer (codename **fyodor**) is a word processor rebuilt from the ground up from
[AbiWord](https://gitlab.gnome.org/World/AbiWord), in modern C++20, on the
Native UI library. It reads and writes AbiWord's `.abw` files.

It is an early port. Paragraph and character formatting, styles, tabs, pages,
columns, images, undo, clipboard, find and replace all work. Tables, lists,
headers and footers do not yet: see [port status](docs/notes/PORT-STATUS.md)
before saving a file that uses them.

## Build and run

```sh
make -j8                     # bin/fyodor: desktop
bin/fyodor document.abw
make headless -j8            # bin/fyodor-headless: no display dependency
bin/fyodor-headless in.abw --to out.html
make debug -j8               # bin/fyodor-debug with sanitizers
make clean                   # remove build/ and bin/
```

Dependencies: Linux, GCC with C++20, GNU make, `flock`, CMake 3.20 and Python 3.
Native comes from the sibling `../native` checkout; override with
`NATIVE_ROOT=/path/to/native`. On Ubuntu without development headers the build
fetches them into `build/sdk/` and installs nothing.

**VS Code F5** builds and debugs the selected backend. Profiles are named
`<name>, <env>`: `SDL2, Local`, `X11, Xephyr`, `GEM, Rasta` and so on.

All application UI goes through Native's public API. The default build selects
Native's SDL2 backend; `make platform-test` builds the others. See
[platform status](docs/notes/PLATFORM-STATUS.md).

## Tests

```sh
make test                    # unit tests, no display
make ui-test                 # the real window, through Native's input hooks
make docker-test             # unit tests under ASan and UBSan
make platform-test           # every backend SDK; PLATFORM=linux-x11 for one
make list-platforms
```

See [testing](docs/notes/TESTING.md). ASan cannot start on hosts that preload a
security agent; see [that note](docs/notes/HOST-ASAN.md).

## Documentation

- [Architecture](docs/ARCHITECTURE.md): layers and design decisions
- [Port status](docs/notes/PORT-STATUS.md): what has moved from AbiWord
- [Native changes](docs/notes/NATIVE-CHANGES.md): what this project added to Native
- [Licensing](docs/notes/LICENSING.md)
- [Coding standards](docs/standards/)

## License

GNU General Public License, version 2 or later, as AbiWord. See [LICENSE](LICENSE)
and [third-party notices](THIRD-PARTY-NOTICES.md).
