# Plan 010: The `nnh` tool and `nnh run`

Status: draft
Spec: [spec.md](spec.md) · Decision D23 (below) · Updates D8

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | A host tool; games don't change. The engine side (stdio redirect in Development builds) already exists |
| II 3DS constraint | ✅ | Nothing on the console changes |
| III Build speed | ✅ | `nnh` is its own target in the PC presets, never a dependency of games; small, no third-party code |
| IV Event-driven | ✅ | Not engine code |
| V No hidden costs | ✅ | Tool code follows `cpp.md` (no exceptions/RTTI); `std::string`/`std::vector` and one helper thread are fine in a host program |
| VI Spec + test first | ✅ | Protocol pieces are pure and unit tested first; loopback fakes of the netloader and FBI test the network code |
| VII Small files | ✅ | One concept per file under `tools/nnh/src/` |
| VIII Documented API | ✅ | New guide `docs/guides/nnh-tool.md`; `nnh help` |
| IX Clean and legal | ✅ | Protocol facts from devkitPro's GPL sources (Homebrew Launcher netloader, libctru 3dslink), FBI's servefiles; no code copied |
| X Safe by default | ✅ | Private addresses only; sockets open only during a run/install; HTTP serves exact file names only |

## Design

### Decision D23 — `nnh` speaks the 3dslink protocol itself

The tool implements the Homebrew Launcher netloader's protocol (and FBI's URL protocol) in its own small
C++ code instead of calling devkitPro's `3dslink` or a Python script: NNH Link (spec 011) needs the same
code on both ends, it works without `3dslink` on the PATH (Linux, CI), and compression isn't needed —
the netloader inflates any valid zlib stream, and a zlib stream of **stored** (uncompressed) blocks is
valid, so no compression library is involved. D8 ("start compatible with 3dslink") stays true.

### The netloader protocol (facts from the Homebrew Launcher's `netloader.c`)

| Step | PC → console | Console → PC |
|------|--------------|--------------|
| discover | UDP `3dsboot` to port 17491 (broadcast) | UDP `boot3ds` from the console's address |
| connect | TCP to console:17491 | |
| header | `int32` name length, name (saved as `sdmc:/3ds/<name>`), `uint32` file length | `int32` 0 (or < 0: can't create the file) |
| data | chunks: `uint32` size (**≤ 16 384**: the console's buffer, unchecked) + bytes of a zlib stream | `int32` 0 after the stream's end |
| args | `int32` length + NUL-separated arguments; argv[0] must be `sdmc:/3ds/<name>` (libctru opens the romfs from it) | the console appends `<ip-hex>_3DSLINK_` and launches |
| logs | the PC listens on TCP 17491; a Development build connects back (libctru `link3dsStdio`) and writes stdout/stderr until it exits | |

Integers are little-endian (both sides). FBI ("Receive URLs over the network"): TCP to console:5000,
`uint32` big-endian length + URLs separated by `\n`; FBI downloads them and answers (or closes) when done.

### Layout (`tools/nnh/`, built in the PC presets as target `nnh`)

| File | Responsibility |
|------|----------------|
| `src/main.cpp` | entry: parse, dispatch, exit status |
| `src/cli.{hpp,cpp}` | subcommands and options (`build`, `run`, `install`, `help`; `--release`, `--pc`, `--console`) |
| `src/zlib_stored.{hpp,cpp}` | zlib stream of stored blocks + Adler-32 (pure) |
| `src/netloader.{hpp,cpp}` | message framing and the client session over a `Stream` interface (pure + I/O seam) |
| `src/fbi.{hpp,cpp}` | the URL message; a tiny HTTP/1.0 file server for exact paths |
| `src/net.{hpp,cpp}` | sockets (Winsock / BSD), timeouts, UDP broadcast, local address towards a host |
| `src/address.{hpp,cpp}` | parse IPv4, private/loopback check (pure) |
| `src/log_view.{hpp,cpp}` | parse engine log lines (`[f12 0.200s] I chan message`), colour them (ANSI; VT mode on Windows) |
| `src/manifest.{hpp,cpp}` | read `build/<preset>/nnh_games.txt` (pure) |
| `src/repo.{hpp,cpp}` | find the repository root, presets, run CMake (through devkitPro's MSYS2 bash on Windows) |
| `src/user_config.{hpp,cpp}` | the remembered console (`%APPDATA%/nnhge/nnh.ini`, `~/.config/nnhge/nnh.ini`) |
| `src/commands/{build,run,install}.cpp` | the subcommands |
| `tests/` | `nnh_tool_tests` with the in-house framework: unit tests + loopback fakes (netloader, log client, FBI) |

### The games manifest

`nnh_add_game()` appends one line per game to `<build>/nnh_games.txt` (cleared at the start of each
configure by the top-level `CMakeLists.txt`): `name|binary dir|has CIA`. `nnh` reads it to turn `<game>`
into `<binary dir>/<game>.3dsx`, the `<game>_3dsx`/`<game>_cia` targets and, on PC, the executable.

### Commands

- `nnh build <game> [--release] [--pc]` — preset `3ds-dev` (or `3ds-release`, `pc-dev`); configures it if
  `build/<preset>` has no cache; builds `<game>_3dsx` (3DS) or `<game>` (PC). On Windows, 3DS commands run
  in `C:\devkitPro\msys2\usr\bin\bash.exe -lc` like `scripts/dkp.sh` (`DEVKITPRO_MSYS2` overrides).
- `nnh run <game> [--release] [--console <ip>] [--pc]` — build; `--pc` runs the simulator executable.
  Else: console = `--console`, the remembered one if it answers the discovery, or the first one that does
  (broadcast, 3 tries over ~3 s); start listening on TCP 17491; send; save the IP; wait up to 10 s for the
  game's log connection (Release builds don't connect: "started; no log in Release builds"); print and
  save every line to `build/logs/<game>-<YYYYMMDD-HHMMSS>.txt` until the game closes the connection.
- `nnh install <game> [--console <ip>]` — build `<game>_cia` (3ds-release), serve it over HTTP on the PC's
  address towards the console (random port), send the URL to FBI, wait for its answer, stop serving.

### Engine side

Nothing new: Development builds already redirect stdio to the 3dslink host (spec 002, with the 1 s
non-blocking connect). `nnh run` relies on the netloader's `_3DSLINK_` argument like 3dslink does.

## Test strategy

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `nnh help`; builds on Windows and Linux |
| FR-2 | `tests/test_cli.cpp` (options), `test_manifest.cpp`; `nnh build hello` (Windows, Linux) |
| FR-3 | `test_zlib_stored.cpp` (Adler-32 vectors, block layout, decoded by the test's own inflater of stored blocks), `test_netloader.cpp` (framing, chunk limit), loopback: fake netloader thread receives a file + args, a fake game connects back with log lines; maintainer run (AC-1) |
| FR-4 | `test_address.cpp`; loopback discovery (UDP on 127.0.0.1); `test_user_config.cpp` |
| FR-5 | `test_log_view.cpp` (levels, channels, malformed lines pass through), mini-fuzz of the parser |
| FR-6 | `test_fbi.cpp` (message), loopback: fake FBI fetches the URL from the HTTP server |
| FR-7 | `test_address.cpp` (public addresses refused), HTTP 404 for any other path |
| FR-8 | the above |

## Risks and alternatives

- **The game's log connection** goes to the address the console saw the transfer come from: with several
  network interfaces (VPN, WSL), the PC must listen on all of them — it does (`0.0.0.0`).
- **Windows firewall** may block the inbound log/HTTP connections the first time (a prompt); the guide
  says so. Ports: 17491 (logs), a random one (HTTP for FBI).
- **Chunk size**: over 16 384 bytes overflows the console's buffer — a constant with a test.
- **Discovery** can fail on networks that block broadcast; `--console` always works and is remembered.

## Constitution Check (after design)

No change.
