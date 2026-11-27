# Plan 033: `nnh new`, `nnh doctor`, `nnh --version` and the newcomer docs

Status: approved 2026-11-27 (the maintainer, the ten questions at the end as recommended; question 4 authorises `apt install` of older compilers on VM 120)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decision D56 (proposed,
[decisions.md](../../docs/architecture/decisions.md))
Uses spec 032 and D55 (the game-project contract: the engine as a subproject, `nnh.toml`, the six presets,
`NNHGE_VERSION*`, `NNHGE_ENGINE_DIR`, `nnh`'s project root, preset check and 3DS configure command), spec 010 and D23
(`nnh build`/`run`/`install`, console discovery, the LAN-only rule, exit codes), spec 013 and D28 (`.nnhs` scripts, the
remote protocol's Hello and its version), spec 011 (`nnh serve`, NNH Link's protocol version), spec 009 (`TITLE`,
`AUTHOR`, `CIA UNIQUE_ID`, the default icon and banner).
Built in one worktree (branch `spec-033`) created from `main` after spec 032's merge (`24f03e4`); no other spec is built
in parallel. Spec 034 builds on two seams this plan creates: the build-time engine pin of a release `nnh`
(`NNH_RELEASE_ENGINE_URL`, `NNH_RELEASE_ENGINE_SHA256`) and `nnh --version`'s build kind.

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | The generated game is one source for PC and 3DS with no platform switch; its one smoke script passes on both (FR-13, FR-16). `nnh new` and `doctor` behave the same on Windows and Linux; system-specific code stays inside `nnh` (a tool, not game code) |
| II 3DS constraint | ✅ | The template is `hello`-sized plus one 32×32 texture and a 3 KB sound: far inside Old 3DS limits; 60 Hz checked by the console run (AC-4); Old 3DS controls only |
| III Build speed | ⚠️→✅ | Pressure points: (a) the template's one-file edit budgets in a game project (spec 032 measured a `hello`-sized project at a median 1.77 s / 3.6 s: AC-5 re-measures with the template); (b) the end-to-end test adds an engine build to the runs that include it: label `consumer-full` (Q7 as approved); the default `ctest` only compiles the template's three sources inside the engine's own build and runs its smoke script (≈ 10 s); (c) `nnh` grows by one generated source (the embedded template, ≈ 15 KB): measured |
| IV Event-driven | ✅ | The template is scene-based: state in the scene, queries for input, the engine's `SoundFinished` event for a change (FR-14) |
| V No hidden costs | ✅ | No engine code changes. The template allocates nothing per frame (checked by reading and by the Development build's allocation counters in the smoke run) |
| VI Spec + test first | ✅ | Name rules, rendering, writing and rollback, the engine-location rule, version parsers, every `doctor` rule and the report are pure functions over fakes, tested first in `nnh_tool_tests` (AC-1, AC-6); the template is proven by its own script in the simulator (default `ctest`), end to end from a generated project (AC-2, AC-3) and on the console (AC-4); hand mutants at the end |
| VII Small files | ✅ | Two new folders in `nnh` (`src/new/`, `src/doctor/`), one concept per file, each ≤ 200 lines planned; `script_runner.cpp` (389 lines) gains no message text: FR-22's text lives in its own file |
| VIII Documented API | ✅ | `nnh new`, `nnh doctor`, `nnh --version`, their options, exit codes and report format: `nnh-tool.md`, the new "Your first game" guide, a rewritten getting-started, the README quick start, D56 (FR-24) |
| IX Clean and legal | ✅ | The template's PNG and WAV are engine-made (provenance recorded); FR-12's licence note settled per library below; no devkitPro file is shipped or fetched by `nnh` |
| X Safe by default | ⚠️→✅ | Pressure points: (a) `nnh new` writes files: names validated, exclusive creates only, nothing outside the new folder, rollback on failure; (b) `doctor` runs external programs: no shell for host tools, fixed argument lists, time limits, bounded output, bounded parsers (mini-fuzzed); (c) network: none without `--online`/`--console`, then only the pinned URLs (listed) and LAN addresses; (d) the generated project never downloads unverified code (the no-pin guard) |
| XI Opt-in, never in the way | ✅ | `nnh new` is one way to start; hand-written projects (spec 032) and games inside the repository keep working unchanged; the template uses core APIs only |

## Design

### Overview

```
nnh (one binary, built from the engine's tools/nnh)
  build_info      version, remote and link protocol versions, host, build kind; the release pin (034) or the checkout
  new             name rules → engine location → render the embedded template → write all or nothing
  doctor          probes (env, files, programs with time limits) → pure checks per group → report → exit code
  run/serve       FR-22: a protocol mismatch names this nnh's version and the project's engine version

tools/nnh/template/   the template as real files (C++ placeholder-free, compiled and smoke-tested by the engine's ctest)
      │  embed (CMake script at build time, LF-normalised)
      ▼
nnh new mygame  →  mygame/ CMakeLists.txt  CMakePresets.json  nnh.toml  README.md  .gitignore  .gitattributes
                           src/main.cpp  src/scenes/play_scene.{hpp,cpp}  assets/art/player.png  assets/sfx/blip.wav
                           tests/smoke.nnhs
                   engine: release nnh → its archive URL + SHA-256; checkout nnh → no pin + guard, NNHGE_ENGINE_DIR
```

### Decision D56: `nnh new` writes an embedded template, all or nothing; `nnh doctor` is a read-only, grouped, offline-by-default checker

Proposed for review (row in [decisions.md](../../docs/architecture/decisions.md)):

1. **Embedded template.** The template lives in the engine repository as real files under `tools/nnh/template/`; the
   build embeds them (LF line endings, a fixed order) into one generated source of `nnh`. Placeholders (`@name@`, …)
   appear only in the non-C++ files (`CMakeLists.txt.in`, `nnh.toml.in`, `README.md.in`); the C++ sources and the smoke
   script are placeholder-free (the game shows its title through `NNH_GAME_TITLE`, which `nnh_add_game()` already
   defines), so the engine's own PC build compiles them as a check game and runs the smoke script (`ctest`). Dotfiles are
   stored without the dot (`gitignore`, `gitattributes`) so they never act on the engine repository.
2. **Name rules** (FR-4): `^[a-z][a-z0-9_-]{0,31}$`, no Windows device name, no CMake or engine reserved target name, no
   name starting with `nnh_`/`nnh-` or equal to `nnh` (the engine's namespace); a refusal names the rule and suggests a
   spelling when one is obvious.
3. **Engine location rule** (FR-9, FR-10 as amended): a release `nnh` (built with `NNH_RELEASE_ENGINE_URL` and
   `NNH_RELEASE_ENGINE_SHA256`, spec 034) writes them as the project's pin; any other `nnh` writes the same file shape
   with both values empty and a guard that stops a configure without `NNHGE_ENGINE_DIR` (never an unverified download),
   and points the user at `NNHGE_ENGINE_DIR` (its own checkout by default, or `--engine`). No file ever holds the local
   path. "Release build" means exactly "has a pin".
4. **Writing** (FR-5, FR-6): the target is a new folder or an existing empty one (no link); every file is created with
   an exclusive create (fails if anything appeared meanwhile); on any failure everything `nnh new` created is removed in
   reverse order (the folder too if it made it).
5. **`doctor`** (FR-18 to FR-21): probes (environment, file system, programs run without a shell with a 10 s limit and
   64 KiB of output) feed pure checks; groups About, Simulator, 3DS, Optional, Console, Downloads, This project; states
   `ok`/`warn`/`FAIL`, a `fix:` line under each `warn`/`FAIL`, one verdict line per group; exit 0/1/2, `--pc` makes the
   3DS group informational; minimums and thresholds in one table (`doctor/doctor_limits.hpp`, mirrored in
   `nnh-tool.md`). It writes nothing in the project or the user's config; `--online` runs CMake on one temporary script
   in the system's temporary folder (question 3).
6. **Version line** (FR-1): `nnh <version> (protocol <remote>, link <link>, <os>-<arch>, <release build | from a source
   checkout>)`; `<version>` is `NNHGE_VERSION` at `nnh`'s configure, compiled into one source file only.
7. **Protocol mismatch** (FR-22): one message builder used by `nnh run` (script and typed modes, PC and console) and by
   `nnh serve`'s NNH Link refusal, naming both versions and the fix.
8. **No `nnh setup`** (spec non-goal, Q3): `doctor` points to official installers and package managers.

Why: the template as real files is compiled, warned and smoke-tested by the engine's own build, so a template that no
longer compiles fails `ctest` long before a release; embedding keeps FR-7 (no file outside `nnh`, same bytes per `nnh`).
The pin rule makes the checkout case safe without a placeholder URL. A pure checker over fake probes makes every rule
testable and mutable (AC-6). One message builder keeps FR-22's wording in one place.

### Public surface (sketch)

```text
$ nnh --version
nnh 0.1.0 (protocol 1, link 1, windows-x64, from a source checkout)

$ nnh new my-game --title "My Game" --author "Me"
nnh: created my-game/ (12 files; CIA UNIQUE_ID 0x5A3C1)
nnh: this project uses the local engine E:/src/nnhge (no engine release is pinned: this nnh was built from a checkout).
     Builds need NNHGE_ENGINE_DIR; set it in your shell:
       Git Bash:    export NNHGE_ENGINE_DIR=E:/src/nnhge
       PowerShell:  $env:NNHGE_ENGINE_DIR = "E:/src/nnhge"
       cmd:         set NNHGE_ENGINE_DIR=E:/src/nnhge
       every new shell (Windows): setx NNHGE_ENGINE_DIR E:/src/nnhge
next:
  cd my-game
  nnh run my-game --pc
  nnh run my-game              (Homebrew Launcher open, press Y)

$ nnh new "My Game"
nnh: "My Game" can't be a game name: only lowercase letters, digits, '_' and '-' (try my-game)      [exit 1]

$ nnh doctor [--online] [--console <ip>] [--pc]
```

Options: `--title <text>` (default: the name; ≤ 64 characters of valid UTF-8, no control characters and none of
`" \ $ ; @ `` ` ``, which CMake or the README would read), `--author <text>` (same rules; default empty: the `AUTHOR`
keyword is then written as a commented line, because `AUTHOR ""` warns under CMake 4, CMP0174, measured 2026-11-27),
`--engine <path>`, `--unique-id <hex>` (`0x00300`–`0xF7FFF`). Unknown options exit 2; invalid values exit 1.

The generated `CMakeLists.txt` (checkout case; a release `nnh` fills the two values):

```cmake
cmake_minimum_required(VERSION 3.25)
project(my-game LANGUAGES C CXX)

# The engine, pinned by URL and SHA-256 (docs/guides/game-projects.md). To upgrade, paste the two lines of a newer
# release's notes. NNHGE_ENGINE_DIR (an engine checkout) replaces the download: see README.md, "The engine".
set(nnhge_url "")
set(nnhge_sha256 "")
if(nnhge_url STREQUAL "" AND NOT FETCHCONTENT_SOURCE_DIR_NNHGE)
    message(FATAL_ERROR "my-game: no engine release is pinned: set NNHGE_ENGINE_DIR to an engine checkout, "
                        "or paste a release's URL and SHA-256 above (README.md, \"The engine\")")
endif()
include(FetchContent)
FetchContent_Declare(nnhge URL "${nnhge_url}" URL_HASH SHA256=${nnhge_sha256} DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nnhge)
if(NNHGE_VERSION VERSION_LESS 0.1)
    message(FATAL_ERROR "my-game needs NNHGE 0.1 or later (found \"${NNHGE_VERSION}\")")
endif()

nnh_add_game(my-game
    SOURCES src/main.cpp src/scenes/play_scene.cpp
    ASSETS assets                      # images, sounds, maps, data: docs/api/assets.md
    TITLE "My Game"
    AUTHOR "Me"                        # (or: # AUTHOR "Your name" when --author was not given)
    VERSION 0.1.0
    # ICON icon.png                    # 48x48 HOME Menu icon (default: the NNHGE icon): docs/guides/packaging.md
    # IMAGES / DATA_SCHEMA / LANGUAGES # embedded images, data assets, translations: docs/tech/cmake.md
    CIA UNIQUE_ID 0x5A3C1)             # keep this number for the life of the game: docs/guides/packaging.md
```

Measured 2026-11-27 (temporary folder): with `FETCHCONTENT_SOURCE_DIR_NNHGE` set, `FetchContent_Declare` with an empty
`URL` and an empty `URL_HASH SHA256=` configures without error in native CMake 4.4.2 and devkitPro's MSYS2 CMake 4.0.2
(the override skips the download step); without it the guard stops first. CMake 3.25 (the minimum) and Ubuntu's 3.28 are
checked at T0.2.

### Before spec 034: no release exists yet

Until 034 ships prebuilt `nnh` binaries with a pin, every `nnh` is a checkout build, so every generated project is the
checkout case: empty pin, the guard, `NNHGE_ENGINE_DIR`. That is safe (no unverified download) and works for the only
people who can run `nnh` today (they built it from a checkout). The docs say so (FR-24): "build `nnh` from an engine
checkout, set `NNHGE_ENGINE_DIR` to it". The pinned path is still exercised before 034:
- unit tests render the template with a fake pin (URL, SHA-256, `blob/vX.Y.Z` docs links) and without;
- `nnh_new_smoke`'s pinned way (label `consumer-full`) packs the engine with spec 032's `pack_engine.cmake`
  (`file://` URL + SHA-256), pastes the two values into the generated `CMakeLists.txt` exactly as the guard message tells
  a user to, unsets `NNHGE_ENGINE_DIR` and builds: the download path a release project takes;
- the configure-time check of the two cache entries (both or neither; `https://` or `file://`; 64 hex digits) fails a
  wrong pin at `nnh`'s configure (tested by a scratch configure in `ctest`).
Spec 034 only sets the two cache entries in its release build of `nnh`.

### The local engine (FR-10, amended to D55)

D55 item 8 made `NNHGE_ENGINE_DIR` the local engine because CMake refuses a `CMakeUserPresets.json` preset named like
one of `CMakePresets.json`. FR-10 is rewritten in the spec accordingly (pending question 1). `nnh new`:
- picks the engine folder: `--engine <path>` (MSYS forms like `/e/x` converted with `native_path`), else, for a
  checkout `nnh`, the checkout it was built from (`NNH_TOOL_ENGINE_DIR`, compiled in only when there is no pin), else
  none (a release `nnh` without `--engine`: the pin alone);
- checks it before creating anything: a folder with `CMakeLists.txt` containing `project(nnhge VERSION <x.y.z>` and
  `cmake/nnh_game.cmake`; otherwise exit 1 (`nnh: --engine E:/x is not an NNHGE engine folder (no project(nnhge) in
  its CMakeLists.txt)`); a checkout that moved since `nnh` was built gets the same refusal with "pass --engine";
- prints the set-lines above unless `NNHGE_ENGINE_DIR` already names the same folder (compared after normalising case
  and separators on Windows); if it names another folder: `nnh: NNHGE_ENGINE_DIR is set to E:/other: builds of this
  project will use that engine`;
- writes no path anywhere and never changes the environment (`setx` is only printed).
Linux prints `export …` and "add it to ~/.profile for every new shell". `doctor` reports the variable (This project
group) and warns when it overrides a project's pin (D55 risk 3: it applies to every project on the machine).

Alternatives (question 1): (b) a git-ignored `nnh.local.toml` (`engine_dir = "…"`) that `nnh` reads and exports to the
CMake it runs (per project, works without a shell setting, but invisible to plain `cmake --preset` and a second
mechanism next to D55's); (c) `nnh build`/`run` falling back to their own checkout when the variable is unset (implicit:
a hand-written project would silently build against whatever checkout built `nnh`).

### The template

Stored in `tools/nnh/template/`, written as:

| Stored as | Written as | Notes |
|---|---|---|
| `CMakeLists.txt.in` | `CMakeLists.txt` | the sketch above; placeholders `@name@`, `@title@`, `@author_line@`, `@unique_id@`, `@engine_url@`, `@engine_sha256@`, `@engine_min@` |
| `CMakePresets.json` | `CMakePresets.json` | byte-equal to `tests/consumer/CMakePresets.json` (a unit test compares them; the guide shows the same) |
| `nnh.toml.in` | `nnh.toml` | `name = "@name@"`, `engine = "@engine_version@"` (this `nnh`'s version), one comment line |
| `README.md.in` | `README.md` | build/run on PC and 3DS, the smoke script, `.cia`, "The engine" (the pin, upgrading, `NNHGE_ENGINE_DIR`), `git init` as an option, links `@docs_base@` (`…/blob/v<version>/docs` for a release, `…/blob/main/docs` and "or `docs/` of your checkout" otherwise), the unique ID note, the licence note (FR-12) |
| `gitignore` | `.gitignore` | `build/`, `CMakeUserPresets.json`, `*.3dsx`, `*.cia`, `*.smdh`, `*.elf` |
| `gitattributes` | `.gitattributes` | `* text=auto`, `*.png binary`, `*.wav binary`, `*.ogg binary`, `*.ttf binary`, `*.otf binary`, `*.glb binary`, `*.bin binary` |
| `src/main.cpp` | same | `nnh::init({})` (title from `NNH_GAME_TITLE`), `nnh::run(play)`, `nnh::shutdown()`: ≈ 20 lines |
| `src/scenes/play_scene.hpp`, `.cpp` | same | the scene below: ≈ 40 + 110 lines |
| `assets/art/player.png` | same | 32×32 RGBA, engine-made (drawn for the template by a committed script or copied from an engine-made example asset; provenance in `tools/nnh/template/README.md`, not embedded) |
| `assets/sfx/blip.wav` | same | the engine-made `tests/data` blip (3.2 KB, the consumer fixture's) |
| `tests/smoke.nnhs` | same | below |

The scene (FR-13, FR-14): `on_enter` loads the texture and the sound (`NNH_ASSERT`-free: a failed load logs an error,
which the script's `fail-on "] E "` catches) and logs `play: started`; `update` reads the Circle Pad (dead zone) or the
D-Pad, moves the player 2 px per frame clamped to the top screen, logs `play: stopped at (x, y)` once each time the
player comes to rest after moving, plays the blip on A and logs `play: blip`, subscribes to `SoundFinished` to log
`play: blip finished` (the event example), and on START logs `play: quit`, shows "Bye!" for 30 frames, then pops itself
(the empty stack ends `nnh::run`); `draw` draws the player, the title (`NNH_GAME_TITLE`) and the controls on the top
screen and a short help text on the bottom screen with `gfx::draw_text`. No heap after `on_enter`, no `#ifdef`, every
block with a one-line comment naming its API page.

The smoke script (FR-16):

```
fail-on "] E "
expect "play: started" 60
tap A
expect "play: blip" 10
stick circle 1 0
wait 30
stick circle 0 0
expect "play: stopped at (" 30
screenshot moved
tap START
expect "play: quit" 10
end
```

The 30 frames between `play: quit` and the exit let `end` pass on the next tick before the game closes (spec 032's
console run found that a script whose game closes before `end` fails); `nnh` then sends its Bye and closes it.

### `nnh new` internals

- `new/project_name`: `check_name(text) → {ok, rule, suggestion}`; `check_text_field(text, limit)` for title/author;
  `parse_unique_id(text)`; pure.
- `new/engine_location`: `EngineChoice choose_engine(const BuildInfo&, const std::string& engine_option, const char*
  env_value, IsEngineFolder)` → {pin or none, local folder, set-lines to print, note}; `engine_folder_version(dir)`;
  `env_lines(folder, windows)`; pure apart from the folder check passed in.
- `new/template_files` (generated `template_files.cpp` + hand-written header): `struct TemplateFile { const char*
  stored; const char* written; const unsigned char* bytes; std::size_t size; bool text; }` and the array.
- `new/template_render`: `std::vector<OutputFile> render_project(const NewInputs&)` — substitutes placeholders in
  text files, copies binaries, fails (programmer error, asserted by a test) on an unknown or leftover `@word@`; pure.
- `new/project_writer`: `WriteResult write_project(const std::string& folder, const std::vector<OutputFile>&, const
  NewFs&)` — FR-5 checks (`symlink_status`: missing → create; empty directory → fill; anything else → refuse, listing
  nothing changed), creates sub-folders, exclusive-creates each file, rolls back on failure; `NewFs` is a struct of
  `std::function`s (create folder, create file exclusively, remove file, remove empty folder) with the real one in
  `new/new_fs_system.cpp` (`_wopen(_O_CREAT | _O_EXCL | _O_BINARY)` on Windows, `open(O_CREAT | O_EXCL)` elsewhere)
  and a failing one in tests.
- `new/unique_id`: `random_unique_id()` from `std::random_device` (`rand_s` on MinGW-w64's libstdc++, `/dev/urandom`
  on Linux), uniform in the homebrew range.
- `commands/new.cpp`: glue (≈ 100 lines): validate, choose the engine, render, write, print.

### `nnh doctor` internals

- `process_run`: `RunResult run_program(path, args, timeout_ms, max_output)` — no shell (CreateProcessW with quoted
  arguments on Windows, `posix_spawn` + `poll` elsewhere), stdout and stderr merged, killed at the time limit,
  `{started, exit_code, timed_out, output}`. The one shell use is devkitPro's `bash.exe -lc "<fixed text>"` (no user
  text in it).
- `doctor/probes`: `struct Probes` of `std::function`s (env, file exists/is dir, read bounded text file, list folder
  names, find on `PATH`, run program, free space, OS name and version, console discovery, URL reachability); the real
  set in `doctor/probes_system.cpp`; tests build fakes field by field.
- `doctor/tool_versions`: parsers for `cmake --version`, `ninja --version`, `g++/c++ --version` (GCC, MinGW-w64 and
  WinLibs banners), `clang++ --version` (LLVM and Apple), `cl` (banner on stderr), `arm-none-eabi-gcc --version`, pacman
  package folder names (`libctru-2.6.0-1` → `2.6.0`); `Version {major, minor, patch}` with compare; bounded, pure,
  tested on recorded outputs (`tools/nnh/tests/data/doctor/*.txt`) and mini-fuzzed.
- `doctor/checks_host.cpp` (About, Simulator), `doctor/checks_3ds.cpp`, `doctor/checks_optional.cpp`,
  `doctor/checks_project.cpp`, `doctor/checks_network.cpp` (Console, Downloads): each `void check_<group>(const Probes&,
  const DoctorContext&, Report&)`; pure over the probes.
- `doctor/fix_lines`: the fix text per problem and host (Windows / Debian-like Linux / other Linux / macOS), one table.
- `doctor/doctor_report`: `Report` (groups, checks), `format_report(report, colour)`, `verdict_lines`, `exit_code(report,
  pc_only)`.
- `doctor/doctor_limits.hpp`: the table below.
- `commands/doctor.cpp`: glue (parse context: OS, inside a project or the repository, options; run groups; print).

Checks (each with its fix; Windows-only checks skipped on Linux and the reverse):

| Group | Check | `ok` | `warn` | `FAIL` |
|---|---|---|---|---|
| About | `nnh` | the version line | — | — |
| About | system | Windows 10/11, Linux | macOS: "best effort, not checked by the project" | — |
| Simulator | CMake | on `PATH`, ≥ 3.25 | — | missing, older |
| Simulator | Ninja | on `PATH` | — | missing |
| Simulator | C++ compiler | GCC/clang at or above the checked versions | below the checked, at or above the minimum; MSVC `cl` only: best effort | none, or below the minimum |
| Simulator | SDL3 | Linux: system `sdl3` (pkg-config); Windows MinGW: "downloaded at the first configure (pinned)" or found in a `build/pc-*/_deps` | — | Linux without system SDL3: no `pkg-config`, or neither `x11` nor `wayland-client` development files (no window); `warn` with no audio backend (`alsa`, `libpulse`, `libpipewire-0.3`) |
| 3DS | devkitPro | `DEVKITPRO` (Windows: else `C:/devkitPro`) holds `cmake/3DS.cmake` | Windows: `DEVKITPRO` unset but `C:/devkitPro` found (fine for `nnh`; `scripts/dkp.sh` and hand configures set it) | missing; Linux: `DEVKITPRO` unset (presets read it) with the `export` fix when `/opt/devkitpro` exists, else the installer link |
| 3DS | devkitARM, libctru, citro2d, citro3d | present (`devkitARM/bin/arm-none-eabi-gcc`, `libctru/lib/libctru.a`, `libctru/lib/libcitro2d.a`, `libcitro3d.a`), versions from pacman's database folder names when readable | version unreadable: `ok` with "version unknown" | missing |
| 3DS (Windows) | devkitPro's MSYS2 | `DEVKITPRO_MSYS2` or `<devkitPro>/msys2` has `usr/bin/bash.exe`; one `bash -lc` run finds `cmake` (≥ 3.25) and `ninja` | — | missing; `cmake`/`ninja` missing inside (the paste-able `pacman` line) |
| 3DS | host compiler for 3DS configures | `find_host_cxx` finds one (spec 032 FR-14; Linux: the Simulator's compiler) | — | none (`NNH_HOST_CXX` fix) |
| Optional | ccache, git, Azahar | found (version) | missing (never `FAIL`) | — |
| Console | `--console <ip>` / discovery | a console answered | — | none answered: "open the Homebrew Launcher and press Y, then try again" |
| Downloads | each pinned URL (SDL3 package for this host when no system SDL3; inside a project, its engine archive when it pins one) | reachable through `cmake` | — | unreachable (CA-certificate failure → the `CMAKE_TLS_CAINFO` fix) |
| This project | `nnh.toml` | parses (warnings shown) | unknown keys | malformed |
| This project | engine version | equals this `nnh`'s | differs (from `build/<preset>/CMakeCache.txt`'s `NNHGE_VERSION`, else `nnh.toml`'s `engine`; in the repository its `project(nnhge VERSION …)`): "the game↔tool protocol must match: use the nnh of NNHGE <x>, or change the pin" | — |
| This project | presets | `pc-dev`, `pc-release`, `3ds-dev`, `3ds-release` (and `NNH_PRESET` when set) defined (`presets_file_defines`) | — | one missing |
| This project | `NNHGE_ENGINE_DIR` | unset ("the pinned download"), or an engine folder (its version) | set while the project pins a release: "this project builds with <path>, not its pinned release" | set to a folder that is not an engine |
| This project | disk space | ≥ warn threshold | below it | below the FAIL threshold |

`doctor` finds a project's pin only in the form `nnh new` writes (`set(nnhge_url "…")`); a hand-written project's URL is
reported as "not read" (no `warn`). The CMake cache is read with `read_text_file` (4 MiB bound). The console check uses
`discover_console` with the remembered console (read from `nnh.ini`, never written), local broadcasts and
255.255.255.255; `--console` must be a LAN address (`is_local_network`), else exit 2 before any check. `--online` checks
the console too (discovery only).

Limits (`doctor/doctor_limits.hpp`, the same table in `nnh-tool.md`; compiler numbers provisional until T0.3):

| Item | `FAIL` below | `warn` below (checked by the project) |
|---|---|---|
| CMake | 3.25 | — |
| GCC | 12 (provisional) | 13 (VM 120: 13, 14; WinLibs 16 on Windows) |
| clang | 16 (provisional) | 18 (VM 120) |
| MSVC `cl` | 19.30 (provisional) | always `warn`: best effort |
| devkitPro's CMake (MSYS2) | 3.25 | — |
| Free space in the project folder | 0.3 GB (one fresh PC + 3DS build of the template: ≈ 0.27 GB from spec 032's `hello`-sized project, `pc-dev` 206 MB + `3ds-dev` 59 MB; re-measured at T4.4) | 1 GB (≈ 3×) |
| Program time limit | 10 s each | — |

Report format (FR-20):

```
About
  ok    nnh            nnh 0.1.0 (protocol 1, link 1, windows-x64, from a source checkout)
  ok    system         Windows 11 (10.0.26100)
Simulator
  ok    cmake          4.4.2 (C:/Program Files/CMake/bin/cmake.exe)
  FAIL  ninja          not found on PATH
        fix: winget install Ninja-build.Ninja
...
Simulator: not ready (1 problem)
3DS: ready
Optional: 1 warning
Console: not checked (--console <ip> or --online)
Downloads: not checked (--online)
This project: ready
```

Fix lines are one line each; on Windows a program path is written unquoted with forward slashes (works in PowerShell,
cmd and Git Bash; checked by hand in all three at T6.6); a path with spaces is never a command's program (fixes use
`winget`, the MSYS2 `bash.exe` under `C:/devkitPro`, or a link). The package names and installer links are verified at
T6.4 (winget IDs, Ubuntu packages, devkitPro's pacman page).

### FR-22: versions on a protocol refusal

`protocol_mismatch.{hpp,cpp}`: `std::string protocol_mismatch_message(Side side, std::optional<int> theirs, int ours,
const std::string& project_engine)` → e.g. `the game speaks remote protocol version 2, nnh 0.1.0 speaks 1 (this
project's engine: 0.2.0): use the nnh of NNHGE 0.2.0, or change the project's engine pin to 0.1.0`. Used:
- `ScriptRunner` (script and typed modes, PC and console): when the game's Hello record has another version (today's
  message replaced) and when the game rejects `nnh`'s Hello (a `Rejected` record for the Hello's sequence number before
  attaching: an older or newer game refuses the version, so `theirs` is unknown). The project's engine version comes in
  `RunnerOptions` (found as in `doctor`'s engine-version check, without the program runs). `script_runner.cpp` gains
  only the call sites (≈ +6 lines, 389 → ≈ 395); if it would cross 400, the Hello handling moves to
  `script_runner_hello.cpp` (the pattern of `script_runner_live.cpp`).
- `nnh serve`: when NNH Link's handshake is refused for the link protocol version, `nnh` prints the same message with
  the link versions (the console's text from `link/` is unchanged).

### Events introduced

None in the engine. The template subscribes to the engine's `SoundFinished` (spec 008).

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `tools/nnh/src/build_info.{hpp,cpp}` | new | Version, protocol versions, host, build kind, the release pin, the checkout folder; `version_line()`; the only source with the build-time definitions |
| `tools/nnh/src/cli.{hpp,cpp}` | changed | `Command::{New, Doctor, Version}`; `--version`; help text |
| `tools/nnh/src/cli_new_doctor.cpp` | new | Parsing of `new` and `doctor` options (keeps `cli.cpp` ≤ 200) |
| `tools/nnh/src/main.cpp` | changed | Dispatch; `--version` and `new` before `net::startup()` (no network library needed) |
| `tools/nnh/src/new/project_name.{hpp,cpp}` | new | FR-4 rules, suggestion; title/author rules; unique ID parsing |
| `tools/nnh/src/new/unique_id.{hpp,cpp}` | new | FR-11 random ID |
| `tools/nnh/src/new/engine_location.{hpp,cpp}` | new | FR-9, FR-10: pin or local engine, engine-folder check and version, the set-lines |
| `tools/nnh/src/new/template_files.hpp` | new | The embedded file table's type |
| `tools/nnh/src/new/template_render.{hpp,cpp}` | new | Placeholders → the output files (FR-7, FR-8) |
| `tools/nnh/src/new/project_writer.{hpp,cpp}` | new | FR-5, FR-6 over `NewFs` |
| `tools/nnh/src/new/new_fs_system.cpp` | new | The real `NewFs` (exclusive create) |
| `tools/nnh/src/commands/new.cpp` | new | `nnh new` glue and output (FR-3) |
| `tools/nnh/src/process_run.{hpp,cpp}` | new | Programs with a time limit and bounded output |
| `tools/nnh/src/doctor/probes.hpp`, `probes_system.cpp` | new | The probe seam and the real probes |
| `tools/nnh/src/doctor/tool_versions.{hpp,cpp}` | new | Version parsers |
| `tools/nnh/src/doctor/doctor_limits.hpp` | new | Minimums and thresholds |
| `tools/nnh/src/doctor/doctor_report.{hpp,cpp}` | new | Types, formatting, verdicts, exit code |
| `tools/nnh/src/doctor/fix_lines.{hpp,cpp}` | new | Fix text per problem and host |
| `tools/nnh/src/doctor/checks_host.cpp`, `checks_3ds.cpp`, `checks_optional.cpp`, `checks_project.cpp`, `checks_network.cpp`, `checks.hpp` | new | One group per file |
| `tools/nnh/src/commands/doctor.cpp` | new | `nnh doctor` glue |
| `tools/nnh/src/protocol_mismatch.{hpp,cpp}` | new | FR-22's message |
| `tools/nnh/src/remote/script_runner.{hpp,cpp}`, `commands/run.cpp`, `run_pc.cpp`, `serve.cpp` | changed | FR-22 call sites; the project's engine version passed in |
| `tools/nnh/src/commands.hpp` | changed | `command_new`, `command_doctor` |
| `tools/nnh/template/` | new | The template files (table above) and a `README.md` of provenance (not embedded) |
| `tools/nnh/embed_template.cmake` | new | `-P` script: the listed template files → `template_files.cpp` (LF for text, fixed order) |
| `tools/nnh/CMakeLists.txt` | changed | New sources; the generated source with `DEPENDS` on every template file (explicit list); the build-time definitions on `build_info.cpp` only; `NNH_RELEASE_ENGINE_URL`/`_SHA256` checked; under `NNH_BUILD_TESTS`: the check game `nnh_template_check` (the template's sources and assets, engine warnings as errors) and the ctests below |
| `cmake/nnh_sdl3_pin.cmake` | new | `NNH_SDL3_VERSION`, the package URLs and hashes (one source for `nnh_sdl3.cmake` and `doctor --online`) |
| `cmake/nnh_sdl3.cmake` | changed | Includes the pin file (no behaviour change) |
| `tools/nnh/tests/test_build_info.cpp`, `test_project_name.cpp`, `test_engine_location.cpp`, `test_template_render.cpp`, `test_project_writer.cpp`, `test_tool_versions.cpp`, `test_doctor_report.cpp`, `test_doctor_host.cpp`, `test_doctor_3ds.cpp`, `test_doctor_project.cpp`, `test_doctor_network.cpp`, `test_process_run.cpp`, `test_protocol_mismatch.cpp` | new | AC-1, AC-6, AC-8, AC-9 |
| `tools/nnh/tests/doctor_fakes.hpp` | new | Fake probes |
| `tools/nnh/tests/data/doctor/` | new | Recorded tool outputs (CMake 3.25 and 4.x, Ninja, GCC/MinGW/WinLibs, clang, MSVC `cl`, `arm-none-eabi-gcc`) |
| `tools/nnh/tests/test_cli.cpp`, `test_script_runner.cpp`, `test_fuzz_tool.cpp` | changed | New options; FR-22 cases; mini-fuzz of the version parsers and the cache reader |
| `tools/nnh/tests/new_files.cmake`, `template_smoke.cmake`, `new_smoke.cmake`, `pin_check/` | new | The ctest drivers below |
| `.gitattributes` | changed | `tools/nnh/template/** text eol=lf` (binaries `binary`) |
| `scripts/vm_check.sh` | changed | `final`: a generated project's 3DS builds through `nnh` with `/opt/devkitpro`, `nnh doctor` with `DEVKITPRO` unset and set (AC-3, AC-7) |
| docs | new/changed | `docs/guides/your-first-game.md` (new), `getting-started.md` (rewritten), `nnh-tool.md`, `game-projects.md` (`nnh new` generates it; layout note), `README.md` quick start, `docs/tech/cmake.md` (the template check, the ctests, the pin cache entries), `docs/architecture/project-structure.md`, `decisions.md` D56, `CLAUDE.md`, `docs/ROADMAP.md` |

### Data and memory

No engine change, no 3DS memory change. The template game: one 32×32 RGBA texture, one 3 KB sound, the engine font:
`hello` plus a few KB. `nnh` grows by the embedded template (≈ 15 KB) and the new code. `nnh_template_check` adds three
compile units and two assets to PC builds with tests. Disk for the implementation on `E:` (precondition ≥ 10 GB): the
worktree's four presets (≈ 2 GB, spec 032), the end-to-end tests (≈ 0.5 GB each, deleted after passing), generated
projects of stages 4 and 9 (≈ 0.3 GB each, deleted).

### Platform work

- pc: none in the platform library. `nnh` (a PC host program) gains the commands.
- 3ds: none in the engine; the template is built for the 3DS from a generated project (AC-3) and run on the Old 3DS XL
  (AC-4).
- fake (tests): fake probes and a failing `NewFs` in `nnh_tool_tests`; `fake_game.hpp` answers another protocol
  version (AC-9).

## Test strategy

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `test_build_info.cpp` (line format, build kind = pin present, host names); `nnh_new_files` runs `nnh --version` (exit 0, the line) |
| FR-2 | `test_cli.cpp` (new commands and options, unknown options → error, `--console` not on the LAN → error); help text lists them; `nnh_new_files` checks exit 2 |
| FR-3 | `test_template_render.cpp`, `test_project_writer.cpp`; `nnh_new_files` (the process: output, next commands) |
| FR-4 | `test_project_name.cpp`: valid names; each rule (separator, `..`, drive letter, leading `-`/`.`, upper case, 33 characters, `con`, `com1`, `dist`, `all`, `nnh_x`) refused with nothing created; suggestions (`My Game` → `my-game`) |
| FR-5 | `test_project_writer.cpp`: existing file, non-empty folder, a link (skipped and reported where symlinks can't be made) refused with the listing (names, sizes, times) unchanged; an empty folder filled |
| FR-6 | `test_project_writer.cpp`: a failing `NewFs` at each file index → nothing left (created folder removed; pre-existing empty folder left empty), the failing file named |
| FR-7 | `test_template_render.cpp`: same inputs → byte-identical outputs; no leftover placeholder; text files LF; the presets equal to `tests/consumer/CMakePresets.json` |
| FR-8 | `test_template_render.cpp`: the exact file list; `nnh.toml` content (read back by `read_project_file`: no warning) |
| FR-9 | `test_engine_location.cpp` and `test_template_render.cpp` (pin / no pin, the guard); `pin_check` ctest (a wrong pin fails `nnh`'s configure); `nnh_new_smoke` pinned way |
| FR-10 | `test_engine_location.cpp`: `--engine` not an engine → refused before anything is created; checkout default; set-lines per shell; variable already equal / different; no file holds the path (render test greps every output) |
| FR-11 | `test_project_name.cpp` (`--unique-id` range, forms), `random_unique_id()` over 10,000 draws within range |
| FR-12 | README content checked by `test_template_render.cpp` (the licence section's lines); reviewed against the licences (below) |
| FR-13, FR-14 | `nnh_template_check` (engine warning flags as errors) + `nnh_template_smoke` in every non-Release PC `ctest`; review of the sources; AC-4 on the console |
| FR-15 | `nnh_new_smoke` (local and pinned ways, PC); T4.3 (Windows 3DS `nnh build`, `--release`, `nnh install`'s target `dist`); VM `final` (Linux 3DS) |
| FR-16 | `nnh_template_smoke`, `nnh_new_smoke`, AC-4 |
| FR-17 | T4.4 (7 warm runs each, interleaved, as spec 032's AC-9): simulator < 2 s, `.3dsx` < 5 s; ROADMAP table |
| FR-18 | `test_doctor_network.cpp` (no network probe called without the options), `test_process_run.cpp` (time limit kills, output bounded); T6.6 real run timed; listings of the project and user config folders before/after (AC-8) |
| FR-19 | `test_doctor_host.cpp`, `test_doctor_3ds.cpp`, `test_doctor_project.cpp`, `test_doctor_network.cpp` (every row of the checks table, AC-6's list); `test_tool_versions.cpp` on recorded outputs |
| FR-20 | `test_doctor_report.cpp` (format, `fix:` under every `warn`/`FAIL`, verdict lines, `NO_COLOR` text complete) |
| FR-21 | `test_doctor_report.cpp` (0/1, `--pc`), `test_cli.cpp` (2) |
| FR-22 | `test_protocol_mismatch.cpp`; `test_script_runner.cpp` (a fake game answering version 2; a fake game rejecting the Hello) |
| FR-23 | the checks' OS switch tested with fake OS values; real runs on Windows and VM 120 (AC-7) |
| FR-24 | T8.x docs; T8.4 follows "Your first game" verbatim on Windows; VM `final` follows its Linux part; `docs_tool.py check` |

ctests added:

| ctest | Label | What | Cost |
|---|---|---|---|
| `nnh_tool` | — | the new unit tests inside `nnh_tool_tests` | < 1 s more |
| `nnh_new_files` | — | the built `nnh`: `--version`, `help`, `new demo` in a scratch folder (exit 0, file list), again (exit 1), `"My Game"` (exit 1, suggestion), an unknown option (exit 2) | ≈ 1 s |
| `nnh_template_smoke` | — | `nnh run nnh_template_check --pc --no-build --script tools/nnh/template/tests/smoke.nnhs`, headless, non-Release PC presets (like `nnh_script_pc_smoke`) | ≈ 10 s |
| `nnh_pin_check` | — | `cmake` configures `pin_check/` (a scratch project including the pin-validation snippet) with a malformed pin: fails naming the entry | ≈ 1 s |
| `nnh_new_smoke` | `consumer-full` | in an empty scratch folder: `nnh new demo --engine <engine>` with `NNHGE_ENGINE_DIR` set → configure `pc-dev` once with the parent's SDL3 source (`FETCHCONTENT_SOURCE_DIR_NNH_SDL3*`, no network) → `nnh build demo --pc` → `nnh run demo --pc --no-build --script tests/smoke.nnhs` exit 0; no warning in the game's own compile lines; then the pinned way (pack, paste, variable unset) | ≈ 2–3 min each way (one engine build; `CCACHE_NOHASHDIR`) |

Planned hand mutants (T8.1): a rule of FR-4 dropped (each); the suggestion wrong; the existing-empty-folder case
treated as non-empty and the reverse; a link accepted; the exclusive create replaced by an overwrite; the rollback
skipped; the pin ignored; the guard condition inverted; the set-lines printed when already set; the checkout default
not checked; the unique ID range off by one; CMake's minimum raised and lowered; the compiler minimum/checked swapped;
`warn` counted in the exit code; `--pc` ignored; a network probe called without `--online`; `DEVKITPRO` unset not
failing on Linux; the MSYS2 Ninja check inverted; the engine-version comparison inverted; the program time limit
ignored; the FR-22 message without the project version. Each turns a named test red or is argued equivalent.

### Licence note (FR-12, Q8)

Settled for the README (each claim with a link to the licence file in the engine's `third_party/` or upstream):
NNHGE, SDL3, libctru, citro2d, citro3d: zlib (no notice required in binaries); `stb_vorbis`, `stb_image`,
`stb_truetype`: MIT or public domain, the public-domain option needs no notice (`stb_image`/`stb_truetype` are host-only
and never in a game binary); Ryu (`f2s`): Apache-2.0 or BSL-1.0 (BSL-1.0 needs no notice for object code), linked only
in Debug/Development games that use live values; devkitARM's runtime: libstdc++ and libgcc under the GCC Runtime
Library Exception (no notice for a normal compile), newlib: a set of BSD-style licences some of which ask for the notice
in binary distributions — the README says a credits line for devkitPro/newlib is a good habit and links to
`newlib/COPYING.NEWLIB`, without claiming "no notice needed" for it (T3.5 records the files checked). Verified at T3.5
against the vendored `third_party/*/LICENSE*` files and devkitPro's installed `licenses/` folder.

## Check order and stages

Stages 0–8 implement; then (stage 9) one independent review together with one `scripts/vm_check.sh final` (Linux
`ctest` including `consumer-full`, the generated project's 3DS builds with `/opt/devkitpro`, `nnh doctor` with and
without `DEVKITPRO`), fixes re-checked with `fast`; then (stage 10) the console run (AC-4) while the maintainer is
there, and the merge. Stage ends on Windows: `nnh_tool_tests` (the module filters while developing), the default
`ctest` once per stage end (`-LE consumer-full`), 0 warnings, `docs_tool.py check` when docs changed.

## Risks and alternatives

1. **FR-10's amendment** (question 1): the checkout case needs one environment variable before the first build, which
   FR-15's "out of the box" did not foresee. Mitigation: `nnh new` prints the exact lines, the guard message says what
   to do, `doctor` reports it; release `nnh` projects need nothing.
2. **`doctor --online` writes a temporary script** (question 3): the spec says `doctor` writes nothing. The file is in
   the system's temporary folder, uniquely named, removed afterwards; the project and user-config listings stay
   unchanged (AC-8). Alternative: drop the Downloads group until `nnh` can fetch over TLS itself (it can't today).
3. **Compiler minimums** (question 4): finding the oldest working GCC/clang "by trying" needs older compilers on VM 120
   (`apt install g++-12 clang-16`, a system change needing the maintainer's OK). Without it, the provisional numbers
   stay and are labelled so in `nnh-tool.md`.
4. **Fix lines in three Windows shells**: quoting differs (PowerShell needs `&` before a quoted program). Mitigation:
   no quoted program paths (devkitPro's default has no space); each fix line run by hand in the three shells (T6.6).
5. **`script_runner.cpp` at 389 lines**: FR-22 call sites could cross 400. Mitigation: the text lives in
   `protocol_mismatch.cpp`; fallback split named above.
6. **The template in the engine's manifest**: `nnh_template_check` appears in the engine repository's `nnh_games.txt`
   (so in `nnh serve`'s list in PC presets with tests). Question 5 offers building it only in the ctest driver instead.
7. **`std::random_device` on old MinGW** was deterministic (GCC < 9.2); the checked WinLibs GCC 16 and Linux are fine;
   a test draws twice in two processes (`nnh_new_files`) and expects different IDs (a 1 in 1,015,040 false failure,
   acceptable; the test retries once).
8. **Non-ASCII current folders on Windows**: `nnh` opens files with narrow paths elsewhere; `new_fs_system.cpp` uses
   wide APIs (`_wopen`, `CreateDirectoryW`) from UTF-8, and the e2e runs from an ASCII path. The rest of `nnh` keeps its
   current limit (noted in `nnh-tool.md`).
9. **Linux SDL3 dependency probing** varies by distribution: fix lines only for Debian-like systems (`/etc/os-release`
   `ID`/`ID_LIKE`), a link elsewhere.
10. **Disk on `E:`**: e2e folders deleted after passing; checked before stages 4 and 9.

Alternatives rejected: the template as C++ string literals (not compiled or tested as a game, escapes everywhere); a
template downloaded at `nnh new` time (FR-7); writing a `CMakeUserPresets.json` (CMake refuses duplicate preset names,
D55); a placeholder URL with a zero hash in the checkout case (works, measured by spec 032's fixture, but an empty pin
plus a guard says what it is and is what a release `nnh` fills); `doctor` checks by configuring a scratch project (slow,
writes a build folder).

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | One template source for both systems; `nnh`'s system code stays in the tool (`process_run`, `new_fs_system`, `probes_system`) |
| II | ✅ | No engine change; the template is `hello`-sized; 60 Hz and controls checked on the Old 3DS |
| III | ✅ | Budgets measured on a generated project (AC-5); the e2e under `consumer-full`; the default `ctest` adds ≈ 12 s; the version and pin definitions recompile one `nnh` file |
| IV | ✅ | Template: scene, queries, `SoundFinished` |
| V | ✅ | No engine change; template allocation-free per frame |
| VI | ✅ | Pure functions over fakes first; template proven by scripts; mutants |
| VII | ✅ | `src/new/`, `src/doctor/` one concept per file; `script_runner.cpp` guarded |
| VIII | ✅ | New guide, rewritten getting-started, `nnh-tool.md`, README, D56; verbatim check |
| IX | ✅ | Engine-made assets with provenance; licence note checked per library |
| X | ⚠️ | Hardened as designed; one deviation for the maintainer: `doctor --online`'s temporary script (question 3) |
| XI | ✅ | Hand-written and in-repository games unchanged |

No constitution exception is requested. D56 is new (proposed). Existing decisions touched additively: D23 (`nnh`'s
commands), D28 (the Hello mismatch message), D55 (item 8 confirmed as the only local-engine mechanism; spec 033's FR-10
amended to it). Flagged: questions 1, 3 and 4; risks 1, 3 and 4.

## Questions for the maintainer

1. **FR-10 rewritten to D55's `NNHGE_ENGINE_DIR`** (spec amended, marked in its status line): `nnh new` checks the local
   engine (`--engine`, or the checkout that built it), writes its path nowhere, and prints the lines that set the
   variable (Git Bash, PowerShell, cmd, `setx`), skipping them when it is already set to that folder; `doctor` reports
   the variable. *Recommendation: yes.* It is D55's one mechanism, works with plain `cmake --preset`, and a contributor
   sets it once per machine. Alternatives: (b) a git-ignored `nnh.local.toml` read by `nnh` (works without a shell
   setting, invisible to plain `cmake`); (c) `nnh` falling back to its own checkout (implicit, also for hand-written
   projects).
2. **The checkout case's `CMakeLists.txt`: empty `nnhge_url`/`nnhge_sha256` plus a guard** (measured: an empty `URL` and
   `URL_HASH SHA256=` configure with the override in native CMake 4.4.2 and devkitPro's 4.0.2; 3.25/3.28 checked at
   T0.2)? *Recommendation: yes* — one shape for both cases, honest about "no pin", and a release `nnh` fills the same two
   lines. Fallback if an older CMake refuses: the consumer fixture's placeholder (`file://unused/…`, zero hash) behind
   the same guard.
3. **`doctor --online` runs CMake on a temporary script in the system's temporary folder** (removed afterwards; nothing
   in the project or `nnh.ini`), using `file(DOWNLOAD <url> RANGE_START 0 RANGE_END 0)` without an output file so only
   one byte is fetched? *Recommendation: yes* — the spec's "through the same CMake the build uses" needs a script, and
   CMake reads scripts only from files; AC-8's listings cover what matters. Alternative: no Downloads group in 1.0.
4. **Compiler minimums**: allow `apt install g++-12 clang-16` (and `clang-17`) on VM 120 to find the oldest compilers
   that build the engine and the template (T0.3)? *Recommendation: yes* (a few hundred MB on the VM; removable). Without
   it: `FAIL` below GCC 12 / clang 16 / MSVC 19.30 as provisional values, labelled "not verified" in `nnh-tool.md`.
5. **The template compiled as a check game (`nnh_template_check`) in the engine's PC builds with tests**, listed in the
   repository's games manifest (so also in `nnh serve`'s list there)? *Recommendation: yes* — every PC `ctest` then
   compiles the template with the engine's warnings as errors and runs its smoke script in ≈ 10 s. Alternative: build it
   only inside `nnh_new_smoke` (`consumer-full`), which catches a broken template only on the VM and at releases.
6. **The template's quit**: START logs `play: quit`, shows "Bye!" for 30 frames, then ends (so the smoke script's last
   `expect` and `end` pass before the game closes)? *Recommendation: yes* — spec 032's console run showed that a game
   closing before `end` fails the script.
7. **`--author` not given: the `AUTHOR` keyword written as a commented line** (`AUTHOR ""` warns under CMake 4, CMP0174,
   measured)? *Recommendation: yes* — FR-3's "empty value the build accepts" without a warning in a fresh project.
8. **`doctor --console <ip>` with a non-LAN address exits 2** (a bad command line, before any check), as `nnh run`
   refuses it (it exits 1 there after building)? *Recommendation: yes* — nothing has run yet, so it is an argument
   error.
9. **devkitPro library versions read from pacman's local database folder names** (`msys2/var/lib/pacman/local/
   libctru-2.6.0-1`; on Linux `/opt/devkitpro/pacman/var/lib/pacman/local` when present), no program run; "version
   unknown" otherwise (VM 120's devkitPro was extracted from a Docker image, checked at T0.2)? *Recommendation: yes*.
10. **FR-22 in `nnh serve`**: the PC prints the versions when NNH Link's handshake fails for the link protocol version;
    the console's message stays NNH Link's? *Recommendation: yes* — the console side belongs to spec 011's app and would
    need a new NNH Link build for a message only.

## Implementation results

### Stage 0 results (2026-11-27; worktree `spec-033` at `c30c22c`)

T0.1, Windows (WinLibs GCC 16.2.0, CMake 4.4.2, Ninja, ccache; devkitARM r68 / devkitPro CMake 4.0.2), in the worktree:
- free on `E:` before the builds 12 GB, after the four preset folders 10 GB (`pc-debug` 798 MB, `3ds-dev` 208 MB,
  `3ds-release` 188 MB);
- fresh `pc-debug` configure 13.9 s (SDL3 from the main checkout's download through
  `FETCHCONTENT_SOURCE_DIR_NNH_SDL3_PREBUILT`), build 5 min 31 s cold, 0 warnings; `3ds-release` and `3ds-dev` configure +
  build 2 min 19 s / 2 min 12 s, 0 warnings;
- `nnh_tool_tests`: 279 tests, 14.1 s; `nnh.exe` (Debug) 22,238,914 bytes;
- `3ds-release` `hello.3dsx` SHA-256 `92e8dbd27aa0d256be55726e9d0d5dbae6de232d34a9783d72647505bdd2408f` (the baseline
  for every later comparison in this worktree; the binary embeds the folder, so only same-folder values compare);
- edit `examples/hello/main.cpp`, three runs: simulator (`pc-debug`) 2.15 / 2.10 / 2.07 s, `.3dsx` (`3ds-dev`) 3.65 /
  3.46 / 3.67 s. The simulator number is already above the 2 s budget before any change of this spec (spec 032 measured
  2.02–2.09 s in `pc-debug` on the same machine): pre-existing, recorded, not caused here.

T0.2:
- **The no-pin `CMakeLists.txt` shape** (empty `nnhge_url`/`nnhge_sha256`, the guard, `FetchContent_Declare` with an empty
  `URL` and `URL_HASH SHA256=`), with a stub engine folder (`project(nnhge VERSION 0.1.0)`, a `cmake_parse_arguments`
  `nnh_add_game`): Ubuntu's CMake 3.28.3 on VM 120 configures with `FETCHCONTENT_SOURCE_DIR_NNHGE` set (no download step,
  no warning; `AUTHOR ""` parsed silently, CMP0174 does not exist before 3.31) and without it stops at the guard before
  any `_deps` folder exists. CMake 3.25 itself was **not measured**: fetching a CMake 3.25 binary onto the VM was not
  allowed in this session (no system change was wanted either); 3.25 stays the documented minimum (spec 032 measured its
  presets schema).
- **`file(DOWNLOAD <url> RANGE_START 0 RANGE_END 0)` without an output file**, the SDL3 release URL (a GitHub redirect):
  devkitPro's MSYS2 CMake 4.0.2 `0;"No error"` in 1.1 s; VM 120's CMake 3.28.3 `0;"No error"` in 0.46 s; nothing written
  in the folder. **WinLibs' native CMake 4.4.2: status 60 ("SSL peer certificate … was not OK")**: it ships without CA
  certificates, exactly the failure the Downloads group must catch; with `-DCMAKE_TLS_CAINFO=<Git for Windows'
  ca-bundle.crt>` (what `cmake/nnh_sdl3.cmake` already does) `0;"No error"` in 0.78 s. So `doctor --online` passes the
  same CA bundle the build would (FR-19: "through the same CMake the build uses").
- **devkitPro's MSYS2 probe** `bash -lc "command -v cmake ninja; cmake --version; ninja --version"`: 1.14 s
  (`/usr/bin/cmake`, `/usr/bin/ninja`, 4.0.2, 1.13.2). Note: `bash -lc` starts in the MSYS2 home folder.
- **pacman database folders**: Windows `C:/devkitPro/msys2/var/lib/pacman/local/` holds `devkitARM-r68-1`,
  `libctru-2.7.1-1`, `citro2d-1.7.0-1`, `citro3d-1.7.1-2`, `devkitarm-gcc-16.1.0-1`, `devkitarm-newlib-4.6.0.20260123-5`,
  `cmake-4.0.2-1`, `ninja-1.13.2-2`; VM 120 (devkitPro extracted from the Docker image) has
  `/opt/devkitpro/pacman/var/lib/pacman/local/` with `devkitARM-r68-1`, `libctru-2.7.0-1`, `citro2d-1.7.0-1`,
  `citro3d-1.7.1-2`. devkitARM's version is `r68` (not dotted): the parser keeps it as text.
- **Exclusive create**: covered by `test_project_writer.cpp` (MinGW `_wopen` here; Linux at the stage 9 pass).
- **MSVC `cl`**: none on this machine; the parser is tested on a recorded banner sample.

T0.3 (question 4, `sudo apt install g++-12 clang-16 clang-17` on VM 120): each compiler built, from the synced
worktree, `nnh_core`, `nnh_platform_pc`, the examples whose APIs the template uses (`hello`, `scenes`, `input`,
`sprites`, `text`, `audio`) and `nnh` itself (Debug, `-Wall -Wextra`), SDL3 from source:

| Compiler | Result |
|---|---|
| GCC 12.4.0 | 0 errors, 0 warnings (132 s) |
| clang 16.0.6 | 0 errors, 0 warnings (161 s) |
| clang 17.0.6 | 0 errors, 0 warnings (152 s) |

The minimums are therefore GCC 12 and clang 16 (the oldest tried; older ones were not installed, so below them `doctor`
says `FAIL` as "older than any compiler the project has built with"). The template's own sources were not built with
them (they did not exist yet); the stage 9 pass builds them with GCC 13/14 and clang 18. The three compilers stay
installed on the VM (≈ 0.9 GB; removable with `sudo apt remove g++-12 clang-16 clang-17`).

### Stage 1 results (`nnh --version` and the command line)

- `build_info.{hpp,cpp}`: the only source with the definitions (`NNH_TOOL_VERSION`, `NNH_RELEASE_ENGINE_URL`,
  `NNH_RELEASE_ENGINE_SHA256`, and `NNH_TOOL_ENGINE_DIR` only without a pin), set with `set_source_files_properties`
  on that one file. `nnh --version` on Windows: `nnh 0.1.0 (protocol 1, link 1, windows-x64, from a source checkout)`,
  exit 0; it runs before `net::startup()`.
- Refinement (T1.2): the pin check is a function in `tools/nnh/nnh_release_pin.cmake` (`nnh_check_release_pin`), called
  by `tools/nnh/CMakeLists.txt` on the two cache entries; the ctest `nnh_pin_check` runs it in `cmake -P` children
  (10 cases: none, `https://`, `file://`, half a pin each way, `http://`, a space, 65 and 4 digits, non-hex), which
  needs no scratch configure (1.8 s). Seen red with the function emptied (7 of 10 cases failed).
- CLI: `parse_new`/`parse_doctor` live in `cli_new_doctor.cpp` in the namespace `nnh::tool::cli_parts` (a nested
  `detail` would hide `nnh::detail` from every file that includes `cli.hpp`). The name of `nnh new` is `Options::game`.
  `nnh doctor --console` must parse as IPv4 and be on the LAN, else exit 2 before anything runs (question 8);
  `nnh new` without a name, a second name or an unknown option exits 2; `--title ""` is accepted by the parser and
  refused by `nnh new` itself (exit 1).
- Tests: 279 → 287 (`test_build_info.cpp` 3, `test_cli.cpp` +5), all seen red first (a stubbed `version_line`, stub
  parsers).
- Stage end: `nnh_tool_tests` green, ctests `nnh_tool`, `nnh_pin_check`, `nnh_script_pc_smoke` green, `pc-debug` 0
  warnings, `docs_tool.py check` 0 broken links. No 3DS check: `tools/nnh/` is not part of 3DS presets.
- The default `ctest -LE consumer-full -j4` took 627 s here; its four `reload_*` failures were caused by editing
  `tools/nnh/CMakeLists.txt` while it ran (those tests rebuild in the same build folder, which re-ran CMake on a
  half-edited file) and passed when re-run (stage 2 end). Lesson kept: no CMake edits during a ctest run.
