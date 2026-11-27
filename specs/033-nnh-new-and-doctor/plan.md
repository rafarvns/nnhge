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
| III | ⚠️ | **AC-5/FR-17 not met for the simulator** (review round, like for like in `pc-debug`, 7 warm interleaved runs: the template's `play_scene.cpp` edit median 2.48 s, the repository's `hello` median 2.17 s, both over the < 2 s budget on this machine; `.3dsx` 3.61 s, within < 5 s): a question for the maintainer under "What is open". The e2e under `consumer-full`; the default `ctest` adds ≈ 12 s; the version and pin definitions recompile one `nnh` file |
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

### Stage 2 results (`nnh new`'s core)

- `new/project_name`: the FR-4 rules in this order, each named in the refusal: empty, a path separator, `..`, a drive
  letter, a leading `-`/`.`, more than 32 characters, a character outside `[a-z0-9_-]` (upper case included: "only
  lowercase letters, digits, '_' and '-'"), not starting with a letter, a Windows device name, a target name
  (`dist all clean install test help package package_source edit_cache rebuild_cache list_install_components nnh`,
  `nnh_*`, `nnh-*`). The suggestion lower-cases, turns each run of other characters into one `-`, starts at the first
  letter, cuts at 32 and appends `-game` to a device or target name; nothing when the result is still invalid
  (`nnh_x`, `!!!`). Titles and authors: strict UTF-8 (no overlongs, surrogates or > U+10FFFF), ≤ 64 characters, no
  C0/DEL/C1 controls, none of `" \ $ ; @ `` ` ``. `--unique-id`: `0x`/`0X` optional, hex only, the homebrew range.
- `new/engine_location`: as designed; `engine_dir_from_option` makes `--engine` absolute (MSYS form, `\`, relative to
  the current folder); the set-lines are quoted only when the path has a space (cmd as `set "NNHGE_ENGINE_DIR=..."`).
  An engine folder = `CMakeLists.txt` with a line starting `project(nnhge VERSION x.y.z` + `cmake/nnh_game.cmake`.
- Refinement (T2.4): `file(WRITE)` is a text write on Windows (`\n` became `\r\n`, seen in the stand-in), so the embed
  script normalises line endings in hex: the bytes become `,xx` tokens and `,0d,0a` → `,0a` (a pair can only match at a
  token boundary). The stand-in (`tools/nnh/tests/data/template_standin/`) is committed, its `greeting.txt.in` kept with
  CRLF by `-text` in `.gitattributes`; `art/blob.png` holds NUL, CR LF and 0xFF bytes. The generated source is always
  rewritten (the command only runs when a listed file or the script changed).
- `.gitattributes`: `tools/nnh/template/** text eol=lf`, then `*.png`/`*.wav` under it `binary` again (a later line
  wins), and the stand-in's `-text`.
- Placeholders may hold digits (`@engine_sha256@`); a `@` not followed by `word@` is copied; values are never scanned
  again; an unknown placeholder fails the render naming the stored file.
- `new/new_fs_system`: Windows uses `GetFileAttributesW` (a reparse point, symbolic link or junction, is a link),
  `CreateDirectoryW`, `_wopen(_O_CREAT | _O_EXCL | _O_BINARY)`, `DeleteFileW`, `RemoveDirectoryW` from UTF-8; elsewhere
  `symlink_status`, `mkdir`, `open(O_CREAT | O_EXCL | O_CLOEXEC)`, `unlink`, `rmdir`. MinGW's
  `std::filesystem::create_directory_symlink` reports "Function not implemented", so the real-link case of
  `a_link_is_refused_unchanged` is skipped here (it runs on Linux) and the rule is checked with a fake `kind`.
- Tests: 287 → 323 (`test_project_name.cpp` 8, `test_engine_location.cpp` 11, `test_template_render.cpp` 7,
  `test_project_writer.cpp` 9 + 1 in `test_template_render.cpp`'s embed check), seen red against stubs (181 failing
  checks; a stubbed render crashed on an empty file list, so the test now stops when the count is wrong).
- Mutation spot-check, one per file, each killed: the Windows-device rule dropped (`names_windows_reserves_are_refused`);
  the random range from 0 (`random_unique_ids_stay_in_the_homebrew_range`); case-sensitive folder comparison on Windows
  (`the_set_lines_are_skipped_when_the_variable_already_names_the_folder`); an unknown placeholder copied
  (`an_unknown_placeholder_is_an_error`; the first try, `continue` without advancing, hung the test: killed by hand);
  the rollback skipped (three writer tests); `_O_EXCL` → `_O_TRUNC` (`the_system_create_is_exclusive`).
- Stage end: `nnh_tool_tests` 323/323, ctests `nnh_tool`, `nnh_pin_check`, `nnh_script_pc_smoke` and the four
  `reload_*` green, `pc-debug` 0 warnings.

### Stage 3 results (the template)

- The twelve files are embedded in the order of `NNH_TEMPLATE_FILES`; the render tests check the exact list, LF text,
  no leftover `@word@`, no checkout path in any file, `nnh.toml` read back with no warning, the presets byte-equal to
  `tests/consumer/CMakePresets.json` (CR removed from the checkout's copy), the `CMakeLists.txt` values (name, title,
  ID, the guard, `VERSION_LESS 0.1`, the commented author, docs links per build kind) and every README section. Seen
  red with only two files embedded (3 of the 6 new tests).
- `CMakeLists.txt.in` follows the sketch, plus a commented `DESCRIPTION` line and one comment naming `BANNER`,
  `BANNER_AUDIO`, `PRODUCT_CODE` (they belong after `CIA UNIQUE_ID`, so they can't be a commented keyword line of
  their own). The author line, when not given: `# AUTHOR "Your name"  # shown in the HOME Menu (default: "Unknown")`.
- Refinement (T3.3): the scene logs `play: started` on its **first `update()`**, not in `on_enter()`: `on_enter()` runs
  at frame 0, before a test script attaches, and a script's frames start at 1 (the first run failed with "no matching
  line in frames 1-61"). The script waits 10 frames after each `expect` before the next input (`nnh` warned that input
  right after a log line may reach the console late).
- The scene sets both screens' clear colours, draws the sprite, the title (`NNH_GAME_TITLE`, scale 2 with a shadow),
  the controls, "Bye!" while quitting, and counts the blips that ended (`SoundFinished` for its own voice) on the
  bottom screen with a stack buffer. `main.cpp` 19 lines, `play_scene.hpp` 32, `play_scene.cpp` 105.
- Includes: `main.cpp` uses `"scenes/play_scene.hpp"`, the scene's source `"play_scene.hpp"` (relative to each file:
  no include path is added for a game's own folders).
- `nnh_template_check` (TITLE "Template Check") gets `nnh_engine_options` (`-Wall -Wextra -fno-exceptions -fno-rtti`)
  and `-Werror`: 0 warnings in `pc-debug`, `pc-dev` and `pc-release`.
- `nnh_template_smoke`: PASS in `pc-debug` and `pc-dev` (3.0 s), `nnh: PASS (19 lines, frames 1-66)`; the two PNGs of
  `screenshot moved` checked; not registered in Release (no remote control). A copy expecting `play: blop` made it fail
  (`nnh exited with 1`), seen once.
- T3.4: the screenshot of the check game (`NNH_EXIT_AFTER_FRAMES=30`) and the script's `moved` pair read back: title
  and controls on the top screen, the sprite (moved right to (244, 104) in the script), the bottom screen's text and
  "Sounds finished: 1" after the blip.
- Assets: `player.png` (155 bytes) drawn by `tools/nnh/make_template_player.py`; `blip.wav` = `tests/data/sounds/
  ok_mono16.wav` (byte-equal to the consumer fixture's `blip.wav`), synthesized by `tests/data/sounds/make_sounds.py`.
  Provenance in `tools/nnh/template/README.md`.
- T3.5 licence note, files checked: `third_party/ryu/LICENSE-Boost` (Boost 1.0), the stb READMEs and the licence
  blocks at the end of `stb_vorbis.c`, `stb_image.h`, `stb_truetype.h` (MIT or public domain). devkitPro's installed
  `licenses/` folder (`C:/devkitPro/licenses/`) holds only its tools' `COPYING` files (3dslink, 3dstools, gdb, ...): no
  newlib, libctru, citro2d or citro3d licence is installed, so the README names the licences and links newlib's
  `COPYING.NEWLIB` upstream rather than a local file.

### Stage 4 results (`nnh new` end to end)

- `commands/new.cpp` (≈ 100 lines): name → title/author → unique ID → engine → render → write → print. It runs before
  `net::startup()`. Output on Windows (checkout `nnh`, variable unset):
  `nnh: created my-game/ (12 files; CIA UNIQUE_ID 0x28259)`, the local-engine line, the four set-lines, `next:` with
  `cd`, `nnh run my-game --pc`, `nnh run my-game`. Refusals go to stderr, exit 1 (`nnh: "My Game" can't be a game
  name: only lowercase letters, digits, '_' and '-' (try my-game)`); an engine refusal ends with ": nothing was
  created".
- ctest `nnh_new_files` (1.1 s; `NNHGE_ENGINE_DIR` unset through `ENVIRONMENT_MODIFICATION`): `--version`'s line,
  `help`, exit 2 for an unknown option and no name, exit 1 for "My Game" (with the suggestion), `../escape`, an ID
  outside the range and a folder that is not an engine, with nothing created by any refusal; the twelve files; no
  checkout path in `CMakeLists.txt`; a second `new demo` refused; two runs with the same inputs byte-identical (the
  name aside); two random IDs differ (a third draw before failing). Seen red before `command_new` was wired.
- ctest `nnh_new_smoke` (label `consumer-full`, `RESOURCE_LOCK consumer`, folders under `build/<preset>/consumer/
  new_smoke/`, deleted on success): Windows `pc-debug` 234 s. Local way: `nnh new` 0 s, configure `pc-dev` 16 s,
  `nnh build demo --pc` 72 s (no warning), smoke script 3 s, PASS. Pinned way (archive packed with spec 032's
  `consumer_pack_engine`, URL and SHA-256 pasted, variable unset, the archive really unpacked in `_deps/nnhge-src`):
  configure 23 s, build 72 s, script 3 s, PASS.
- T4.3, Windows, a project made by `nnh new demo` in `E:/RANDOM/n033/demo` (outside the repository), `NNHGE_ENGINE_DIR`
  set, `DEVKITPRO` unset in the shell, no `scripts/dkp.sh`: `nnh build demo` (3ds-dev) 61 s, `nnh build demo
  --release` 60 s (`demo.3dsx` 348,720 bytes), `cmake --build build/3ds-dev --target dist` 9 s (`dist/demo/demo.cia`
  and `.3dsx`), 0 warnings each; `scripts/check_command_length.sh` PASS on both folders.
- T4.4 budgets (AC-5), 7 warm runs interleaved with the repository's `hello` in the same series:

  | Edit | Runs 1–5 | Median (7) |
  |---|---|---|
  | `src/scenes/play_scene.cpp` → simulator (`pc-dev`, the preset `nnh run --pc` uses) | 2.34–2.37 s | 2.36 s |
  | `examples/hello/main.cpp` → simulator (`pc-debug`) | 2.03–2.14 s | 2.06 s |
  | `play_scene.cpp` → `.3dsx` (`3ds-dev`) | 3.54–3.69 s | 3.61 s |
  | `hello` → `.3dsx` (`3ds-dev`) | 3.51–3.60 s | 3.56 s |

  Runs 6 and 7 had outliers (19.1 s, 13.2 s, 5.97 s: another process on the machine) and are left out of the ranges.
  **The simulator budget (< 2 s) is not met on this machine today, by the template nor by `hello`**: `hello` measured
  2.07–2.15 s before any change of this spec (stage 0) and 2.02–2.09 s at spec 032's end. Broken down with
  `NINJA_STATUS=%e` in `pc-dev`: the template's compile 0.79 s (hello's 0.54 s: more headers at `-O2 -g`), the link
  1.2–1.3 s (hello's 1.4 s); the rest is CMake and Ninja start-up. The template adds ≈ 0.25 s of compile over `hello`
  and nothing to the link; the link of a static `-O2 -g` program dominates. Not fixed here (an engine build-speed
  question, not this spec's code): flagged for the maintainer. The `.3dsx` budget (< 5 s) is met.
- Disk of the generated project after the four builds: `pc-dev` 38 MB, `3ds-dev` 43 MB, `3ds-release` 36 MB (116 MB
  in all, the compiler cache warm). `doctor`'s thresholds re-set from it: `FAIL` below 0.1 GB (one PC and one 3DS
  build), `warn` below 0.35 GB (≈ 3× the three folders).

### Stage 5 results (`nnh doctor`'s core)

- `process_run`: Windows `CreateProcessW` (suspended, assigned to a job with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`,
  then resumed, so the time limit also kills what the program started, e.g. `bash -lc`'s children), stdin `NUL`,
  stdout and stderr on one pipe read with `PeekNamedPipe`; elsewhere `posix_spawnp` + `poll` + `SIGKILL`. After the
  program exits, what is left in the pipe is read and the loop stops even if a grandchild still holds the pipe.
  `windows_command_line` quotes like the C runtime (backslashes doubled only before a quote or the closing quote).
- `tool_versions`: `cmake --version` must start with `cmake version `; Ninja's first line is the version; compilers by
  their banner (`Optimizing Compiler Version`, `Apple clang version`, `clang version`, else GCC when the line names
  `gcc`/`g++`/`c++`/`(GCC)`: the version after the last `)`, so Ubuntu's `13.3.0-6ubuntu2~24.04.1` package suffix is
  skipped); numbers of at most 6 digits, 64 KiB read; pacman folders `<name>-<version>-<release>` (`devkitARM-r68-1` →
  `r68`, `devkitARM-gdb-*` and `3ds-cmake-*` not taken for `devkitARM`/`cmake`). Recorded outputs in
  `tools/nnh/tests/data/doctor/` (its README says which are real recordings and which are format samples).
- `doctor_report`: `  <state:6><name:15><found>`, `        fix: <one line>` under every warn/FAIL, a blank line, the
  verdicts; About has no verdict line (`DoctorGroup::verdict`), a group not checked shows only its verdict; the
  not-required groups (About, Optional) say "N problems (not required)"; `--pc` keeps the 3DS checks and says
  "not counted: --pc". Colours wrap only the state word (green/yellow/red).
- `Probes` (`doctor/probes.hpp`): env, is_file, is_dir, read_file (bounded), list_dir, find_program, run (10 s, 64 KiB),
  free_bytes, os_name (`RtlGetVersion`: "Windows 11 (10.0.26100)"; `/etc/os-release`'s `PRETTY_NAME`), and the two
  network probes, `find_console` (the remembered console read from `nnh.ini`, never written, plus the broadcasts, or
  only `--console`) and `fetch` (a uniquely named script in the system's temporary folder, `file(DOWNLOAD ... RANGE_START
  0 RANGE_END 0 ... [TLS_CAINFO])`, removed after). Fakes: `tools/nnh/tests/doctor_fakes.hpp` (`FakeMachine`; the
  network probes count their calls).
- Tests: 323 → 348 (`test_process_run.cpp` 7 incl. the real-probe smoke, `test_tool_versions.cpp` 5,
  `test_doctor_report.cpp` 6, the parsers' mini-fuzz in `test_fuzz_tool.cpp`), the first three files seen red against
  stubs (10 failing tests; the report stub crashed on an empty verdict list). The real-probe case was written after
  `probes_system.cpp` (a smoke of the real machine, not a rule).

### Stage 6 results (`nnh doctor`'s checks)

- Files: `doctor/checks.hpp` (context, one function per group, `run_doctor`), `checks_host.cpp` (About, Simulator, the
  shared helpers, the SDL3 pins), `checks_3ds.cpp`, `checks_optional.cpp`, `checks_project.cpp`, `checks_network.cpp`,
  `project_versions.{hpp,cpp}` (the cache and pin readers, mini-fuzzed), `fix_lines.{hpp,cpp}` (one table per
  problem and host: Windows, Debian-like, other Linux, macOS), `doctor_limits.hpp`, `commands/doctor.cpp`;
  `cmake/nnh_sdl3_pin.cmake` (the SDL3 version, URLs and hashes, included by `nnh_sdl3.cmake`; no behaviour change,
  a unit test compares nnh's URLs with it).
- Refinements, decided here:
  - **Windows `DEVKITPRO` unset is `ok`**, not `warn` (the plan's table): `nnh` configures 3DS presets through
    devkitPro's login shell, which sets it, so nothing `nnh` does fails; the line says so ("C:/devkitPro (DEVKITPRO
    unset: fine, nnh's devkitPro shell sets it)"). A `warn` would have made AC-7's "every required check ok" false on
    the maintainer's machine, where it is unset. `DEVKITPRO=/opt/devkitpro` (what devkitPro's installer sets) is read
    as `C:/devkitPro`.
  - The compiler CMake would pick: `CXX`, else `c++`, `g++`, `clang++`, and `cl` on Windows (`cl` run without
    arguments: it prints its banner).
  - Azahar is only looked for on `PATH`, never run (a GUI program might open a window for `--version`).
  - The name column is 16 wide (`NNHGE_ENGINE_DIR`).
  - The MSYS2 probe is one fixed `bash -lc` text with markers (`nnh-cmake=…`, `nnh-ninja=…`, the `cmake --version` line,
    `nnh-ninja-version=…`): no user text in it.
  - `--online` fetches the SDL3 package this host's first configure needs (MinGW package on Windows, the source archive
    on Linux without a system SDL3 and on macOS, nothing with a system SDL3) and, in a game project, the engine URL of
    `set(nnhge_url "…")` when it is a plain `https://`/`file://` URL (anything a CMake script would read is skipped);
    the CA bundle passed is `CMAKE_TLS_CAINFO`, else Git for Windows' (what `nnh_sdl3.cmake` does).
- Tests: 348 → 382 (`test_doctor_host.cpp` 10, `test_doctor_3ds.cpp` 8, `test_doctor_project.cpp` 9,
  `test_doctor_network.cpp` 6, a mini-fuzz of the project readers), seen red against stub groups (the first project
  tests failed, the rest crashed on missing groups). AC-6's list: CMake 3.20 `FAIL` + `https://cmake.org/download/`;
  Ninja missing `FAIL` + `winget install Ninja-build.Ninja` / `sudo apt install ninja-build`; `DEVKITPRO` unset (Linux)
  `FAIL` + `export DEVKITPRO=/opt/devkitpro`, devkitPro missing + the installer link; MSYS2 without Ninja `FAIL` +
  `C:/devkitPro/msys2/usr/bin/bash.exe -lc "pacman -S --needed cmake ninja"`; no host compiler `FAIL` + the WinLibs
  line; GCC 12 / clang 17 `warn`; engine 0.2.0 ≠ 0.1.0 `warn`; macOS `warn`; exit codes with `--pc`.
- T6.5, fix lines verified: `winget show --id <id> --exact` found `Kitware.CMake`, `Ninja-build.Ninja`,
  `BrechtSanders.WinLibs.POSIX.UCRT`, `Ccache.Ccache`, `Git.Git`; Ubuntu 24.04 packages `cmake`, `ninja-build`, `g++`,
  `g++-13`, `pkg-config`, `libx11-dev libxext-dev libwayland-dev libxkbcommon-dev`, `libasound2-dev libpulse-dev`,
  `ccache`, `git`, `ca-certificates` (the names SDL's Linux README and Ubuntu use); devkitPro's pages
  `https://github.com/devkitPro/installer/releases` (Windows installer) and `https://devkitpro.org/wiki/devkitPro_pacman`
  (Linux, macOS). The paste-able devkitPro form `C:/devkitPro/msys2/usr/bin/bash.exe -lc "pacman …"` was run (with the
  harmless `pacman -Q cmake ninja`) in Git Bash, PowerShell and a cmd batch file: the same output in all three. No
  install or `setx` line was run (they change the machine).
- T6.6, real runs on Windows inside a generated project (`E:/RANDOM/n033/demo`, `NNHGE_ENGINE_DIR` set):
  ```
  About
    ok    nnh              nnh 0.1.0 (protocol 1, link 1, windows-x64, from a source checkout)
    ok    system           Windows 11 (10.0.26100)
  Simulator
    ok    cmake            4.4.2 (C:/Users/…/mingw64/bin/cmake.exe)
    ok    ninja            1.13.2 (C:/Users/…/mingw64/bin/ninja.exe)
    ok    C++ compiler     GCC 16.2.0 (C:/Users/…/mingw64/bin/c++.exe)
    ok    SDL3             downloaded at the first PC configure (SDL3 3.4.20, pinned)
  3DS
    ok    devkitPro        C:/devkitPro (DEVKITPRO unset: fine, nnh's devkitPro shell sets it)
    ok    devkitARM        r68 (C:/devkitPro/devkitARM)
    ok    libctru          2.7.1
    ok    citro2d          1.7.0
    ok    citro3d          1.7.1
    ok    devkitPro shell  C:/devkitPro/msys2 (cmake 4.0.2, ninja 1.13.2)
    ok    host compiler    C:/Users/…/mingw64/bin/g++.exe
  Optional
    ok    ccache           4.13.6 (…/ccache.exe)
    ok    git              2.55.0 (C:/Program Files/Git/mingw64/bin/git.exe)
    warn  Azahar           not found (a 3DS emulator, to try .3dsx files on the PC)
          fix: https://azahar-emu.org/
  This project
    ok    nnh.toml         E:/RANDOM/n033/demo (demo)
    ok    engine           0.1.0 (build/pc-dev/CMakeCache.txt; this nnh: 0.1.0)
    ok    presets          pc-dev, pc-release, 3ds-dev, 3ds-release
    ok    NNHGE_ENGINE_DIR E:/RANDOM/…/spec-033 (NNHGE 0.1.0)
    ok    disk             10.0 GB free

  Simulator: ready
  3DS: ready
  Optional: 1 warning
  Console: not checked (--console <ip> or --online)
  Downloads: not checked (--online)
  This project: ready
  ```
  exit 0, 2.09 s offline (AC-7 Windows). With `DEVKITPRO_MSYS2` at an empty folder: `FAIL devkitPro shell no
  …/fake-msys2/usr/bin/bash.exe` + the installer link, "3DS: not ready (1 problem)", "Simulator: ready", exit 1; with
  `--pc` exit 0 ("not counted: --pc"). With `DEVKITPRO` at a missing folder: `FAIL devkitPro` + the installer link,
  exit 1. (An MSYS2 *without Ninja* could not be made on this machine without uninstalling devkitPro's Ninja: that
  case is covered by `the_devkitpro_shell_needs_cmake_and_ninja_inside`.) `--console 8.8.8.8`: exit 2 before any
  check. `--online` (2.7 s): `ok SDL3 https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-devel-
  3.4.20-mingw.tar.gz` (the only URL; the project has no pin), `FAIL console none answered on the local network` + its
  fix (no console was on), exit 1. AC-8: the listings (names, sizes, times) of the project folder (without `build/`)
  and of `%APPDATA%/nnhge/` were equal before and after `--online`, and no `nnh-doctor-*.cmake` was left in `%TEMP%`.
- `clang-format` (23.1.5) run on the new C++ files; the comments it re-wrapped badly were fixed by hand.
- Stage end: `nnh_tool_tests` 382/382, ctests `nnh_tool`, `nnh_pin_check`, `nnh_new_files`, `nnh_template_smoke`,
  `nnh_script_pc_smoke` green, `pc-debug` 0 warnings; `cmake/` changed, so `3ds-release` and `3ds-dev` rebuilt: 0
  warnings, `hello.3dsx` SHA-256 `92e8dbd2…408f` unchanged.

### Stage 7 results (versions on a protocol refusal)

- `protocol_mismatch.{hpp,cpp}`: `protocol_mismatch_message(side, theirs, ours, nnh_version, project_engine)`; with the
  project's engine known and different: "… use the nnh of NNHGE <x>, or change the project's engine pin to <ours>";
  unknown: "… use the nnh of the game's engine release, or rebuild the game with NNHGE <ours>"; equal to this nnh's
  (a stale game binary): "… rebuild the game (it was built with another engine than this project's)"; a refused
  Hello has no version to name ("the game refused nnh's remote protocol version 1 (nnh 0.1.0; this project's engine:
  0.2.0)…"). `serve_end_text` turns nnh serve's "the console speaks link protocol version N" into the Link message.
- Refinement (T7.2): the versions reach the runner through `ScriptRunner::set_versions(nnh_version, project_engine)`,
  not new `RunnerOptions` members: GCC's `-Wmissing-field-initializers` warns for every designated initializer that
  leaves a member out, and 22 initialisations (run.cpp and the tests) leave them out. `commands/project.cpp`'s
  `name_versions(run, built)` sets them once the game is built (the preset's `CMakeCache.txt` `NNHGE_VERSION`, the
  engine really built), from `run.cpp` and `run_pc.cpp`.
- The Hello's answer and refusal moved to `remote/script_runner_hello.cpp` (39 lines); `script_runner.cpp` 389 → 373.
  A refusal is a `Rejected` record for sequence number 0 (the Hello's) before attaching.
- `link/src/server_session.cpp` (the PC side of NNH Link's handshake, also compiled into the 3DS app's library): "the
  console speaks another protocol version" → "the console speaks link protocol version N"; the console's own text is
  unchanged (question 10).
- Tests: 382 → 389 (`test_protocol_mismatch.cpp` 4, `test_script_runner.cpp` +2 (a game answering version 2, a game
  rejecting the Hello), `test_serve.cpp` +1), each seen red (a stub builder; the runner's old message; a stub
  `serve_end_text`). `nnh_link_tests` 55/55.
- Stage end: `nnh_tool_tests` 389/389, ctests `nnh_tool`, `nnh_script_pc_smoke`, `nnh_template_smoke` green, `pc-debug`
  0 warnings; `link/` changed, so `3ds-release` and `3ds-dev` rebuilt: 0 warnings, `hello.3dsx` unchanged
  (`92e8dbd2…408f`).

### Stage 8 results (mutants, docs, the verbatim walk)

- T8.1, 19 hand mutants of the planned list, one at a time, each judged by `nnh_tool_tests` (filters named), every one
  killed:

  | Mutant | Killed by |
  |---|---|
  | the `..` rule dropped | `names_with_separators_dots_or_drive_letters_are_refused` |
  | the suggestion's `-game` dropped | `a_refusal_suggests_a_valid_spelling_when_one_is_obvious` |
  | an existing empty folder treated as non-empty | `an_existing_empty_folder_is_filled` |
  | a link accepted | `a_link_is_refused_unchanged` |
  | the release pin ignored | `a_release_nnh_pins_its_archive_and_needs_no_variable` |
  | the set-lines printed when the variable already names the folder | `the_set_lines_are_skipped_when_the_variable_already_names_the_folder` |
  | the checkout default not checked | `a_moved_checkout_is_refused_with_a_hint` |
  | the unique-ID range off by one | `unique_ids_are_parsed_in_the_homebrew_range` |
  | CMake's minimum raised to 3.26 / lowered to 3.20 | `cmake_missing_or_older_than_3_25_fails` (both) |
  | GCC's minimum raised to 13 (minimum = checked) | `compilers_below_the_minimum_fail_and_unchecked_ones_warn` |
  | `warn` counted in the exit code | `the_exit_code_counts_only_failures_of_required_groups` |
  | `--pc` ignored | `the_exit_code_counts_only_failures_of_required_groups` |
  | the Downloads group run without `--online` | `no_network_probe_runs_without_the_options` |
  | `DEVKITPRO` unset not failing on Linux | `linux_needs_devkitpro_set_and_says_how` |
  | the MSYS2 cmake/ninja check inverted (`||` → `&&`) | `the_devkitpro_shell_needs_cmake_and_ninja_inside` |
  | the engine-version comparison inverted | `a_fresh_project_is_ready`, `another_engine_version_warns_with_the_fix` |
  | FR-22's message without the project's version | `a_game_answering_another_version_gets_both_versions_and_the_fix`, `a_game_with_another_version_names_both_and_the_fix` |
  | the program time limit ignored | `a_program_past_its_time_limit_is_killed` |

  With stage 2's six (the device rule, the random range, the case-sensitive folder comparison, an unknown placeholder
  copied, the rollback skipped, `_O_EXCL` → `_O_TRUNC`): 25 mutants, 25 killed. The guard inverted in
  `CMakeLists.txt.in` was not killed by a unit test at first (only `nnh_new_smoke` would catch it): the render test now
  checks the guard's exact line.
- T8.2–T8.3 docs: `docs/guides/your-first-game.md` (new), `getting-started.md` rewritten (tools per system with the
  `pacman` line for CMake and Ninja in devkitPro's MSYS2, `nnh` from a checkout, `nnh doctor`, `nnh new`, then "Working
  inside the engine repository"), `nnh-tool.md` (`nnh new`, `nnh doctor` with its groups, report, exit codes and the
  limits table, `--version`, the FR-22 message), the README's quick start, `game-projects.md` (`nnh new` generates it,
  the root layout), `docs/tech/cmake.md` (the template's embedding, the pin cache entries, the four ctests,
  `nnh_sdl3_pin.cmake`, the final pass's new steps), `project-structure.md`, `CLAUDE.md` (summary, layout, build lines),
  `decisions.md` (D56 change-log line), the ROADMAP's budget paragraph. `docs_tool.py check`: 223 files, 0 broken links.
- T8.4, "Your first game" followed on Windows in an empty folder (`E:/RANDOM/n033walk`, deleted after), with the
  worktree as the engine checkout instead of a fresh clone: `cmake --build --preset pc-release --target nnh` (0
  warnings), `PATH` extended, `nnh --version`, `nnh doctor` (exit 0; "This project: not checked" outside a project),
  `nnh new my-game --title "My Game" --author "Me"` (the output the guide shows), `export NNHGE_ENGINE_DIR=…`,
  `nnh run my-game --pc` (first configure downloaded SDL3, the engine built, the window showed "My Game", the sprite and
  both screens' text: 91 s with `NNH_EXIT_AFTER_FRAMES=90`), the smoke script `nnh: PASS (19 lines, frames 1-66)`,
  `kSpeed` changed and rebuilt, `assets/sfx/jump.wav` added with the guide's three lines (builds with 0 warnings, the
  smoke script still passes). One fix to the page: the edit-and-rebuild time measured 2.3–2.6 s through `nnh build`
  (`cmake --build` alone 2.3–2.4 s, `nnh build`'s no-op 0.8 s), so it says "two or three seconds" instead of "about
  two". The keyboard map was corrected (arrows = D-Pad, I/J/K/L = Circle Pad).
- T8.5: `scripts/vm_check.sh final` gains the generated project under `/tmp`: `nnh doctor` with `DEVKITPRO` unset (its
  exit, the `FAIL devkitPro` line and fix, the verdicts) and set (exit and the whole report), `nnh build demo`,
  `--release`, `dist` (the `.cia`), the command-length check on both 3DS folders, and the smoke script in the headless
  simulator; `nnh_new_smoke` already runs with `ctest -L consumer-full`. Not run here (stage 9).

### Console run (stage 10, for the maintainer; not run yet)

From Git Bash on Windows, a project made by `nnh new` outside the repository, the worktree's `nnh` and engine (an Old
3DS XL on the LAN; the maintainer presses Y once per step):

```sh
W=E:/RANDOM/native-nintendo-homebrew-game-engine-nnhge/.claude/worktrees/spec-033
cmake --build --preset pc-release --target nnh        # in $W (or use $W/build/pc-debug/tools/nnh/nnh)
export PATH="$PATH:/e/RANDOM/native-nintendo-homebrew-game-engine-nnhge/.claude/worktrees/spec-033/build/pc-release/tools/nnh"
export NNHGE_ENGINE_DIR=$W
mkdir -p E:/RANDOM/n033console && cd E:/RANDOM/n033console
nnh new demo
cd demo
nnh doctor                                            # expect: every required check ok, exit 0
nnh run demo --script tests/smoke.nnhs                # 3DS: Homebrew Launcher open, press Y -> nnh: PASS, exit 0
nnh install demo                                      # 3DS: FBI -> Remote Install -> Receive URLs over the network
```

Questions after the install: "Does `demo` start from the HOME Menu with the NNHGE default icon and banner, and does the
sprite move with the Circle Pad and the D-Pad, A play the blip and START quit? (yes/no)". Afterwards:
`rm -rf E:/RANDOM/n033console` (and the title can be deleted with FBI).
- T8.6, final Windows guards (2026-11-27): builds `pc-debug`, `pc-release` (full, 257 s), `3ds-dev`, `3ds-release`: 0
  warnings each; `hello.3dsx` (3ds-release) `92e8dbd27aa0d256be55726e9d0d5dbae6de232d34a9783d72647505bdd2408f`,
  unchanged from stage 0; longest 3DS command 2,533 characters (7.7 %) in both 3DS presets, as before; `ctest
  -LE consumer-full -j4`: `pc-debug` 385/385 in 556 s (stage 1: 383 tests, 627 s), `pc-release` 343/343 in 126 s;
  `ctest -L consumer-full -R nnh_new_smoke` (`pc-debug`) passed in 129 s (the compiler cache warm); `nnh_tool_tests`
  389/389 (279 at stage 0). Free on `E:` 9.1 GB.

### What is open (for stage 9 and 10)

- The review and the Linux `final` pass (T9.1): `nnh_new_smoke` and the template's builds on GCC 13/14 and clang 18,
  the real-link case of `a_link_is_refused_unchanged`, the exclusive create on Linux, the new `final` steps (`nnh
  doctor` with and without `DEVKITPRO`, a generated project's 3DS builds). The template's sources were not built with
  GCC 12 or clang 16/17 (they didn't exist at T0.3).
- CMake 3.25 itself was not measured (T0.2): fetching a CMake binary onto the VM was not allowed in this session.
- **Maintainer question (constitution III, AC-5/FR-17):** the simulator edit budget (< 2 s) is missed on this
  machine, like for like in `pc-debug`: the template's edit median 2.48 s, `hello`'s 2.17 s (review round); the link
  of a static program dominates (1.2–1.4 s). Record an exception for 1.0, or open an engine build-speed item (link
  time)?
- AC-7's "MSYS2 without Ninja" was shown with an MSYS2 folder without bash (the unit test covers the Ninja case).
- The console run (T10.1) and the status changes (T10.2).

### Review round (2026-11-27): fixes

The independent review asked for changes (two blockers). Every point fixed in the worktree, test first where it was
code; the commits `1a4902b` (blockers and should-fix code), `0db9805` (nits and docs) and the plan update.

- **Blocker 1, FR-6 with a real write failure:** `create_file` removes a half-written file when a write or the close
  fails (the `NewFs::create_file` contract says so). `create_file_exclusive(path, bytes, fail_after)` is the real
  create with a test hook that makes the real write fail after N bytes. Tests (seen red: the file stayed):
  `a_real_write_failure_after_the_open_leaves_no_file`, `a_write_failing_mid_file_leaves_nothing_behind` (the third
  file fails mid-write: no folder left).
- **Blocker 2:** the FR-22 runner cases moved to `tools/nnh/tests/test_script_runner_hello.cpp`;
  `test_script_runner.cpp` back to 385 lines.
- **3, the time limit:** both loops of `run_program` check the deadline first in every iteration (a program that keeps
  writing is killed; what it wrote past `max_output` is still drained only until the deadline). Test
  `a_program_that_never_stops_writing_is_still_killed_at_its_limit` (a CMake script printing a 64 KiB line forever,
  300 ms): it could **not** be seen red on Windows, where `nnh` reads faster than CMake writes, so the old loop still
  reached its deadline check between writes; it stays as the regression test (Linux runs it at stage 9). Also on
  Windows only the pipe and `NUL` are inherited (`PROC_THREAD_ATTRIBUTE_HANDLE_LIST`, falling back to the old call if
  the list can't be built).
- **4, the rollback's own failures:** `undo` collects what it could not remove; the error then says "could not remove
  X, Y (remove them by hand)", exit 1. Test `what_the_rollback_cannot_remove_is_named` (red first).
- **5, docs:** README's quick start sets `NNHGE_ENGINE_DIR`; getting-started says `--pc` shows the 3DS group without
  counting it.
- **6, `DEVKITPRO_MSYS2`:** cleaned (`\` → `/`, no trailing `/`); with a space in it, the fix is devkitPro's pacman page
  instead of a command. Values in MSYS form (`/d/...`) of `DEVKITPRO`, `DEVKITPRO_MSYS2` and `NNHGE_ENGINE_DIR` get
  "write it in drive form: D:/...". Tests (red first): `the_msys2_folder_is_normalised_and_a_space_gets_the_docs_link`,
  `an_msys_form_path_outside_msys_gets_the_drive_form_hint`, `the_engine_variable_in_msys_form_and_in_the_repository`.
- **7, AC-5 like for like** (`pc-debug` both, a project made by `nnh new` outside the repository configured with the
  parent's SDL3, one warm-up then 7 interleaved runs, edit + `cmake --build --preset pc-debug --target <game>`):

  | Run | 1 | 2 | 3 | 4 | 5 | 6 | 7 | Median |
  |---|---|---|---|---|---|---|---|---|
  | template `play_scene.cpp` | 2.48 | 2.51 | 2.43 | 2.45 | 2.50 | 2.57 | 2.47 | **2.48 s** |
  | `examples/hello/main.cpp` | 2.26 | 2.26 | 2.10 | 2.17 | 2.17 | 2.00 | 2.06 | **2.17 s** |

  Both over the 2 s budget; the template +0.31 s. The after-design Constitution Check now marks III ⚠️ and "What is
  open" carries the question.
- **Nits:** the set-lines single-quoted for Git Bash and PowerShell (`'''`, `''`), `set "X=..."` for cmd, a quoted
  `setx` (test with `( ) ' $` and a backtick); `doctor --online`'s script built by `doctor_fetch_script`, which
  refuses a URL or CA path with a quote, `$`, `;`, a backslash or a control character (test with hostile values), the
  file created exclusively, removed if the write fails, passed as UTF-8; `embed_template.cmake` with one list of
  binary and one of text extensions, failing on any other; the release pin's URL character set equal to doctor's
  (three more `nnh_pin_check` cases, red first); the repository's `NNHGE_ENGINE_DIR` line "not set"; the generated
  `CMakeLists.txt` with one docs line at the top, every line ≤ 120 columns (tested) and the commented `AUTHOR` aligned;
  the template's `.gitattributes` `* text=auto eol=lf`; `CXX="ccache g++"` named ("CXX=... not found ...
  CMAKE_CXX_COMPILER_LAUNCHER"); the OneDrive (reparse point) refusal documented; with `CMAKE_TLS_CAINFO` unset a
  certificate failure gets the docs link, not a no-op `setx`; `<algorithm>` where `std::min` is used; a Linux clone
  path in "Your first game".
- Tests: 389 → 399 in `nnh_tool_tests`.
- Guards after the round (Windows): `pc-debug`, `pc-release`, `3ds-dev`, `3ds-release` 0 warnings; `hello.3dsx`
  `92e8dbd27aa0d256be55726e9d0d5dbae6de232d34a9783d72647505bdd2408f`, unchanged from the stage 0 baseline in this
  worktree; `nnh_tool_tests` 399/399; `ctest --preset pc-debug -LE consumer-full -j4` 385/385 (556 s);
  `nnh_new_smoke` passed (131 s); `docs_tool.py check` 223 files, 0 broken links. The Linux `final` pass that was
  running during this round used the tree synced before it: the fixes above still need a `scripts/vm_check.sh fast`.

### Linux final pass (2026-11-27): the template's smoke script

The final pass (run from the tree before the review round) passed everything but one step: the generated project's
smoke script in the headless simulator, `FAIL at tests/smoke.nnhs:6: expect "play: started": no matching line in frames
1-61`. Reproduced on VM 120: the game logged `[f1 0.000s] play: started` and only then `[f1 0.067s] PC attached`. The
connection to `nnh` completes during frame 1, after the first `update()`; the runner reads log lines only from its
Hello's answer on (D28; `lines_from_before_the_hello_is_answered_are_not_matched` now pins this down), so on a fast
machine the line came too early. Stage 3's refinement (log on the first `update()`) only moved the race. Fix: every
`expect` of the smoke script waits for a line the script's own input causes. The scene logs `play: started` in
`on_enter()` again (for people reading the log) and answers SELECT with `play: running, the sprite at (x, y)`; the
script starts `wait 30`, `tap SELECT`, `expect "play: running" 10` (the consumer fixture's pattern), and waits 10 frames
before `screenshot moved` (the warning nnh gave). The deterministic test
`the_smoke_script_only_expects_lines_its_own_input_causes` (red on the old script) guards it. On VM 120 the generated
project's script then passed three runs in a row (`nnh: PASS (23 lines, frames 1-96)`), and on Windows
`nnh_template_smoke` passes.

`nnh doctor` with `DEVKITPRO` unset on Linux: `FAIL devkitPro /opt/devkitpro found, but DEVKITPRO is not set (the 3DS
presets read it)`, `fix: export DEVKITPRO=/opt/devkitpro`, exit 1: what FR-19, AC-7 and the plan's table ask (the
presets read `$env{DEVKITPRO}`, so a 3DS configure would fail).
