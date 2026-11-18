# Spec 010: The `nnh` tool and `nnh run`

Status: implemented (approved and implemented 2026-11-18)
Author: NNHGE maintainers · Date: 2026-11-18
Depends on: [spec 002](../002-core-foundation/spec.md) (logs, the 3dslink stdio redirect),
[spec 009](../009-packaging/spec.md) (dist, `.cia`, FBI install). First part of ROADMAP M6; the console
app (NNH Link), `nnh serve` and pairing follow in spec 011 (see open questions).

## Problem and goal

Running a game on the console takes several steps and tools today: configure and build through
`scripts/dkp.sh` (on Windows), remember the console's address, call devkitPro's `3dslink` with the right
flags so the logs come back, and read raw log lines; installing a `.cia` needs a Python script. After this
spec one program does it: `nnh run hello` builds the game for the 3DS (only what changed), finds the
console, sends the game to the Homebrew Launcher, shows its log in the terminal, readable and saved, and
returns when the game closes. `nnh install` does the same for `.cia` files through FBI. The tool is also
the first half of NNH Link: the protocols and network code the console app will reuse.

## User stories

- As a game developer, I want `nnh run mygame` to build, send and show my game's log, so that trying a
  change on the console is one command, like running it in the simulator.
- As a game developer, I want the tool to find my console on the network and remember it.
- As a game developer, I want the log to be easy to read (levels, channels, colours) and kept in a file,
  so that I can look at it after the game crashed or closed.
- As a game developer, I want `nnh install mygame` to put the `.cia` on the console without copying files.
- As a contributor, I want the same tool to work on Windows and Linux without extra installs.

## Functional requirements

- **FR-1 One tool.** A host program `nnh`, built with the engine's repository (C++, decision D10), for
  Windows and Linux, with subcommands and `nnh help`. It works from anywhere inside the repository.
- **FR-2 Build.** `nnh build <game> [--release]` builds the game for the 3DS (Development by default,
  Release with `--release`), configuring the preset when needed and going through devkitPro's environment
  on Windows. `nnh build <game> --pc` builds and `nnh run <game> --pc` runs it in the simulator.
- **FR-3 Run on the console.** `nnh run <game>` builds, then sends the `.3dsx` (with its romfs) to the
  Homebrew Launcher's netloader (press Y on the console), compatible with what `3dslink` does, then shows
  the game's log until the game closes the connection; Ctrl+C stops waiting. Exit status: 0 if the game
  ran, non-zero if the build or the transfer failed.
- **FR-4 Finding the console.** Without `--console <ip>`, the tool looks for a console on the local
  network (broadcast, like 3dslink) and remembers the last one used (per user, outside the repository).
- **FR-5 Readable, kept logs.** Engine log lines are shown with their level coloured and the channel
  aligned; other output passes through unchanged. The whole session is also written to
  `build/logs/<game>-<date-time>.txt`. A crash report sent by the game is shown in full.
- **FR-6 Install.** `nnh install <game> [--console <ip>]` builds the `.cia` (spec 009) and installs it
  through FBI's "Receive URLs over the network" (replacing `scripts/fbi_install.py`).
- **FR-7 Local network only.** The tool talks only to private (LAN) addresses, refuses others, opens
  listening sockets only while a run or install needs them, and serves only the files it is sending.
- **FR-8 Testable.** The protocol pieces (message framing, the compressed stream the netloader expects,
  checksums, the FBI URL message, log line parsing) are unit tested; a fake netloader and a fake FBI on
  the loopback interface test a whole transfer without a console.

## Acceptance criteria

- **AC-1** `nnh run hello` and `nnh run audio` on the maintainer's Old 3DS XL: built, sent, the log shown
  coloured and saved; the session ends when the game exits (maintainer).
- **AC-2** `nnh install audio` installs the `.cia` with FBI over the network (maintainer).
- **AC-3** Unit tests for every protocol piece and log parsing; loopback tests of a full send and of the
  FBI exchange; fuzz target for the log/crash parser.
- **AC-4** Builds and passes its tests on Windows and Linux (VM 120); no new dependency.
- **AC-5** Docs: new `docs/guides/nnh-tool.md`; getting-started, debugging-on-3ds, packaging and CLAUDE.md
  use `nnh run`/`nnh install`; ROADMAP M6 first item.

## Non-goals

- The console app NNH Link, `nnh serve`, project listing, build requests from the console and pairing
  (spec 011).
- A standalone project template (`nnh new`) and using the engine outside this repository.
- Emulator integration, debugging (GDB), hot reload.
- Other consoles.

## Constraints

- Constitution III: the tool builds quickly and is not part of the game's build (games don't wait for it).
- Constitution IX: own implementation of the 3dslink-compatible protocol (documented by devkitPro's
  sources, GPL — protocol facts only, no code copied); no third-party code.
- Constitution X: LAN only; nothing listens longer than needed; files served only by exact name.

## Open questions

Answered by the maintainer (2026-11-18): the proposals, all accepted.

- [x] **Split M6** — proposal: this spec is the PC tool (`nnh build/run/install`); spec 011 adds NNH Link
  on the console, `nnh serve` (the console asks the PC for builds), pairing, and launching the received
  game through Luma's homebrew loader (`hb:ldr`, as the Homebrew Launcher does).
- [x] **Own protocol code or devkitPro's `3dslink`** — proposal: our own, small implementation: NNH Link
  needs the same code on both sides anyway, it works where `3dslink` isn't on the PATH, and the netloader
  accepts an uncompressed zlib stream (stored blocks), so no compression library is needed.
- [x] **How the tool knows the games** — proposal: `nnh_add_game()` writes a small manifest into the build
  folder (game name, outputs); `nnh` reads it, so `<game>` is the CMake target name.
- [x] **Remembered console** — proposal: the last IP per user (`%APPDATA%/nnhge/nnh.ini`,
  `~/.config/nnhge/nnh.ini`); `--console` overrides and updates it.
