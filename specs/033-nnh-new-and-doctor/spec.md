# Spec 033: `nnh new`, `nnh doctor`, `nnh --version` and the newcomer docs

Status: approved 2026-11-27 (the maintainer, every open question as recommended)
Author: NNHGE maintainers · Date: 2026-11-26
Depends on: [spec 032](../032-game-projects/spec.md) (draft: the game-project contract this spec generates: the engine as
a subproject, `nnh.toml`, the six presets in a game project, `NNHGE_VERSION`, `nnh` working from a game project),
[spec 010](../010-nnh-cli-run/spec.md) (`nnh build`/`run`/`install`, console discovery, LAN-only rule, exit codes),
[spec 013](../013-console-remote/spec.md) (`.nnhs` scripts, `--pc --script`), [spec 009](../009-packaging/spec.md)
(`TITLE`/`AUTHOR`/`CIA UNIQUE_ID`, the default icon and banner). Implemented after spec 032 (and therefore after
[spec 031](../031-hot-reload-assets/spec.md), which also changes `nnh`). Second of the three release specs proposed by the
[1.0 release study](../../docs/plans/2026-11-26-release-1.0.md) (032 game projects, 033 this spec, 034 versions and
releases); the study is a draft the maintainer has not reviewed, so its decisions are open questions below, not settled.

## Problem and goal

Starting a game with NNHGE today means cloning the engine, reading several pages, adding a folder inside the engine
repository and discovering one missing tool at a time (devkitPro not installed, Ninja missing inside devkitPro's MSYS2,
CMake without CA certificates, a compiler too old) through build errors that don't say what to do. Spec 032 makes a game
project outside the repository possible, but writing one by hand (a `CMakeLists.txt` with a hash-pinned engine, six
presets, `nnh.toml`, a first scene, a test script) is still a page of copying. After this spec a newcomer runs
`nnh doctor` and gets one line per missing or wrong tool with the exact command or link that fixes it; runs
`nnh new mygame` and gets a small, working, documented game project that builds for the simulator and the 3DS and passes
its own test script on both; and `nnh --version` says which engine release the tool belongs to. A "Your first game" guide
and a rewritten getting-started page walk the whole path.

## User stories

- As a newcomer, I want one command that tells me which tools are missing or too old and how to fix each one, so that I
  don't learn it from a failed build.
- As a newcomer on Windows, I want the fix lines to be commands I can paste, so that I don't have to know MSYS2.
- As a game developer, I want `nnh new mygame` to give me a project that builds and runs in the simulator and on my 3DS
  without editing anything, so that my first minutes are spent on my game.
- As a game developer, I want the generated project to show the usual pieces (a scene, a sprite, text on both screens, a
  sound, an assets folder, a test script, a `.cia` set up), each with a short comment, so that I learn the engine from my
  own code.
- As a game developer, I want `nnh new` never to overwrite anything and to refuse names that would escape the folder, so
  that a typo can't damage my files.
- As a game developer, I want a test script in my new project that passes on the PC and on the console, so that I start
  with a working test I can grow.
- As a game developer, I want `nnh --version` and `nnh doctor` to tell me when my `nnh` and my project's engine version
  differ, so that I understand why a game refuses to talk to the tool.
- As a game developer working on the engine, I want `nnh new` from my engine checkout to point the new project at that
  checkout, so that I can try an engine change in a fresh project.
- As a game developer on Linux, I want the same commands and the same project to work, so that Windows is not required.

## Functional requirements

### `nnh --version`

- **FR-1 Version line.** `nnh --version` prints one line and exits 0: `nnh <version>` followed by, in parentheses, the
  game↔tool protocol version it speaks, the host system and architecture it was built for, and how it was built (a
  release build or a build from a source checkout). `<version>` is the engine version `nnh` was built from (spec 032's
  `NNHGE_VERSION`; the number itself is spec 034's). Nothing is read from the network or the current folder.
- **FR-2 Help.** `nnh help` lists `new`, `doctor` and `--version` with their options, in the same style as today's
  commands; an unknown option of either new command is refused with exit 2 and a message naming it.

### `nnh new`

- **FR-3 Command.** `nnh new <name> [--title <text>] [--author <text>] [--engine <path>] [--unique-id <hex>]` creates the
  folder `<name>` in the current folder and writes a game project into it. `--title` defaults to the name, `--author`
  to an empty value the build accepts. It exits 0 and prints the folder it created and the next three commands to run
  (`cd`, `nnh run <name> --pc`, `nnh run <name>`).
- **FR-4 Safe names.** `<name>` is the game's CMake target, its folder and its file names, so it must be 1–32
  characters of lowercase ASCII letters, digits, `_` and `-`, starting with a letter. Refused, with exit 1 and a
  message saying which rule failed and suggesting a valid spelling when one is obvious (`My Game` → `my-game`):
  anything containing a path separator, `..`, a drive letter or a leading `-`/`.`; names reserved by Windows (`con`,
  `nul`, `aux`, `prn`, `com1`–`com9`, `lpt1`–`lpt9`); target names the engine or CMake reserve in a game's build (spec
  032 FR-8's list, plus `all`, `clean`, `install`, `test`, `help`, `package`). Nothing is created when a name is refused.
- **FR-5 Never overwrite.** If `<name>` exists and is anything but an empty folder (a file, a non-empty folder, a link),
  `nnh new` refuses with exit 1, names it and changes nothing. An existing empty folder is filled. Every file is created
  new (never opened for overwrite), and nothing is written outside the project folder.
- **FR-6 All or nothing.** If writing fails part way (disk full, permissions), `nnh new` removes what it created (the
  folder too, if it created it), reports the failing file and exits 1; a pre-existing empty folder is left empty.
- **FR-7 Self-contained template.** The template is embedded in `nnh`: generating a project reads no file outside
  `nnh`, needs no network and no git, and the same `nnh` always writes the same files apart from the name, title,
  author, unique ID and engine location. It runs no downloaded code and no other program.
- **FR-8 Generated files.** Exactly this file list (paths relative to the project folder):
  - `CMakeLists.txt` — CMake ≥ 3.25, `project(<name>)`, the engine brought in by spec 032's pinned download (FR-1.1 of
    032, content name `nnhge`), and one `nnh_add_game(<name> ...)` call with `SOURCES`, `ASSETS assets`, `TITLE`,
    `AUTHOR`, `VERSION 0.1.0` and `CIA UNIQUE_ID <id>`, no `ICON`/`BANNER` (the engine's defaults), and a one-line
    comment for each optional keyword it doesn't use, with a link to the page that explains it;
  - `CMakePresets.json` — the six presets of the engine repository with the same names, flavours and `build/<preset>`
    layout (spec 032 Q2), compile commands exported;
  - `nnh.toml` — `name` and `engine` (the version of this `nnh`) as spec 032 defines them;
  - `src/main.cpp` and one scene (a header and a source file);
  - `assets/art/player.png` and `assets/sfx/blip.wav`, made for the engine and under its zlib licence;
  - `tests/smoke.nnhs`;
  - `.gitignore` (the build folder, `CMakeUserPresets.json`, and the `.3dsx`, `.cia`, `.smdh`, `.elf` outputs) and
    `.gitattributes` (assets are binary);
  - `README.md` — how to build and run on PC and 3DS, how to run the test script, links to the engine's docs for the
    engine version it pins, and the licence note of FR-12;
  - `CMakeUserPresets.json` only when a local engine is used (FR-10).
  No `LICENSE` file is written (the game's licence is the author's choice).
- **FR-9 Engine location.** The generated `CMakeLists.txt` always has the same shape. Which engine it pins:
  - an `nnh` that was given an engine archive URL and SHA-256 when it was built (spec 034's release builds) writes them
    into the pinned download;
  - otherwise (an `nnh` built from a source checkout) the project uses a local engine (FR-10), and the pinned download
    is written so that configuring without a local engine stops at once with a message saying how to set one or which
    release pin to paste, instead of an unverified download (open question Q6).
- **FR-10 Local engine.** With `--engine <path>`, or by default for an `nnh` built from a source checkout (that
  checkout), `nnh new` writes `CMakeUserPresets.json` setting spec 032's local override for all six presets, in the
  path form each preset needs on this system (spec 032 FR-15). The path must be an existing engine source folder
  (checked: it contains the engine's top-level CMake project); otherwise `nnh new` refuses with exit 1 before creating
  anything. The file is git-ignored, so the path never reaches the game's repository.
- **FR-11 Unique ID.** `CIA UNIQUE_ID` is a random number in the homebrew range `0x00300`–`0xF7FFF` from the operating
  system's random source, or the value of `--unique-id` (refused outside that range). The README says to keep it for the
  life of the game ([packaging guide](../../docs/guides/packaging.md#choosing-a-unique-id)).
- **FR-12 Licence note.** The README states: the game's licence is the author's choice; NNHGE is zlib-licensed and asks
  nothing of a shipped game; which notices, if any, the engine's vendored code and the 3DS toolchain's runtime libraries
  ask of a shipped binary (settled per library in open question Q8); no Nintendo SDK, leaked code or copyrighted
  assets.

### The generated game

- **FR-13 What the game does.** On both screens the game shows text naming the game and its controls; a sprite moves
  with the Circle Pad (and the D-Pad) and stays on the top screen; A plays the sound; START quits. It logs one line when
  the scene starts, one per sound played and one when it quits, which the test script checks. It uses only public
  `<nnh/...>` headers and the engine's own font, and works on an Old 3DS without ZL, ZR or the C-Stick.
- **FR-14 Example code.** The generated sources follow the engine's rules for game code: no platform `#ifdef`, no heap
  allocation per frame, state in the scene, query for state and events for changes (constitution I, IV, V); each file
  is short (well under the aim of constitution VII) and every block has a one-line comment pointing to the API page it
  uses. They compile without warnings at the engine's warning level on every checked compiler.
- **FR-15 Builds out of the box.** In a fresh project, with the tools `nnh doctor` reports as ready: `nnh build <name>
  --pc`, `nnh build <name>` (3DS, Development), `nnh build <name> --release` and `nnh install <name>` (the `.cia`) succeed
  from the project folder with no file edited, on Windows and Linux.
- **FR-16 Smoke script.** `tests/smoke.nnhs` checks that the scene started, taps A and expects the sound's log line,
  moves the Circle Pad and checks the sprite moved (a log line or a screenshot plus a line), takes one screenshot, fails
  on any error line, and ends by pressing START and expecting the quit line. `nnh run <name> --pc --script
  tests/smoke.nnhs` and `nnh run <name> --script tests/smoke.nnhs` (console) both exit 0. The script uses no control an
  Old 3DS lacks.
- **FR-17 Build budgets.** Editing the generated scene's source and rebuilding stays within constitution III's budgets
  (simulator < 2 s, `.3dsx` < 5 s) on the maintainer's machine.

### `nnh doctor`

- **FR-18 Command.** `nnh doctor [--online] [--console <ip>] [--pc]` checks the machine (and the game project, when run
  inside one) and prints a report. It only reads: it installs, downloads, configures and writes nothing (not even
  `nnh.ini`), and without `--online` or `--console` it opens no network connection. Each external program it runs has a
  time limit; an offline run finishes in a few seconds.
- **FR-19 Checks.** Each check is `ok`, `warn` or `FAIL`; `FAIL` means a build or run that needs it will fail, `warn`
  means it works with a limit or a risk. Grouped as:
  - **About**: the `nnh` version line (FR-1) and the operating system; macOS and MSVC are reported as `warn` ("best
    effort, not checked by the project", open question Q9);
  - **Simulator**: CMake present and ≥ 3.25; Ninja present; a C++20 host compiler (`g++`, `clang++` or `cl`) with its
    version, `FAIL` below the minimum the engine compiles with, `warn` for versions the project doesn't check (Q10);
    SDL3: a system SDL3 found, or the prebuilt or source build the first configure will download; on Linux without a
    system SDL3, `pkg-config` and the SDL build dependencies the source build needs;
  - **3DS**: `DEVKITPRO` set (Windows default `C:/devkitPro`, Linux `/opt/devkitpro`); devkitPro's CMake toolchain file
    for the 3DS; devkitARM, libctru, citro2d and citro3d present, with versions when they can be read; on Windows,
    devkitPro's MSYS2 shell (or `DEVKITPRO_MSYS2`), and CMake and Ninja reachable inside it; a host C++ compiler `nnh`
    can pass to 3DS configures (spec 032 FR-14);
  - **Optional** (never `FAIL`): ccache (faster rebuilds), git, the Azahar emulator;
  - **Console** (only with `--online` or `--console <ip>`): a console found on the LAN the way `nnh run` finds it, or
    "not found: open the Homebrew Launcher and press Y, then try again"; LAN addresses only (spec 010 FR-7);
  - **Downloads** (only with `--online`): the pinned downloads a first configure needs are reachable through the same
    CMake the build uses (the SDL3 package on PC; inside a project, its engine archive URL when it pins one), which
    catches a CMake without CA certificates; only those pinned URLs are contacted, and they are listed;
  - **This project** (inside a game project or the engine repository): `nnh.toml` parses; the engine version the
    project builds with (from an existing configure if there is one, else `nnh.toml`'s `engine`) equals this `nnh`'s
    version, else `warn` with the fix (the game↔tool protocol must match); free disk space in the project folder
    (thresholds: Q11); the presets `nnh` needs exist.
  The Python interpreter is not checked: no build step uses it (it is only used by the engine repository's docs tool).
- **FR-20 Report format.** One line per check: the state, the check's name and what was found (version, path). Every
  `warn` and `FAIL` has a following line `fix: <one command or one link>`: a command the user can paste in the shell
  they most likely use (on Windows, a form that works in PowerShell, cmd and Git Bash alike: forward-slash paths and
  double quotes), or a link to the official page when installing needs an installer (devkitPro, WinLibs, Azahar). After
  the groups, one verdict line per group (`Simulator: ready`, `3DS: not ready (2 problems)`, `Console: not checked`, …).
  Colours follow `nnh run`'s rules (`NO_COLOR`); the text is complete without them.
- **FR-21 Exit codes.** 0 when no check of a required group failed; 1 when one did; 2 for a bad command line. Required
  groups are Simulator, 3DS, This project, and Console/Downloads when asked for; `--pc` makes the 3DS group
  informational (its `FAIL`s shown, not counted) for developers who only use the simulator (Q12). `warn` never changes
  the exit code.
- **FR-22 Versions on a protocol refusal.** When `nnh run` (or `nnh serve`) meets a game that speaks another protocol
  version, its message names this `nnh`'s version and the project's engine version (as found by FR-19) and the fix (use
  the `nnh` of the same release as the engine, or change the pin).
- **FR-23 Same on every host.** `nnh new` and `nnh doctor` behave the same on Windows and Linux (Windows-only checks
  skipped on Linux and the reverse); on macOS they run and the report says best effort.

### Docs

- **FR-24 Newcomer path.** A new guide **Your first game** walks the whole path with `nnh`: install the tools,
  `nnh doctor`, `nnh new`, run in the simulator, run on the console, run the smoke script, change the scene (with the
  rebuild budgets), add an asset, build and install the `.cia`, where to go next. **Getting started** is rewritten around
  `nnh doctor` and `nnh new` (tools per system, then the guide); how to work inside the engine repository moves to a
  contributor section. The `nnh` tool guide documents `new`, `doctor` and `--version` (options, report format, exit
  codes); the README's quick start shows the same commands. Until spec 034 ships prebuilt `nnh` binaries, the docs say
  how to build `nnh` from an engine checkout (the pinned-download path then follows FR-9's checkout case).

## Acceptance criteria

- **AC-1** `nnh_tool_tests` cases for `nnh new`: valid names; each refusal rule of FR-4 (a separator, `..`, a drive
  letter, a leading `-`/`.`, upper case, 33 characters, `con`, `dist`, `all`) with nothing created; an existing file, a
  non-empty folder and a link refused with the folder's listing (names, sizes, times) unchanged; an existing empty
  folder filled; the written file list equals FR-8's exactly; `nnh.toml` content; `UNIQUE_ID` within range over many
  runs and `--unique-id` outside it refused; a simulated write failure leaves nothing behind; `--engine` to a folder
  that is not an engine refused; two runs with the same inputs give byte-identical files. (FR-3 to FR-11)
- **AC-2** End-to-end test `nnh_new_smoke` (Windows, and Linux on VM 120): in an empty temporary folder, `nnh new demo
  --engine <checkout>` → `nnh build demo --pc` → `nnh run demo --pc --no-build --script tests/smoke.nnhs` exits 0
  headless; the build log of the game's own sources has no warning. (FR-8, FR-10, FR-13 to FR-16)
- **AC-3** From the generated project, on Windows (devkitPro's MSYS2, no `scripts/dkp.sh` run) and on VM 120
  (`/opt/devkitpro`): `nnh build demo`, `nnh build demo --release` produce the `.3dsx`; the `dist` target produces the
  `.cia`. (FR-15)
- **AC-4** On the maintainer's Old 3DS XL: `nnh run demo --script tests/smoke.nnhs` → PASS after the maintainer presses
  Y; `nnh install demo` installs the `.cia` through FBI and the game starts from the HOME Menu with the default icon and
  banner. (FR-15, FR-16)
- **AC-5** Budgets, measured and recorded in the ROADMAP table: one edit to the generated scene's source rebuilds in
  < 2 s (simulator) and < 5 s (`.3dsx`). (FR-17)
- **AC-6** `doctor` rule tests in `nnh_tool_tests` with faked tool outputs and a faked file system: CMake 3.20 → `FAIL`
  + fix; Ninja missing → `FAIL` + fix; `DEVKITPRO` unset → 3DS `FAIL` + the installer link; devkitPro's MSYS2 without
  Ninja → `FAIL` + the paste-able `pacman` line; no host compiler for 3DS → `FAIL` + fix; an untested compiler version →
  `warn`; engine version ≠ `nnh` version → `warn`; macOS → `warn`. Exit codes 0/1/2 including `--pc`. Every rule
  mutated (minimum raised, check inverted) turns at least one test red. Version parsers tested on recorded outputs of
  the real tools (CMake 3.25 and 4.x, Ninja, GCC, clang, MSVC `cl`). (FR-19 to FR-21)
- **AC-7** Real runs: on the maintainer's machine `nnh doctor` inside the generated project prints every required
  check `ok` and exits 0; with `DEVKITPRO_MSYS2` pointing to an MSYS2 without Ninja (or devkitPro hidden) it prints the
  `FAIL` and its fix line and exits 1, and the Simulator group still says ready; on VM 120 with `DEVKITPRO` unset the
  same, and with it set every check `ok`. The reports are pasted into the spec's verification notes. (FR-19, FR-20,
  FR-21, FR-23)
- **AC-8** Offline and bounded: with the network disabled (or a fake network layer that records attempts), `nnh doctor`
  makes no connection and finishes within its time limit; `nnh doctor --online` lists the URLs it contacted, and they
  are exactly the pinned ones; `--console 8.8.8.8` is refused as not a LAN address. Running `doctor` changes no file
  (listing of the user config folder and the project folder unchanged). (FR-18, FR-19)
- **AC-9** `nnh --version` prints the line of FR-1 and exits 0 on Windows and Linux; `nnh help` lists the new commands;
  an unknown option exits 2. A game built from an older engine run with `nnh run` gives the FR-22 message (tested with a
  fake game answering another protocol version). (FR-1, FR-2, FR-22)
- **AC-10** Docs: `docs/guides/your-first-game.md` new; `getting-started.md`, `nnh-tool.md` and the README quick start
  updated; decision row D56; `python -I scripts/docs_tool.py check` reports 0 broken links. Following "Your first game"
  verbatim in an empty folder on Windows and on VM 120 reaches the simulator window and a passing smoke script. (FR-24)

## Non-goals

- **`nnh setup`** or any command that installs, updates or repackages tools. devkitPro's trademark policy forbids
  calling a repackaged or modified toolchain devkitPro, and its installer notes ask third parties not to mix their own
  downloads into its install ([study, decision 4](../../docs/plans/2026-11-26-release-1.0.md)); a `setup` would also run
  privileged, OS-specific installers whose failures `doctor` would have to diagnose anyway. `doctor` points to the
  official installers and `dkp-pacman` instead. Reconsidered after the newcomer test (study step 7) only for the one
  safe step (Ninja inside devkitPro's MSYS2), if it proves to be the usual stumble.
- The version number 1.0.0, `<nnh/version.hpp>`, the stability promise, `CHANGELOG.md`, release archives, prebuilt `nnh`
  binaries and the build-time injection of the engine archive's URL and SHA-256 into them: spec 034. This spec uses
  whatever version and pin `nnh` was built with.
- More than one template, template options (2D/3D, with or without UI), `nnh new` into an existing project
  (`init`), upgrading a generated project to a newer engine, `git init` of the new project (Q5).
- Machine-readable `doctor` output (JSON) and a `doctor --fix`.
- Checking New 3DS hardware, emulators beyond "Azahar found", or the console's firmware.
- macOS and MSVC as checked hosts (best effort, reported as such).

## Constraints

- **I Same game code everywhere.** The generated game is the same source on PC and 3DS with no platform switch, and its
  one test script passes on both (FR-13, FR-16).
- **II The 3DS is the constraint.** The generated game runs at 60 Hz on an Old 3DS and uses only controls it has.
- **III Build speed.** The generated project's one-file edit budgets are measured (AC-5). The end-to-end test adds an
  engine build to whichever test run includes it (Q7).
- **IV / V.** The template is code newcomers copy: scene-based, events for changes, no per-frame heap, no exceptions
  (FR-14).
- **VI Spec first, test first.** `nnh new` and `doctor` rules are unit-tested first with fakes (AC-1, AC-6); the template
  is proven by its own script on PC and on the console (AC-2, AC-4).
- **VII Small, organized files.** The template files and the new `nnh` commands keep the file-size rules; the
  template's assets are small.
- **VIII Documented API.** `nnh new`, `nnh doctor`, `nnh --version`, their options and exit codes are public surface
  (study decision 7 lists `nnh` commands, options and exit codes as stable in 1.x): documented with the change (FR-24).
- **IX Clean and legal.** The template's art and sound are made for the engine under zlib; the licence note is checked
  against each library's licence (Q8); no devkitPro files are shipped or fetched by `nnh`.
- **X Safe by default.** `nnh new` validates names, never overwrites, writes only inside the new folder and runs nothing
  downloaded; `doctor` only reads, contacts nothing without `--online`/`--console`, and then only the pinned URLs and
  LAN addresses; the generated project pins the engine by SHA-256. `doctor` reads tool outputs and `nnh.toml` with
  bounded parsers.
- **XI Opt-in.** `nnh new` is one way to start; a hand-written project (spec 032) and a game inside the engine
  repository stay supported. The template shows plain scenes and core APIs, not optional modules.
- One implementation at a time, after spec 032 is merged; ≥ 10 GB free on the maintainer's `E:` before the end-to-end
  builds.

## Prior art

- `flutter doctor` reports each part of the toolchain with a pass/warn/fail mark and an indented fix hint, grouped by
  component ([Flutter CLI reference](https://docs.flutter.dev/reference/flutter-cli)); FR-20 follows that shape with one
  copyable fix line per problem.
- `cargo new <path>` writes a manifest, a sample source and a VCS ignore file and is the model for a tiny, working
  template ([cargo new](https://doc.rust-lang.org/cargo/commands/cargo-new.html)); unlike Cargo, `nnh new` takes a name,
  not a path, and does not run `git init` by default (Q4, Q5).
- devkitPro trademark policy: <https://devkitpro.org/wiki/Trademarks> (quoted by the release study; the page answers 403
  from the maintainer's network, so it was not re-read for this spec); installer notes:
  <https://github.com/devkitPro/installer/releases>.

## Open questions

Answered 2026-11-27: the maintainer approved every recommendation below.

The first four come from the [release study](../../docs/plans/2026-11-26-release-1.0.md) (pending the maintainer's
review); the rest came up while writing this spec.

- [x] **Q1 How the generated project consumes the engine** (study decision 1; spec 032 Q1). — *Recommendation (study):
  `FetchContent` of the release source archive by URL + SHA-256, with the local override in `CMakeUserPresets.json`.
  Submodule and vendored copy stay documented by hand (spec 032), not as `nnh new` options.*
- [x] **Q2 Presets and marker in the generated project** (study decisions 2 and 3; spec 032 Q2, Q3). —
  *Recommendation (study): a full `CMakePresets.json` with the six names; `nnh.toml` with `name` and `engine`.*
- [x] **Q3 `doctor` only, no `nnh setup` in 1.0** (study decision 4). — *Recommendation (study): yes, for the reasons in
  Non-goals; revisit only the Ninja-in-MSYS2 step after the newcomer test.*
- [x] **Q4 Name or path argument.** `nnh new <name>` (folder = name, in the current folder) or `nnh new <path>` like
  Cargo. — *Recommendation: name only for 1.0. It makes FR-4's traversal rule trivial to state and test; `cd` first to
  choose the parent folder.*
- [x] **Q5 `git init`.** — *Recommendation: no. `nnh` stays free of git (spec 032 FR-3), and the README's first lines
  show `git init` for those who want it. The `.gitignore` is written either way.*
- [x] **Q6 An `nnh` built from a checkout, without `--engine`** (FR-9). Options: a) use that checkout as the local engine
  (the source path is known at build time) and write the pinned download with a "no pin known" guard that stops a
  configure without the override; b) refuse without `--engine`; c) write the URL of the release matching its version and
  leave the hash empty. — *Recommendation: a. It is what an engine contributor wants, and the guard never lets an
  unverified download happen; c would trust an unpinned download (constitution X); b adds a step for no gain.*
- [x] **Q7 Cost of `nnh_new_smoke` in `ctest`.** The end-to-end test configures one more engine build. —
  *Recommendation: as spec 032 Q4: the default `ctest` compiles the template's sources and assets inside the engine's own
  build (cheap, catches a template that no longer compiles) and runs `nnh new` + checks the file list; the full
  end-to-end runs under spec 032's `consumer-full` label, used by VM 120 runs and the release checklist.*
- [x] **Q8 Licence note for shipped games** (FR-12). The study says NNHGE and its vendored code need no notice in a
  shipped game, "to confirm per library". Facts to settle in the plan: NNHGE, SDL3, libctru, citro2d and citro3d are
  zlib (no notice in binaries); the stb libraries are dual MIT / public domain (the public-domain option needs no
  notice); Ryu (BSL-1.0, only in Debug/Development games that use live values) needs none for object code; the 3DS
  toolchain's runtime (newlib's C library, libstdc++ under the GCC Runtime Library Exception) is the uncertain part:
  some newlib files carry BSD-style licences that may ask for a notice in binary distributions. —
  *Recommendation: the README states the clear cases, says that a credits line for devkitPro/newlib is a good habit,
  and links to the licences; it does not claim "no notice needed" for the toolchain runtime until checked per file.*
- [x] **Q9 Best-effort hosts in `doctor`.** — *Recommendation (study decision 10): macOS and MSVC give `warn`, never
  `FAIL`, with "best effort, not checked by the project".*
- [x] **Q10 Compiler minimums.** — *Recommendation: `FAIL` below the oldest GCC/clang/MSVC that compile the engine's
  C++20 subset (found in the plan by trying), `warn` between that and the checked versions (GCC 13/14, clang 18,
  WinLibs GCC 16); the numbers live in one table in `nnh-tool.md`.*
- [x] **Q11 Disk space thresholds.** — *Recommendation: measure a fresh project's PC + 3DS builds in the plan; `warn`
  below about three times that, `FAIL` below one build's worth.*
- [x] **Q12 Does a missing devkitPro fail `doctor`?** A simulator-only developer would get exit 1. —
  *Recommendation: yes by default (the engine's promise is PC **and** 3DS), with `--pc` to check only what the simulator
  needs (FR-21); the verdict lines make the split visible either way.*
- [x] **Q13 Smoke script and the generated scene's log lines.** Should the template's log lines and screenshots be part
  of the stable `nnh new` output (so docs can quote them)? — *Recommendation: no; they are example code the developer is
  expected to change. Only the file list, the `nnh.toml` keys and the commands are documented as stable.*
- [x] **Q14 Decision row.** — *Recommendation: D56 records the embedded template, the name rules, the engine-location
  rule of FR-9/Q6, the `doctor` groups, report format and exit codes, and "no `nnh setup`".*
