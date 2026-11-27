# Plan 032: Game projects outside the engine repository

Status: approved 2026-11-27 (the maintainer, the twelve questions at the end as recommended, `NNHGE_ENGINE_DIR` included)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decision D55 (proposed,
[decisions.md](../../docs/architecture/decisions.md))
Uses spec 010 and D23 (`nnh build`/`run`/`install`, the games manifest, the MSYS shell for 3DS on Windows), spec 011
(`nnh serve`), spec 031 and D53 (`nnh run`'s hot reload, devkitPro's Ninja called directly), D2 (presets), D27 and D29
(`nnh-assetc` and the data schema programs, built for the host by `ExternalProject` in 3DS presets), and the
[release study's spike](../../docs/plans/2026-11-26-release-1.0.md#spike-results-2026-11-27) (2026-11-27, `3f4b4c8`).
Built in one worktree (branch `spec-032`) created from `main` after spec 031's merge (done: `5b3752c`); no other spec is
built in parallel. Specs 033 and 034 build on the seams this plan keeps: `nnh.toml` and its reader, the
`NNHGE_VERSION*` variables (one file, `cmake/nnh_version.cmake`, for 034's label), the consumer fixture
`tests/consumer/` (033's end-to-end smoke and 034's compatibility check reuse it) and the `consumer-full` ctest label.

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | A game's sources and its `nnh_add_game()` call are the same inside and outside the repository, on PC and 3DS. No path form or system switch is written in a game's `CMakeLists.txt`: `nnh` converts paths for devkitPro's MSYS CMake (FR-15) |
| II 3DS constraint | ✅ | No runtime change: no engine source file changes, the `3ds-release` `hello` `.3dsx` stays byte-identical (FR-17) |
| III Build speed | ⚠️→✅ | Pressure points: (a) the consumer test adds one engine build to every PC `ctest` run (Q4: one way by default, the rest under `consumer-full`; question 8 here); (b) games outside the repository lose the compiler cache today (spike: 2.56–2.68 s against the < 2 s budget). Answered: the engine sets its cache launcher on every target `nnh_add_game()` creates (FR-7), the extras are not configured in a game project (FR-4), budgets measured before and after (stage 0, stage 6, AC-9) |
| IV Event-driven | ✅ | Not concerned (build system and tool only) |
| V No hidden costs | ✅ | No engine code changes; `nnh`'s new readers are bounded |
| VI Spec + test first | ✅ | The consumer fixture is written first and fails today with the spike's errors (red); `nnh`'s root detection, `nnh.toml` reader, preset check and configure command are unit-tested first in `nnh_tool_tests`; the unknown-platform error by a scratch-project test; the command-length check script is first run against today's tree, where it fails (27,354 > 16,383) |
| VII Small files | ✅ | One concept per CMake helper (new `nnh_platform.cmake`, `nnh_version.cmake`, `nnh_game_defaults.cmake`, `nnh_optional_parts.cmake`); four new `nnh` sources ≤ 150 lines each; the consumer tests registered from their own file, not by growing `tests/CMakeLists.txt` |
| VIII Documented API | ✅ | `nnh_add_game()`, the subproject contract, the options, the reserved target names, `nnh.toml` and the local-engine variable are public surface: a new guide page, `cmake.md`, `nnh-tool.md`, getting-started §5 (stage 7) |
| IX Clean and legal | ✅ | The fixture's assets are copies of engine-made test fixtures (`tests/data/…`: `nnhtest.ttf` from `scripts/make_test_fonts.py`, the Aseprite, glTF, map, sound and package fixtures) |
| X Safe by default | ⚠️→✅ | Pressure point: a game now downloads the engine. Answered: the documented recipe pins it by SHA-256 (FR-2, a wrong hash is CMake's error); the engine keeps its own pins; `nnh.toml` is read by a bounded reader (spec 030's TOML-subset reader, already mini-fuzzed) with a mini-fuzz case of its own; `nnh` reads one environment variable and talks to nothing new |
| XI Opt-in, never in the way | ✅ | Inside the repository nothing changes for games (same targets except one internal object library, same option defaults); the four ways of FR-1 are equal |

## Design

### Overview

```
game project/                      engine (subproject: FetchContent, add_subdirectory)
  CMakeLists.txt ── project() ──► FetchContent_MakeAvailable(nnhge) / add_subdirectory(nnhge)
                                    ├─ global property NNH_PLATFORM (pc | 3ds), NNHGE_VERSION* cache entries
                                    ├─ core, platform, host tools (nnh-assetc, data schema support): always
                                    ├─ examples / tests / apps / tools: only if NNH_BUILD_<PART> (default
                                    │  PROJECT_IS_TOP_LEVEL: ON in the engine repository, OFF here)
                                    └─ functions nnh_add_game() & helpers: engine files found through
                                       CMAKE_CURRENT_FUNCTION_LIST_DIR, the platform through nnh_platform()
  game/CMakeLists.txt ── nnh_add_game(...) ──► targets with C++20, no extensions, the cache launcher
  CMakePresets.json (six presets; base maps $env{NNHGE_ENGINE_DIR} to FETCHCONTENT_SOURCE_DIR_NNHGE)
  nnh.toml  ◄── nnh: nearest nnh.toml or engine root; build/<preset>/, build/logs/ under it
nnh (3DS on Windows): bash -lc "cd '/e/game' && cmake --preset 3ds-dev -DNNH_HOST_CXX=/c/…/g++.exe
                                     -DFETCHCONTENT_SOURCE_DIR_NNHGE=/e/…/nnhge"
3DS presets: libnnh_core.a archived from a response file (D55), longest command checked ≤ 50 % of 32,767
```

### Decision D55: the engine is a well-behaved CMake subproject; `nnh` works in a game project marked by `nnh.toml`

Proposed for review (row in [decisions.md](../../docs/architecture/decisions.md)). It records, as one decision (D56 and
D57 stay reserved for specs 033 and 034):

1. **Caller-independent helpers.** Every helper finds the engine's own files through
   `CMAKE_CURRENT_FUNCTION_LIST_DIR` (CMake ≥ 3.17; the minimum is 3.25), read inside the function, never through
   variables set when the file was included (`_NNH_GAME_CMAKE_DIR`, `_NNH_ASSETS_DIR`, `_NNH_CIA_DIR`,
   `_NNH_CTR_TOOLS_DIR`, `_NNH_IMAGES_DIR`, `_NNH_PACKAGE_ART`, `_NNH_DATA_ROOT`, `_NNH_DATA_HOST_SOURCES` disappear).
   The system comes from the global property `NNH_PLATFORM`, set once by the engine's top-level `CMakeLists.txt`,
   read through `nnh_platform(<out> <who>)`, which fails the configure for an empty or unknown value with
   `nnh_add_game(<game>): unknown platform "<value>" (empty) — supported: pc, 3ds`. The engine's own files keep using
   their normal `NNH_PLATFORM` variable (their scope has it). The global property is also the test-only override of
   AC-12.
2. **Extras only at the top level.** `NNH_BUILD_EXAMPLES`, `NNH_BUILD_TESTS` (unit, conformance, fuzz) and the new
   `NNH_BUILD_APPS` (NNH Link) and `NNH_BUILD_TOOLS` (the `nnh` tool, PC presets only) default to the engine's
   `PROJECT_IS_TOP_LEVEL`. The `link/` library is configured only when one of apps, tools or tests is on. A part whose
   option is ON and whose folder is missing (a pruned copy) fails the configure naming the option and the folder. The
   host programs (`nnh-assetc`, the data schema support) are not behind any option.
3. **Game targets get the repository's settings unless the game chose its own.** `nnh_add_game()` sets, on every target
   it creates (the game, `<game>_data_schema_objs`, `<game>_data_schema`): `CXX_STANDARD 20`, `CXX_STANDARD_REQUIRED`
   and `CXX_EXTENSIONS OFF` when the calling scope has no `CMAKE_CXX_STANDARD`/`CMAKE_CXX_EXTENSIONS`, and the
   engine's compiler cache (`NNH_CCACHE`) as `C/CXX_COMPILER_LAUNCHER` when the calling scope has no launcher. Inside
   the repository the calling scope already has all three, so nothing changes there. `NNH_CCACHE` is searched only
   when it is undefined or `*-NOTFOUND`, so `-DNNH_CCACHE=` turns the cache off.
4. **Version in CMake.** `NNHGE_VERSION`, `_MAJOR`, `_MINOR`, `_PATCH` are `CACHE INTERNAL` entries set from
   `project(nnhge VERSION …)` at every configure (visible in every scope, also when the game wraps the fetch in a
   function); one file, `cmake/nnh_version.cmake`, where spec 034 adds its label.
5. **Outputs stay in the game's build folder** (already true: manifest, `dist/`, `_tools/`, `_host/` under
   `CMAKE_BINARY_DIR`/`PROJECT_BINARY_DIR`). The engine prints one status line, `NNHGE <version>: <engine source
   folder>`, so a configure log always says which engine it used.
6. **Game project marker and `nnh`.** `nnh` walks up from the current folder and stops at the nearest folder that holds
   `nnh.toml` (a game project) or `CMakePresets.json` + `engine/core/` (the engine repository). `nnh.toml` is read by
   spec 030's TOML-subset reader: keys `name` and `engine` (text), unknown keys a one-line warning, a malformed file
   `nnh.toml:<line>: reason` and exit 1. A missing preset is reported before CMake runs.
7. **Paths for devkitPro's MSYS CMake.** For 3DS configures on Windows, `nnh` passes `NNH_HOST_CXX` (environment, else
   `g++`, `c++`, `clang++` on its own `PATH`) and the local engine override in MSYS form; the root is already given as
   `cd '/e/…'`. On Linux and in PC presets the command is unchanged.
8. **Local engine: the environment variable `NNHGE_ENGINE_DIR`** (question 1: the approved Q11 option A cannot work,
   see "Local engine override"). The documented presets map it with `"FETCHCONTENT_SOURCE_DIR_NNHGE":
   "$env{NNHGE_ENGINE_DIR}"` in their hidden base preset (empty → the pinned download); `nnh` reads the same variable
   and passes the MSYS form on the command line for 3DS presets on Windows (a command-line `-D` wins over the preset).
9. **The core archive from a response file in 3DS presets** (question 2). `nnh_core`'s sources become the object
   library `nnh_core_objects` (same folder, same flags, same order); the static library `nnh_core` is declared in its
   own folder `engine/core/archive/`, whose scope sets `CMAKE_NINJA_FORCE_RESPONSE_FILE` in 3DS presets: CMake's Ninja
   generator reads that variable per directory, so only the archive step of that one target uses a response file —
   holding relative object paths, which the native `arm-none-eabi-gcc-ar` reads. The archive goes to the same place
   (`engine/core/libnnh_core.a`) with the same members in the same order. `scripts/check_command_length.sh` prints the
   longest command of a build folder and fails above 50 % of 32,767.

Why (recorded with D55): CMake's own guidance for dependencies (`FetchContent`, `FETCHCONTENT_SOURCE_DIR_<NAME>`,
`PROJECT_IS_TOP_LEVEL`); each item is the smallest fix for one finding of the spike (H1–H4, N1–N3), measured; no
global state beyond documented cache entries and two global properties.

### Public surface (sketch)

A game project written by hand (the guide page of stage 7; spec 033 generates the same files):

```cmake
# CMakeLists.txt (the pinned download; the other ways replace the three FetchContent lines)
cmake_minimum_required(VERSION 3.25)
project(mygame)
include(FetchContent)
FetchContent_Declare(nnhge
    URL https://github.com/rafarvns/nnhge/releases/download/v1.0.0/nnhge-1.0.0-src.tar.gz   # spec 034's asset
    URL_HASH SHA256=<64 hex digits>
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nnhge)            # git submodule / copied folder: add_subdirectory(nnhge)
if(NNHGE_VERSION VERSION_LESS 1.0)
    message(FATAL_ERROR "mygame needs NNHGE 1.0 or later (found ${NNHGE_VERSION})")
endif()
add_subdirectory(game)                       # nnh_add_game(mygame SOURCES ... ASSETS assets ...) from any folder
```

```json
// CMakePresets.json: the engine repository's six presets, plus the local-engine mapping and compile commands
{ "name": "base", "hidden": true, "generator": "Ninja", "binaryDir": "${sourceDir}/build/${presetName}",
  "cacheVariables": { "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
                      "FETCHCONTENT_SOURCE_DIR_NNHGE": "$env{NNHGE_ENGINE_DIR}" } }
```

```toml
# nnh.toml (an empty file is valid)
name = "mygame"
engine = "1.0.0"
```

Engine options and cache entries a game may read or set: `NNH_BUILD_EXAMPLES`, `NNH_BUILD_TESTS`, `NNH_BUILD_APPS`,
`NNH_BUILD_TOOLS` (default: ON only at the top level), `NNH_CCACHE`, `NNH_HOST_CXX`, `NNH_ASSETC`, `NNHGE_VERSION*`.
Reserved target names in a game's build (listed in `cmake.md` from the consumer's `--target help` at T7.2): `dist`,
`nnh_assetc`, `nnh_ctr_tools`, `nnh_engine`, `nnh_core`, `nnh_core_objects`, `nnh_core_contract`,
`nnh_platform_<system>`, `nnh_stb_vorbis`, `nnh_ryu`, `nnh_data_schema_support`, `nnh_link` (with apps/tools/tests),
SDL3's targets when built from source, and per game `<game>_assets`, `_icon`, `_3dsx`, `_dist`, `_cia`,
`_data_schema`, `_data_schema_objs`, `_data_description`; the aliases `nnh::*`. Global properties: `NNH_PLATFORM`,
`NNH_ASSETC_PROGRAM`, `NNH_ASSETC_MISSING`.

`nnh` (no new command; FR-11 to FR-15):

```cpp
// tools/nnh/src/project_root.hpp
struct ProjectRoot { std::string path; bool game = false; };   // game: nnh.toml; else the engine repository
ProjectRoot find_project_root(const std::string& start);        // nearest wins; empty path = none
std::string no_project_message();                              // names nnh.toml and the engine repository
// tools/nnh/src/project_file.hpp
struct ProjectFile { std::string name, engine; std::vector<std::string> warnings, errors; };
ProjectFile read_project_file(std::string_view text);           // "nnh.toml:<line>: reason"; 64 KiB bound
// tools/nnh/src/preset_names.hpp
bool presets_define(std::string_view presets_json, std::string_view user_presets_json, const std::string& preset);
// tools/nnh/src/host_compiler.hpp
std::string find_host_cxx(const char* env_value, const std::function<std::string(const char*)>& find_on_path);
// tools/nnh/src/repo.hpp (changed)
struct ConfigureExtras { std::string host_cxx, engine_dir; };   // converted for the shell by configure_command
std::string configure_command(const std::string& root, const std::string& preset, const Shell& shell,
                              const ConfigureExtras& extras);
```

### Events introduced

None (build system and tool only).

### Local engine override (FR-1.2, FR-15; the approved Q11 revisited)

Measured on 2026-11-27 (CMake 4.4.2, temporary folder): a `CMakeUserPresets.json` that defines a preset named like one
of `CMakePresets.json` fails every command with `Duplicate preset: "pc-debug"`, and presets of `CMakePresets.json` may
not inherit from presets of `CMakeUserPresets.json`. So Q11's option A ("the override as a cache variable of the chosen
preset in `CMakeUserPresets.json`") cannot change the six presets `nnh` uses; only presets with other names. Also
measured: a preset's `"FETCHCONTENT_SOURCE_DIR_DEP": "$env{X}"` uses the folder when `X` is set and downloads when it is
empty; `cmake --preset pc-debug -DFETCHCONTENT_SOURCE_DIR_DEP=<other>` overrides the preset's value; native CMake given
an MSYS path (`/tmp/…/dep`, path conversion off) fails (`FetchContent.cmake:2072`), so PC presets need the drive form and
3DS presets the MSYS form — no single spelling, as the spike found.

The design keeps Q11's goals (one spelling, written once outside the game's `CMakeLists.txt`, never in the game's
repository, works with plain `cmake --preset` on PC, `nnh` converts for 3DS) through an environment variable mapped by
the project's own presets (D55 item 8). Question 1 asks the maintainer to confirm it; spec 033 FR-10 (`nnh new
--engine` writing `CMakeUserPresets.json`) then needs a wording change in 033's plan (it would print the line that sets
`NNHGE_ENGINE_DIR`, or write a file `nnh` reads).

### FR-19: the command-line limit, measured and designed

Baseline measured on 2026-11-27 from the existing `build/3ds-dev` and `build/3ds-release` (Ninja's command list,
`ninja -t commands`, whole command lines including the `rm` and `ranlib` around the archive):

| Command (3DS presets, engine repository) | Length | Share of 32,767 |
|---|---|---|
| archive `libnnh_core.a` (447 objects) | 27,354 | 83.5 % |
| archive `libnnh_platform_3ds.a` | 2,533 | 7.7 % |
| link of the largest example | 1,914 | 5.8 % |
| longest compile | 1,245 | 3.8 % |
| out of the tree (spike, `_deps/nnhge-build/` + 17 characters per object) | 35,454 | 108 % (fails) |

PC presets use native CMake, which already switches to response files on Windows (longest `pc-debug` command 5,096).

Options (the spec's notes):
- **A. A response file for the core's archive step only** (D55 item 9). The archive command becomes constant (≈ 300
  characters); the longest 3DS command becomes the platform archive (≈ 2.5 K in the repository, ≈ 3.2 K out of the tree
  with a 64-character project path: ≈ 10 %). Expected byte-identical `hello` (same objects, same members, same order;
  checked: link lines compared before and after). Costs one internal target (`nnh_core_objects`) and one small folder.
  Margin independent of the core's growth.
- **B. Split the core into a few static libraries** (by module group). Each archive shorter, but the margin shrinks again
  as the core grows, link order and cross-library references change (cycles need link groups), and `hello` changes
  (allowed once by the maintainer, but avoidable).
- **C. Archive in chunks** (a custom command calling `ar q` per slice): reimplements CMake's archive rule, loses
  CMake's dependency handling of the archive.
- **D. A shorter engine binary folder** (`BINARY_DIR`): only moves the limit (spike).

**Chosen: A** (question 2), with B as the fallback if T1.2 shows `gcc-ar` or MSYS CMake mis-handling the response file;
then `hello`'s one-time change is checked (sizes, `nm` symbols, disassembly of changed objects) and recorded, and its new
hash becomes FR-17's reference.

The check (`scripts/check_command_length.sh <build dir> [limit]`, default 16,383 = 50 % of 32,767) reads
`ninja -C <dir> -t commands`, prints the longest command's length, its share and its first 100 characters, exits 1
above the limit. It runs on Windows (where the limit bites) at stage ends, in stage 5 on the consumer project at a path
of ≥ 64 characters, and in spec 034's release checklist. A game's own link line (all its objects) is not in a response
file — its library list holds MSYS paths (`-L/opt/devkitpro/…`) the native linker can't read — so a very large game can
still reach the limit; the guide states the size (question 10).

### `nnh` in a game project

- `find_project_root` replaces `find_repo_root` in `build.cpp`, `run.cpp`, `run_reload.cpp` and `serve.cpp`; the four
  messages "run it inside the NNHGE repository" become `no_project_message()`: `nnh: no game project here: no nnh.toml
  (game project) or NNHGE repository (CMakePresets.json + engine/core) in this folder or above it`. Exit 1.
- In a game project `nnh` reads `nnh.toml` once per command (warnings printed, errors → exit 1). Nothing else is read
  from it in this spec (`engine` is shown by 033's `doctor`).
- Before configuring, `nnh` checks that `CMakePresets.json` (or `CMakeUserPresets.json`) defines the preset it chose:
  `nnh: this project has no preset "3ds-dev" (CMakePresets.json): add the six presets (docs/guides/game-projects.md)`,
  exit 1, CMake not run. The reader looks for `"name": "<preset>"` string pairs (bounded: 1 MiB; JSON strings with
  escapes handled; no comments, as CMake allows none).
- `configure_command` builds the configure line for both the first configure and the "game not found, configure again"
  path. For a 3DS preset on Windows: `-DNNH_HOST_CXX=<msys>` when a compiler is found, `-DFETCHCONTENT_SOURCE_DIR_NNHGE=
  <msys>` when `NNHGE_ENGINE_DIR` is set, each single-quoted inside the `bash -lc "…"` string (spaces allowed; a path
  with `'` or `"` is refused with a message). Elsewhere: unchanged (`cmake --preset <p>`). Build commands never carry
  them (the cache keeps them).
- `find_host_cxx`: `NNH_HOST_CXX` from the environment if non-empty, else `g++`, `c++`, `clang++` (each with `.exe` on
  Windows) on `PATH`, as `scripts/dkp.sh`. Called only for 3DS presets on Windows (question 11).

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `CMakeLists.txt` | changed | Platform as a global property too; `NNH_CCACHE` search rule; the four part options with `PROJECT_IS_TOP_LEVEL` defaults; `link/` gate; optional parts through `nnh_optional_parts`; version file; the `NNHGE <version>: <dir>` status line |
| `cmake/nnh_platform.cmake` | new | `nnh_platform(<out> <who>)`: the global property, validated, FR-6's error |
| `cmake/nnh_version.cmake` | new | `NNHGE_VERSION*` cache entries from the project version (034 adds the label here) |
| `cmake/nnh_game_defaults.cmake` | new | `_nnh_game_target_defaults(<target>)`: C++20/extensions/launcher unless the caller chose (FR-7, FR-8) |
| `cmake/nnh_optional_parts.cmake` | new | `nnh_optional_part(<option> <folder>)`: add the folder when ON, fail when ON and missing (FR-4, FR-5) |
| `cmake/nnh_game.cmake` | changed | Platform check first (before any target); `CMAKE_CURRENT_FUNCTION_LIST_DIR`; target defaults on every target it creates |
| `cmake/nnh_assets.cmake`, `nnh_images.cmake`, `nnh_cia.cmake`, `nnh_ctr_tools.cmake`, `nnh_package_rules.cmake`, `nnh_data.cmake`, `nnh_host_tools.cmake` | changed | Directory variables → `CMAKE_CURRENT_FUNCTION_LIST_DIR`; `NNH_PLATFORM` → `nnh_platform()`; data schema targets get the defaults |
| `engine/core/CMakeLists.txt` | changed | `nnh_core` → OBJECT library `nnh_core_objects` (sources, flags, per-file options unchanged); `add_subdirectory(archive)` |
| `engine/core/archive/CMakeLists.txt` | new | Static `nnh_core` from `nnh_core_objects` with the same usage requirements and output folder; response file in 3DS presets (D55 item 9) |
| `tests/CMakeLists.txt` | changed | The fuzz instrumentation names `nnh_core_objects`; one `include(consumer.cmake)` line (the file is not UTF-8: edit with a shell command that keeps its encoding) |
| `tests/consumer.cmake` | new | Registers the consumer tests (labels `consumer`, `consumer-full`) |
| `tests/consumer/` | new | The consumer game project (FR-16): `CMakeLists.txt`, `CMakePresets.json`, `nnh.toml`, `.gitignore`, `game/` (every keyword, every asset kind, self-check, `tests/check.nnhs`), `small/` (a `hello`-sized game for AC-9), `plain/` (a project library for AC-7) |
| `tests/consumer_tools/` | new | `run_consumer.cmake` (the ctest driver: prepare the engine per way, configure, build, run, checks), `compare_flags.cmake`, `plain_only/` (the plain library without the engine), `unknown_platform/` (AC-12 scratch project), `copy_project.sh` (a copy of the fixture at a given path with an engine pin, for stages 5 and 9) |
| `scripts/check_command_length.sh` | new | FR-19's check |
| `scripts/vm_check.sh` | changed | `final` also runs `ctest -L consumer-full` and a 3DS build of a consumer copy with `/opt/devkitpro` |
| `tools/nnh/src/project_root.{hpp,cpp}` | new | Nearest `nnh.toml` or engine root; the message |
| `tools/nnh/src/project_file.{hpp,cpp}` | new | `nnh.toml` keys over `remote::read_tunes` |
| `tools/nnh/src/preset_names.{hpp,cpp}` | new | Does a presets file define a preset |
| `tools/nnh/src/host_compiler.{hpp,cpp}` | new | `find_host_cxx` and the `PATH` lookup |
| `tools/nnh/src/repo.{hpp,cpp}` | changed | `configure_command` with `ConfigureExtras`; `find_repo_root` removed |
| `tools/nnh/src/commands/build.cpp`, `run.cpp`, `run_reload.cpp`, `serve.cpp` | changed | Project root, `nnh.toml`, preset check, configure command |
| `tools/nnh/CMakeLists.txt` | changed | New sources and tests |
| `tools/nnh/tests/test_project_root.cpp`, `test_project_file.cpp`, `test_preset_names.cpp`, `test_configure_command.cpp` | new | AC-6 |
| `tools/nnh/tests/test_repo.cpp`, `test_fuzz_tool.cpp` | changed | Root cases moved; mini-fuzz of `nnh.toml` |
| docs (stage 7) | new/changed | `docs/guides/game-projects.md` (new), `getting-started.md` §5, `nnh-tool.md`, `docs/tech/cmake.md`, `docs/architecture/project-structure.md`, `decisions.md` D55, `CLAUDE.md`, `docs/ROADMAP.md`, `README.md` if it mentions games in the repository |

### Data and memory

No runtime data, no 3DS memory change (`hello` byte-identical). Build side: a game project's `pc-debug` holds the engine's
libraries, SDL3 (prebuilt or source) and the host programs (spike: 606 steps; the 818 MB folder shrinks to the game's
part, measured at T5.6). Disk budget of the implementation on `E:` (12 GB free on 2026-11-27): the worktree's builds
(`pc-debug` 0.8 GB, `pc-dev` 0.6, `pc-release` 0.25, `3ds-dev` and `3ds-release` 0.2 each), the default consumer test
(≈ 0.3 GB per PC preset that runs it), `consumer-full` (four engine builds, ≈ 1.2 GB, deleted by its driver after each
way passes), the stage 5 copies (≈ 0.6 GB, deleted at the stage end). Peak ≈ 4.5 GB; precondition ≥ 10 GB free (the
spec's), checked at T0.0 and before stage 5.

### Platform work

- pc: none in the platform library; the archive and options changes apply to PC presets unchanged (response files are
  forced only in 3DS presets).
- 3ds: the core's archive through a response file; `nnh`'s MSYS forms.
- fake (tests): none.

### Test fixture: `tests/consumer/`

A game project laid out like the guide's: `CMakeLists.txt` (`project(nnh_consumer)`, the engine by the way the cache
variable `CONSUMER_WAY` names — `fetch` (default; URL and hash from `engine_pin.cmake` written by the harness, git-ignored)
or `subdir` (`add_subdirectory(${CONSUMER_ENGINE_SUBDIR})`), then `enable_testing()`, one test of its own, and
`add_subdirectory(game small plain)`), the six presets with the `NNHGE_ENGINE_DIR` mapping, `nnh.toml`, `.gitignore`.

- `game/` — `nnh_add_game(nnh_consumer SOURCES … ASSETS assets IMAGES images/logo.png DATA_SCHEMA data_types.cpp
  LANGUAGES en pt-BR BASE en TITLE … DESCRIPTION … AUTHOR … VERSION 1.2.3 ICON package/icon.png CIA UNIQUE_ID 0xF7032
  PRODUCT_CODE CTR-N-NNHC BANNER package/banner.png BANNER_AUDIO package/banner.wav)` called from this nested folder;
  assets copied from `tests/data/` (a PNG, a Tiled map with its tileset, an Aseprite export, data files of the schema, a
  `.font` over `nnhtest.ttf`, `strings/en.strings` and `strings/pt-BR.strings`, a glTF model, a WAV). One scene loads
  each kind and prints `consumer: PASS <crc32>` (texture sizes, map size and a property, sprite frame count, data values,
  font metrics, both languages' text of one key, mesh vertex count, sound length, the embedded image's size); the same
  line on PC (every way, every flavour) and on the 3DS. `tests/check.nnhs` waits for the line, takes a screenshot and
  quits on START.
- `small/` — `nnh_add_game(nnh_consumer_small SOURCES main.cpp)`: `hello`'s size, for AC-9's budget.
- `plain/` — a static library of the project's own (`-Wshadow` and a definition of its own), compiled once without the
  engine (`tests/consumer_tools/plain_only/`) and once in the consumer; their compile commands must be equal apart from
  paths (FR-8).

The ctest driver (`run_consumer.cmake`, `-P` script) prepares the engine per way, configures with the parent build's
compilers, generator and build type, builds, runs the games headless (`NNH_EXIT_AFTER_FRAMES`, SDL dummy drivers), and
checks: the self-check line; the target list (`ninja -t targets all`: no example, engine test, `nnh-link`, `nnh`,
`nnh_link` target); `ctest -N` lists only the consumer's own test; `nnh_games.txt` lists exactly `nnh_consumer` and
`nnh_consumer_small`; `NNHGE_VERSION*` equal the engine's `project()` version (read from its `CMakeLists.txt`); the plain
library's flags; the game's compile command equal to the parent's `hello` apart from paths and `NNH_GAME_TITLE`
(`compare_flags.cmake`); the launcher on the game's compile command when `NNH_CCACHE` is set (`ninja -t commands`). Ways
that copy the engine (archive, pruned) also list the engine folder (names, sizes, times) before and after the build
(AC-5). Builds use `CCACHE_NOHASHDIR=1` so the engine's objects can hit the cache filled by the parent build (measured
at T3.6; question 8). On Linux the parent's SDL3 download is reused (`FETCHCONTENT_SOURCE_DIR_NNH_SDL3`).

| ctest | Label | Way / what |
|---|---|---|
| `consumer` | `consumer` | local source (`FETCHCONTENT_SOURCE_DIR_NNHGE` = the engine folder), every check above |
| `consumer_platform_empty`, `consumer_platform_unknown` | `consumer` | AC-12: `unknown_platform/` includes the engine's `cmake/nnh_game.cmake` in a `LANGUAGES NONE` project, the property unset or `wii`, `nnh_add_game` → the FR-6 message, no target generated (≈ 1 s) |
| `consumer_archive` | `consumer-full` | `file://` tarball of the engine folder (tracked layout, no `build/`, no `.git`) + its SHA-256; configured with `CMAKE_DISABLE_FIND_PACKAGE_Git=ON` (FR-3) |
| `consumer_archive_override` | `consumer-full` | the same pin + `FETCHCONTENT_SOURCE_DIR_NNHGE` to the unpacked copy: no download step runs |
| `consumer_submodule` | `consumer-full` | `add_subdirectory` of a git clone of the engine's committed `HEAD` (skipped, reported, when the engine folder has no `.git`: the VM's synced tree; question 9) |
| `consumer_pruned` | `consumer-full` | `add_subdirectory` of a copy without `examples/`, `tests/`, `apps/`, `specs/`, `docs/` (FR-5) + AC-5 listing |
| `consumer_bad_hash` | `consumer-full` | wrong SHA-256: configure fails naming the URL, no game binary exists |
| `consumer_options_on` | `consumer-full` | all four options ON (local source, configure only): the targets exist and `ctest -N` lists the engine's tests (AC-13) |

Registered for each PC preset (in its flavour); `TIMEOUT 1200`, `RESOURCE_LOCK consumer` (one engine build at a time).

## Test strategy

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `consumer` (override), `consumer_archive`, `consumer_archive_override`, `consumer_submodule`, `consumer_pruned`: the same self-check line; stage 5 (`nnh` from a copy) |
| FR-2 | `consumer_bad_hash`; SDL3 and makerom/bannertool found under the consumer's build folder (`_deps/`, `_tools/`) at T5.4 |
| FR-3 | `consumer_archive` with `CMAKE_DISABLE_FIND_PACKAGE_Git`; `grep` of the engine's CMake for git (none today) kept in the review checklist |
| FR-4 | `consumer` target list, `ctest -N`, manifest; `consumer_options_on`; the default consumer has maps, sprites, data and fonts with every option OFF (AC-13) |
| FR-5 | `consumer_pruned`; an option ON with its folder missing fails naming both (`consumer_pruned` second configure) |
| FR-6 | `consumer` (nested `game/`, every keyword and asset kind); `consumer_platform_*`; in-repository goldens unchanged (stage 6) |
| FR-7 | `compare_flags.cmake` in `consumer`; launcher check; AC-9 budgets (T5.6) |
| FR-8 | `plain/` vs `plain_only/`; the cache entries the engine adds compared with `cmake.md`'s list (T7.2) |
| FR-9 | AC-5 listings in `consumer_archive`/`consumer_pruned`; `git status --porcelain` of the engine checkout after stage 5's builds |
| FR-10 | `consumer` version check; for the repository's own build, the driver also reads `NNHGE_VERSION*` from the parent build's `CMakeCache.txt` |
| FR-11, FR-12 | `test_project_root.cpp`, `test_project_file.cpp`, mini-fuzz in `test_fuzz_tool.cpp` |
| FR-13 | `test_preset_names.cpp`; stage 5 (`nnh build/run/install/serve` from the copy: `build/<preset>/`, `build/logs/` under it) |
| FR-14, FR-15 | `test_configure_command.cpp` (environment, `PATH`, absent; drive letters upper/lower, spaces; Linux and PC forms unchanged); stage 5 (fresh 3DS configures by `nnh`, the no-compiler failure, the in-repository fresh `nnh build animation`); stage 8 (VM) |
| FR-16 | the fixture and its tests; stage 5 (3DS builds on Windows), stage 8 (Linux), stage 9 (console) |
| FR-17 | stage-end guards and stage 6 (targets, defaults, manifest, goldens, `hello` hash, budgets) |
| FR-18 | stage 7; T7.4 follows the page verbatim in an empty folder |
| FR-19 | `check_command_length.sh` (red at T1.1, green at T1.2), stage 5 on the consumer at a ≥ 64-character path, stage-end guards |

Planned hand mutants (T6.4): the platform check returning early; `pc` accepted as default when empty; a helper reading a
directory variable again; `PROJECT_IS_TOP_LEVEL` replaced by `ON`; the `link/` gate removed; the missing-folder check
removed; the launcher not applied; the standard forced to 20 over a caller's choice; the version not cached; nearest
root replaced by farthest; `nnh.toml` errors ignored; unknown keys refused instead of warned; the preset check skipped;
`NNH_HOST_CXX` in drive form; the override not converted; the response-file variable set in `engine/core/` (compiles
break) or not set (the length check fails). Each must turn a test red (or a stage check) or be argued equivalent.

## Risks and alternatives

1. **The response-file approach misbehaves** (`gcc-ar`'s `@file` through MSYS, CMake writing MSYS paths into the
   response file, the object library changing link order). Signal: T1.2 (archive builds, `hello` hash, link lines).
   Fallback: option B (split the core), with the maintainer's one-time `hello` change.
2. **FR-17's "same targets"**: one internal target, `nnh_core_objects`, is added (question 2). Everything else in the
   target lists must match (T0.1 vs T6.1).
3. **The local-engine variable is machine-wide**: with `NNHGE_ENGINE_DIR` set, every game project on the machine builds
   against that checkout. Mitigation: the engine's status line names its folder at every configure; the guide shows
   setting it per shell; question 1 offers a per-project file instead.
4. **Consumer test cost** (≈ 1–1.5 min per PC `ctest` run on Windows if the cache misses). Mitigation: label `consumer`
   (`ctest -LE consumer` while developing), `CCACHE_NOHASHDIR` measured at T3.6; `consumer-full` only on the VM and in
   the release checklist (question 8).
5. **`https://` from devkitPro's MSYS CMake** (untested) and `file://` URL forms on Windows 3DS. Signal: T0.3 measures
   which `file://` forms both CMakes read. If none works in MSYS, the fetch way on Windows 3DS is checked through the
   override and on Linux by URL (question 12); `https://` waits for spec 034's release candidate.
6. **Windows CMake without CA certificates** fails the game's own `https://` fetch of the engine (the engine's SDL3
   workaround runs only after the engine is added). Documented in the guide (set `CMAKE_TLS_CAINFO` or use the local
   override); spec 033's `doctor --online` detects it.
7. **Disk on `E:`.** Peak ≈ 4.5 GB; drivers delete their builds after passing; stage 5 copies deleted at its end.
8. **A large game's own link line on 3DS/Windows** (question 10): documented; the check script reports it.
9. **`tests/CMakeLists.txt` is not UTF-8**: the one-line change is made with a shell append, never by an editor that
   re-encodes it (the review checks `git diff` shows one line).
10. **Spec 031's `nnh` code** (`run_reload.cpp`, `asset_build.cpp`) is the newest code touched: only the root lookup
    changes; hot reload is re-run from the consumer copy in stage 5 (AC-3's `--script` run with reload on).

Alternatives rejected: a global `CMAKE_NINJA_FORCE_RESPONSE_FILE` (breaks every MSYS compile, spike); `CACHE INTERNAL`
paths for the helpers (works, but leaves state that can go stale between engines; `CMAKE_CURRENT_FUNCTION_LIST_DIR` has
none); `PARENT_SCOPE` version variables (lost when the game wraps the fetch in a function); detecting a game project by
its presets (Q3 B, rejected by the spec).

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | Game sources and calls identical in and out of the repository; paths converted by `nnh`, never written by the game |
| II | ✅ | No engine code change; `hello` byte-identical expected (fallback: one recorded change, maintainer-approved) |
| III | ✅ | Game targets get the cache launcher; extras out of game builds; budgets and clean engine measured before/after; the consumer test's cost isolated by labels |
| IV | ✅ | Not concerned |
| V | ✅ | Not concerned; `nnh`'s readers bounded |
| VI | ✅ | Fixture, scratch-project and `nnh_tool_tests` cases written first; the check script red before the fix; mutants |
| VII | ✅ | Four small CMake helper files, four `nnh` sources, the consumer tests in their own file |
| VIII | ✅ | Guide page, `cmake.md`, `nnh-tool.md`, getting-started §5, verbatim-page check |
| IX | ✅ | Fixture assets copied from engine-made fixtures |
| X | ✅ | Hash-pinned recipe, engine pins kept, bounded and mini-fuzzed `nnh.toml` reader, no new network use |
| XI | ✅ | Repository build unchanged for games; four equal ways |

No constitution exception is requested. D55 is new (status: proposed). Existing decisions touched additively: D2 (game
projects reuse the six presets; the local-engine mapping), D23 (`nnh`'s root is a game project or the repository), D27
and D29 (host programs never gated; found through function-relative paths), D53 (hot reload from a game project).
Flagged for the maintainer: questions 1, 2 and 8, and risks 1, 3 and 5.

## Questions for the maintainer (answered 2026-11-27: each as recommended)

1. **Local engine override: the approved Q11 (A) cannot work** — CMake refuses a `CMakeUserPresets.json` preset with
   the name of a `CMakePresets.json` preset (measured: `Duplicate preset: "pc-debug"`), and project presets can't
   inherit from user presets. Recommended: **an environment variable `NNHGE_ENGINE_DIR`** (drive form, set per machine
   or per shell), mapped by the project's own presets (`"FETCHCONTENT_SOURCE_DIR_NNHGE": "$env{NNHGE_ENGINE_DIR}"`,
   empty → the pinned download), read by `nnh` for the MSYS form on 3DS; it keeps Q11's goals (one spelling, outside
   the game's files, plain `cmake --preset` on PC, `nnh` converts), and the engine's status line shows which engine was
   used. Alternatives: (b) a git-ignored `nnh.local.toml` (`engine_dir = "E:/…"`) that only `nnh` reads and passes as
   `-D` to every preset (per project, but invisible to plain `cmake`); (c) user presets under other names
   (`pc-debug-local`) chosen with `NNH_PRESET` (build folders and `--release` differ from the repository). Either way
   spec 033's plan rewords FR-10 (`nnh new --engine` can't write the six presets into `CMakeUserPresets.json`).
2. **FR-19 by a response file for the core's archive only** (object library `nnh_core_objects` + the static `nnh_core`
   archived in `engine/core/archive/` with `CMAKE_NINJA_FORCE_RESPONSE_FILE` in 3DS presets), accepting one new
   internal target in FR-17's target comparison? Recommended: yes — margin independent of the core's growth (≈ 10 %
   out of the tree instead of 108 %), `hello` expected identical, PC unchanged. Alternative: split the core into a few
   libraries (fallback if T1.2 fails; `hello` changes once as you allowed).
3. **`link/` configured only with apps, tools or tests?** Recommended: yes — no game needs `nnh_link`; FR-4's
   "only what a game needs".
4. **An option ON whose folder is missing fails the configure** (naming option and folder)? Recommended: yes — loud;
   the alternative (skip with a warning) would hide a wrong copy.
5. **Game target defaults only where the game project didn't choose** (C++20 and no extensions only without the
   caller's `CMAKE_CXX_STANDARD`/`CMAKE_CXX_EXTENSIONS`; the launcher only without the caller's launcher)? Recommended:
   yes — FR-7 by default, FR-8 respected (a C++23 game keeps C++23). Alternative: always force the repository's values.
6. **`-DNNH_CCACHE=` (empty) turns the cache off** (the search runs only when undefined or `*-NOTFOUND`)? Recommended:
   yes — Q7's "`NNH_CCACHE` empty" literally (`find_program` would search again on an empty value).
7. **The engine prints `NNHGE <version>: <source folder>` at every configure** (in the repository too)? Recommended:
   yes — one line, says which engine a game used (question 1's risk).
8. **Consumer test runs**: `consumer` and the two platform tests in every PC preset's `ctest` under label `consumer`
   (skip with `ctest -LE consumer` while developing); `consumer-full` in `scripts/vm_check.sh final` (extended) and in
   spec 034's release checklist; builds with `CCACHE_NOHASHDIR=1` to reuse the parent's cache? Recommended: yes (Q4 as
   approved, made concrete).
9. **The "submodule" way tested as a git clone of the committed `HEAD`** (Windows), skipped where the engine folder has
   no `.git` (the VM's synced tree)? Recommended: yes — a submodule is a checkout with a `.git`; the working tree's
   uncommitted changes are covered by the other ways.
10. **A large game's own link line on 3DS/Windows**: document its size limit (≈ the number of objects at which the
    link reaches 50 %, measured at T5.5) and leave it to the check script? Recommended: yes — a response file there
    would carry MSYS library paths the native linker can't read; no current game is near it.
11. **`NNH_HOST_CXX` passed only for 3DS presets on Windows** (where devkitPro's shell has no compiler); on Linux CMake's
    own search, the command unchanged? Recommended: yes — AC-6's "unchanged forms on Linux", `dkp.sh` parity.
12. **The pinned-download way on Windows 3DS**: checked by `file://` only if T0.3 finds a form devkitPro's MSYS CMake
    reads, else through the override (and by URL on Linux, VM); `https://` from MSYS CMake left to spec 034's release
    candidate? Recommended: yes — no local web server in the tests.

## Implementation results

### Stage 0 results (2026-11-27, Windows; worktree `spec-032` at `7e08303`)

- **T0.0** `df -h /e`: 12 GB free at the start, 10 GB after the four baseline builds. The spike's findings hold in the
  tree: the eight directory values (`_NNH_GAME_CMAKE_DIR`, `_NNH_ASSETS_DIR`, `_NNH_CIA_DIR`, `_NNH_CTR_TOOLS_DIR`,
  `_NNH_IMAGES_DIR`, `_NNH_PACKAGE_ART`, `_NNH_DATA_ROOT`, `_NNH_DATA_HOST_SOURCES`) are set at file scope in `cmake/`,
  `NNH_PLATFORM` is read as a variable by `nnh_game`, `nnh_assets`, `nnh_images`, `nnh_data` and `nnh_host_tools`; the
  `find_repo_root` callers are `build.cpp`, `run.cpp`, `run_reload.cpp` and `serve.cpp`.
- **T0.1** Fresh `pc-debug`, `pc-release`, `3ds-dev`, `3ds-release` (0 warnings each; full builds 5 m 30 s, 5 m 50 s,
  2 m 05 s, 2 m 01 s). Target lists 183 (PC) / 182 (3DS) names, `NNH_*` cache entries and the 27 games of
  `nnh_games.txt` kept in `build/ref-032/base/` (not committed). **`hello` reference:** 3ds-release `hello.3dsx`
  247,628 bytes, SHA-256 `b0b243ddfe6f94cc932c771f32c2d7a9ac14945561ad996eae4bc2b6873670c3`; stripped `hello.elf`
  SHA-256 `91680297f5395b6b32d41a5bb6ec21ae5869fc61f8062ef5bba1d4f260a73531` (`size`: text 202,204, data 28,360,
  bss 107,368); pc-release `hello.exe` `.text` 0x189a0. `libnnh_core.a`: 447 members. The `hello` link commands
  (both 3DS presets, pc-debug, pc-release) and `nnh_tests`' kept for T1.2.
  **Pre-existing failure found:** `ctest --preset pc-debug` at the base commit fails `unit` and `unit_reverse` on
  three `test_door_run.cpp` cases: commit `fd688f3` made the door run judge 60 Hz only in Development builds
  ("not judged (Debug)") without changing its tests. Fixed in stage 0 as a test-only change (the expected verdict
  follows `nnh::build::flavor()`); every other test passed (380 tests, 9 m 21 s with `-j8`).
- **T0.2** Longest 3DS commands (`scripts/check_command_length.sh`, written at T1.1): 27,354 characters (83.5 %) in
  `3ds-dev` and `3ds-release`, the `libnnh_core.a` archive. `hello` one-file edit: pc-debug simulator 2.22 / 1.91 /
  1.95 s; 3ds-dev `.3dsx` 3.81 / 3.57 / 3.66 s. Clean engine (`nnh_core` + platform, `ninja -t clean`): pc-release
  19.4 s (ccache), 3ds-release 48.1 s (devkitPro's MSYS2 has no ccache; the 447-object core). Fresh configures:
  pc-debug 25.6 s, 3ds-dev 24.4 s. pc-release stands in for pc-dev (one PC build folder less on a tight disk).
- **T0.3** (temporary folder outside the repository, deleted): `file://` URL forms for `FetchContent` with
  `URL_HASH`: native CMake 4.4.2 reads `file://E:/x/y.tar.gz` and fails `file:///E:/...` and `file:///e/...`
  ("File not found: /E:/..."); devkitPro's MSYS2 CMake 4.0.2 reads `file://E:/...`, `file:///e/...`, `/e/...` and
  `E:/...`, and fails `file:///E:/...`. So the tests use `file://` + the path as CMake writes it (`file://E:/...` on
  Windows, `file:///tmp/...` on Linux), which both CMakes read (question 12: the pinned way is checked by `file://` on
  Windows 3DS too). In MSYS2 CMake, a preset's `"FETCHCONTENT_SOURCE_DIR_DEP": "$env{X}"` uses the folder when `X` is
  set, and `cmake --preset x -DFETCHCONTENT_SOURCE_DIR_DEP=<other>` overrides it, as in native CMake.
  `arm-none-eabi-gcc-ar qc lib.a @list.rsp` from MSYS2 bash reads a response file of relative paths (members `a.o b.o`)
  and refuses MSYS absolute paths (`/e/.../a.o: No such file or directory`): the archive's response file must hold
  paths relative to the build folder, which CMake's Ninja generator writes.

### Stage 1 results (FR-19)

- **T1.1** `scripts/check_command_length.sh` red on the base tree: 27,354 characters (83.5 %), exit 1, in `3ds-dev` and
  `3ds-release`; exit 0 with the limit 32,767; exit 2 on a folder without `build.ninja`.
- **T1.2** Option A works: `nnh_core_objects` (OBJECT, every source, per-file option and usage requirement as before)
  and `engine/core/archive/` (static `nnh_core`, alias `nnh::core`, archived to `engine/core/libnnh_core.a`;
  `CMAKE_NINJA_FORCE_RESPONSE_FILE` only when `NNH_PLATFORM` is `3ds`). The archive command became
  `arm-none-eabi-gcc-ar qc engine/core/libnnh_core.a @CMakeFiles/nnh_core.rsp` (≈ 250 characters; the response file
  holds paths relative to the build folder, which the native archiver reads). Longest 3DS command after: **2,533
  characters (7.7 %)**, the platform archive, in both 3DS presets (before: 27,354, 83.5 %). `hello` unchanged:
  `.3dsx` and stripped `.elf` SHA-256 equal to T0.1's, `nm -C` list equal, pc-release `.text` 0x189a0; the `hello` link
  commands (3ds-dev, 3ds-release, pc-debug, pc-release) and `nnh_tests`' identical; target lists equal to T0.1's apart
  from `nnh_core_objects`; cache entries and games equal. 0 warnings in the four presets. Fallback B (T1.3) not needed.
- **T1.4** Stage-end guards: ctest pc-release 338/338, pc-debug 379/379 (one disabled); `check_command_length.sh` PASS
  on both 3DS folders.

Stage 1 refinements:
- **Archive member order.** `libnnh_core.a` has the same 447 members, but the two vendored objects (`stb_vorbis.o`,
  `f2s.o`, object libraries the core links) moved from the first two places to the last two: CMake puts the objects
  of linked object libraries after `$<TARGET_OBJECTS:...>` sources. Members' order inside an archive with a symbol
  index does not change what the linker takes (the `hello` hashes prove it), so the order is accepted rather than
  forced with a link-order trick.
- **`scripts/check_release_strings.sh` on `hello.elf`** fails at the base commit too (`dev_pause`: the `.elf`'s DWARF
  names `dev_pause.hpp`; the script's own notes say to check the `.3dsx` or a stripped `.elf`); the stage-end guard runs
  it on `hello.3dsx` (ok). The `pc-release` `hello.exe` form reports `NNH_REMOTE` at the base commit as well (same
  `.text`). Both are pre-existing and left to the maintainer.

### Stage 2 results (the consumer skeleton and the CMake contract)

- **T2.1** `tests/consumer/` (skeleton: `game/` with `SOURCES`, `ASSETS` (a PNG, a data file) and `DATA_SCHEMA` in a
  nested folder, `small/`, `plain/`, the six presets with the `NNHGE_ENGINE_DIR` mapping, `nnh.toml`, `.gitignore`), the
  driver `tests/consumer_tools/run_consumer.cmake` (+ `consumer_engine.cmake`, `compare_flags.cmake`, `plain_only/`),
  `tests/consumer.cmake` and one appended `include()` line in `tests/CMakeLists.txt` (`git diff` shows that line only;
  the file keeps its encoding). Red against the stage 1 engine (a `git archive` of `HEAD` as `ENGINE`): the configure
  fails at the fixture's version check (`NNHGE_VERSION is "" (..)`), before the spike's `/tools/data_schema` error.
- **T2.2** `cmake/nnh_platform.cmake` + the global property; `consumer_platform_empty` and `consumer_platform_unknown`
  (≈ 1 s each) pass: `nnh_add_game(probe_game): unknown platform "" (empty, supported: pc, 3ds)` / `"wii" (supported:
  pc, 3ds)`, no `build.ninja`. The first run of the empty case caught a real bug of the helper: `get_property` of an
  unset property leaves the variable undefined, so `if(value STREQUAL "")` compared the literal `value`; now quoted.
- **T2.3** No file-scope directory variable left in `cmake/` (`_NNH_*_DIR`, `_NNH_PACKAGE_ART`, `_NNH_DATA_*`: none);
  the helpers read `CMAKE_CURRENT_FUNCTION_LIST_DIR` inside their functions. The regenerated `3ds-dev` `build.ninja`
  differs from stage 1's only in the regeneration rule's list of CMake files (8 lines): no helper path changed.
- **T2.4** The four options (`NNH_BUILD_TOOLS` PC only), `nnh_optional_part`, the `link/` gate, `enable_testing()` only
  with tests. `consumer` checks: no example, engine test, NNH Link, `nnh` or `nnh_link` target; `ctest -N` lists only
  `consumer_own_test` although the project calls `enable_testing()`; `nnh_games.txt` lists `nnh_consumer` and
  `nnh_consumer_small`. In the repository: target lists equal to T0.1's apart from `nnh_core_objects`; games equal; the
  cache gains `NNH_BUILD_APPS` (and `NNH_BUILD_TOOLS` on PC) and `NNHGE_VERSION*`, the option defaults are unchanged
  (ON).
- **T2.5** `cmake/nnh_game_defaults.cmake`; `NNH_CCACHE` searched only when undefined or `*-NOTFOUND`. `consumer`
  compares `nnh_consumer_small`'s compile command with the parent's `hello` (ccache launcher, `-DNNH_FLAVOR_DEBUG`, the
  engine's include folder, `-g -std=c++20`): equal once the build folder, `NNH_GAME_TITLE` and the `-MT`/`-MF`
  dependency names are taken out; the project's `consumer_plain` compiles exactly as in `plain_only/` (no engine); a
  second configure with `-DCMAKE_CXX_STANDARD=23 -DNNH_CCACHE=` keeps `-std=c++23` and no launcher on the game.
- **T2.6** `cmake/nnh_version.cmake`: the fixture checks `NNHGE_VERSION*` against `CONSUMER_EXPECT_VERSION` (the
  engine's `project()` version read by the driver) and the driver reads the parent's `CMakeCache.txt`. Status line
  `-- NNHGE 0.1.0: <engine folder>` at every configure.
- **T2.7** `consumer` green on pc-debug and pc-release (skeleton line `consumer: PASS AD8A1613`; first cold run ≈ 2 min,
  an incremental rerun 15–28 s). Guards: 0 warnings in the four presets; `hello.3dsx` and stripped `.elf` SHA-256
  unchanged; longest 3DS command 2,533; ctest pc-release 341/341 and pc-debug 382/382 (`-LE consumer-full`; the tree also held the stage 4 `nnh` changes).

Stage 2 refinements:
- **The unknown-platform message** is ASCII and keeps the plan's parts: `nnh_add_game(<game>): unknown platform
  "<value>" (supported: pc, 3ds)`, and `"" (empty, supported: pc, 3ds)` for an empty value (no em dash: one less
  encoding question in Windows consoles and regular expressions).
- **The self-check line goes to stdout too.** Release builds compile `NNH_LOG_INFO` out, so the consumer test (which runs
  in every PC flavour) read no line in pc-release; `CheckScene` prints the line with `printf` as well, and runs the check
  again on SELECT (the console script's trigger, so the line comes after the remote attached).
- **SDL3 reuse.** The driver passes the parent's SDL3 download as `FETCHCONTENT_SOURCE_DIR_NNH_SDL3(_PREBUILT)` when the
  parent fetched one (Linux; MinGW without a system SDL3); the maintainer's machine has a system SDL3, found as before.
- `nnh_assetc_command()` (called inside `nnh_add_assets`) and the data helpers also read the platform through
  `nnh_platform()`; `nnh_host_tools_setup()` runs in the engine's own scope and keeps the variable.

### Stage 3 results (the full fixture and the four ways)

- **T3.1** `tests/consumer/game/` calls `nnh_add_game()` with every keyword (`SOURCES`, `ASSETS`, `IMAGES`,
  `DATA_SCHEMA`, `LANGUAGES en pt-BR`, `BASE`, `TITLE`, `DESCRIPTION`, `AUTHOR`, `VERSION 1.2.3`, `ICON`, `CIA
  UNIQUE_ID 0xF7032 PRODUCT_CODE CTR-N-NNHC BANNER BANNER_AUDIO`) and ships one asset of each kind, copied from
  `tests/data/` (`art/hero.png`, `levels/good.tmj` + tileset, `sprites/good.ase.json`, `things/lamp.thing`,
  `fonts/body_16.font` over `nnhtest.ttf`, `strings/en.strings` + `pt-BR.strings`, `models/vertex_colors.gltf`,
  `sfx/blip.wav`, `images/logo.png` (16×8), `package/` icon, banner and jingle). Every asset step runs on PC; a
  `3ds-dev` configure of the fixture generates `nnh_consumer.elf`, `_3dsx`, `_dist`, `_cia`, `dist` and the data schema
  targets (and `nnh build` built the `.3dsx`, stage 4).
- **T3.2** The self-check (textures' sizes, the map's size and `lives`, the sprite library's frames and clips, the data
  values, the font's metrics and an advance, both languages' code and name and the base text of one key, the mesh's
  vertices, the sound's length) prints **`consumer: PASS D99E1764`** in pc-debug and pc-release and in every way below;
  changing `value = 321` to `322` in `things/lamp.thing` made it `D8176E32` (then put back). `game/tests/check.nnhs`
  taps SELECT and expects that line.
- **T3.3** `plain/` + `consumer_tools/plain_only/`: equal compile commands (stage 2).
- **T3.4** The other ways (label `consumer-full`), each with the same line, each folder deleted once it passed:

  | ctest | Time (Windows pc-debug) | Configure | Notes |
  |---|---|---|---|
  | `consumer` (local) | 76 s cold, 23 s warm | ≈ 15 s fresh, 2–4 s again | 595 ccache hits, 0 misses (cold) |
  | `consumer_archive` | 180 s | 24 s | `file://E:/...` tarball + SHA-256, `CMAKE_DISABLE_FIND_PACKAGE_Git=ON`; `_deps/nnhge-src` listing unchanged by the build |
  | `consumer_archive_override` | 177 s | 16 s | no `nnhge-subbuild` (nothing downloaded); the unpacked copy unchanged |
  | `consumer_submodule` | 131 s | 15 s | `git clone` of the committed `HEAD`; the clone unchanged |
  | `consumer_pruned` | 144 s | 14 s | no `examples/ tests/ apps/ specs/ docs/`; then `-DNNH_BUILD_EXAMPLES=ON` fails naming the option and `examples/` |
  | `consumer_bad_hash` | 43 s | – | fails: `SHA256 hash of .../nnhge.tar.gz does not match expected value`; nothing unpacked or built |
  | `consumer_options_on` | 21 s | 19 s | `hello`, `nnh_tests`, `nnh-link`, `nnh`, `nnh_tool_tests` exist; `ctest -N` lists the engine's tests |

- **T3.5** `consumer_options_on` as above (configure only).
- **T3.6** Cost on Windows pc-debug: the default `consumer` costs 76 s on a cold folder with `CCACHE_NOHASHDIR=1` (every
  engine object a direct ccache hit from the parent build) and 23 s when its folder exists; without `CCACHE_NOHASHDIR` a cold run took 139 s (568 misses: Debug
  objects hash the build folder), so the driver sets it. The
  other ways copy or clone the engine to another folder, so their compiles miss the cache (paths differ): 2–3 minutes
  each, ≈ 13 minutes for `consumer-full`. Linux reuses the parent's SDL3 download through
  `FETCHCONTENT_SOURCE_DIR_NNH_SDL3` (wired; checked by the VM pass).

Stage 3 refinements:
- **A wrong hash of a local archive** is reported by CMake as `SHA256 hash of <file> does not match expected value`
  (a download says `HASH mismatch`): both name the archive; the check accepts either.
- **The pseudo-language `qps`** ships in Debug/Development (spec 029), so the self-check looks `en` and `pt-BR` up with
  `loc::find` instead of counting languages.
- **The submodule way clones the committed `HEAD`**: it checks the last commit, not the working tree (the other ways
  check the working tree). Skipped with a message where the engine folder has no `.git` (`SKIP_REGULAR_EXPRESSION`).
- **Golden in two places:** `tests/consumer.cmake` (`_consumer_line`) and `game/tests/check.nnhs`.
- **T3.7** Stage-end guards: stage 3 changed only `tests/` (no engine or CMake helper), so the 3DS builds and `hello`
  stay those of stage 2; `consumer` and the platform checks pass in pc-debug and pc-release with the golden; the full
  ctest runs at stage 4's end on the same tree.

### Stage 4 results (`nnh` in a game project)

- **T4.1** `tools/nnh/src/project_root.{hpp,cpp}` (29 lines): nearest `nnh.toml` or engine root; a folder with both
  markers is a game project. `test_project_root.cpp` (7 cases: the engine repository and below it, a game project and
  below it, a game project inside the repository (and the repository again above it), both markers, the real repository
  from the tool's sources, nothing found under the system's temporary folder, the message). `test_repo.cpp`'s root case
  moved there; `find_repo_root` removed.
- **T4.2** `project_file.{hpp,cpp}` over `remote::read_tunes` (`nnh.toml:<line>: reason`); `test_project_file.cpp`
  (empty and comment-only files, `name`/`engine`, an unknown key and a key in a table each one warning line, malformed
  lines, a key set twice, > 64 KiB refused) and the mini-fuzz `project_file_target_survives_mutated_inputs` (6,000
  mutations: every error and warning starts with `nnh.toml:`).
- **T4.3** `preset_names.{hpp,cpp}`: a bounded JSON reader (1 MiB, 64 levels) that records only the `name` of objects
  directly in the top-level `configurePresets`/`buildPresets` arrays; a preset counts when both a configure and a build
  preset have the name, in `CMakePresets.json` or `CMakeUserPresets.json`. `test_preset_names.cpp` (the repository's own
  file, a style copy, a user file, escaped strings, a preset only in `buildPresets`, names outside the arrays or nested
  in a preset, malformed and too deep JSON, > 1 MiB) and the mini-fuzz `presets_target_survives_mutated_inputs`.
- **T4.4** `host_compiler.{hpp,cpp}` (`find_host_cxx`: `NNH_HOST_CXX`, else `g++`, `c++`, `clang++` on `PATH` with
  `.exe` on Windows) and `repo.{hpp,cpp}` `configure_command` + `ConfigureExtras`; `test_configure_command.cpp`
  (environment first, the `PATH` order, none; Windows 3DS: `C:\...` and `c:/...` to `/c/...`, spaces kept inside single
  quotes, the override as `/e/...`; a `'` or `"` in any of the three paths refused; Linux 3DS and PC presets unchanged).
- **T4.5** `commands/project.cpp` (`open_project`, `check_preset`, `project_configure_command`) used by `build.cpp`
  (both configure paths), `serve.cpp`; `run.cpp` and `run_reload.cpp` use `find_project_root`. Checked by hand: outside
  any project `nnh build hello` prints the FR-11 message, exit 1; a project with `nnh.toml` and only `pc-dev` presets:
  `nnh: this project has no preset "3ds-dev" (CMakePresets.json): ...`, exit 1, CMake not run, and the unknown key
  warned; `name =` → `nnh: nnh.toml:1: expected "name = value"`, exit 1. From `tests/consumer/game/` (a game project
  inside the repository, `NNHGE_ENGINE_DIR` set): `nnh build nnh_consumer` configured `tests/consumer/build/3ds-dev`
  (the command carried `NNH_HOST_CXX` and the override in MSYS form) and built `nnh_consumer.3dsx` (514 KB; longest
  command 3,222 characters, 9.8 %), and `NNH_PRESET=pc-debug nnh build nnh_consumer --pc` configured and built
  `tests/consumer/build/pc-debug/game/nnh_consumer.exe` (that folder deleted afterwards). `nnh_script_pc_smoke` passes.
- **T4.6** `nnh_tool_tests`: 271 cases, 0 failures (36 in the touched filters). Stage-end guards: 0 warnings;
  ctest pc-debug 382/382 and pc-release 341/341 (`-LE consumer-full`); the 3DS presets have no `nnh` code (unchanged
  since stage 2).

Stage 4 refinements:
- **`NNH_PRESET=pc-debug nnh build nnh_consumer`** (T4.5's wording) builds the 3DS game: `nnh build` without `--pc`
  targets the console and `NNH_PRESET` only picks among presets of the same system; the PC check uses `--pc`.
- The engine override is passed only when the project is a game project (an `nnh.toml` at its root): the engine
  repository's own configure never gets an unused `FETCHCONTENT_SOURCE_DIR_NNHGE`.
- `preset_names.cpp` is 174 lines (the plan's sketch said ≤ 150 for the new `nnh` files): one class, the JSON reader;
  within the 200-line aim.
- The red step of these `nnh` tests was not observed separately (the sources were written alongside them while the
  baseline builds ran); the mutation pass (T6.4) covers that they can fail.

### Stage 5 results (Windows end to end, outside the repository)

- **T5.1** `tests/consumer_tools/copy_project.sh <dest> [pin|local]` (+ `pack_engine.cmake`): a copy of the fixture;
  `pin` packs this engine folder as `<dest>/../nnhge-engine/nnhge.tar.gz` (5.5 MB) and writes `engine_pin.cmake` with
  its `file://E:/...` URL and SHA-256. Copy at `E:/RANDOM/nnhge-032-checks/a-game-project-folder-with-a-deliberately-long-name`
  (78 characters).
- **T5.2** From the copy, `nnh` of the worktree's pc-debug, fresh build folders, no `scripts/dkp.sh` run:
  `nnh build nnh_consumer` (3ds-dev) 113 s and `--release` 104 s, both from the pinned archive, 0 warnings: `.3dsx` 527
  KB / 457 KB; the cache holds `NNH_HOST_CXX=/c/.../g++.exe` passed by `nnh`. `nnh run nnh_consumer --pc --script
  game/tests/check.nnhs` (pc-dev): the expect matched (`consumer: PASS D99E1764` at frame 31), screenshot read back
  (the PASS banner and the line), log under `<copy>/build/logs/`; the first run failed at the script's last line
  because the script pressed START before `end` (the game closed first): the fixture's script no longer quits
  (`nnh` closes the game at the end), then exit 0. With the pin file removed and `NNHGE_ENGINE_DIR=E:/...` (drive
  form): `NNH_PRESET=pc-debug nnh run ... --script` exit 0 (`-- NNHGE 0.1.0: E:/...`), `nnh build nnh_consumer`
  (3ds-dev) 97 s (`-- NNHGE 0.1.0: /e/...`: `nnh` converted the override). Both manifests list only `nnh_consumer` and
  `nnh_consumer_small` (what `nnh serve` offers). With `PATH=/c/Windows/system32` and no `NNH_HOST_CXX`, a fresh
  `3ds-debug` configure through `nnh` fails: `This game has ... which need nnh-assetc: no host C++ compiler: set
  NNH_HOST_CXX (environment or -D), or -DNNH_ASSETC=<prebuilt nnh-assetc>`, `nnh: configuring 3ds-debug failed`.
- **T5.3** In the worktree, `build/3ds-dev` deleted, then `nnh build animation`: configured by `nnh` (host compiler
  passed) and built in 112 s, 0 warnings.
- **T5.4** After the builds the worktree has no new or changed file but this stage's own sources; the copy's pc build
  fetched SDL3's pinned MinGW package into `<copy>/build/pc-*/_deps/nnh_sdl3_prebuilt-src`, and `cmake --build
  build/3ds-dev --target dist` (10.5 s) fetched makerom and bannertool into `<copy>/build/3ds-dev/_tools/` and made
  `dist/nnh_consumer/nnh_consumer.3dsx` and `.cia`.
- **T5.5** Longest commands of the copy: 3,145 characters (9.6 %) in `3ds-dev` and `3ds-release` (the platform archive
  under `_deps/nnhge-build/`; the core's archive from its response file). A game's own link command holds its objects
  relative to the build folder (the project's path doesn't count): 788 characters with 6 objects, about 50 characters
  per object (`game/CMakeFiles/<game>.dir/src/<file>.o`), so a game reaches the 50 % margin at roughly 300 source files
  (question 10; the guide says so).
- **T5.6** One-file edit of `small/main.cpp` (3 runs): simulator (pc-debug) 2.94 / 2.14 / 1.94 s, `.3dsx` (3ds-dev)
  3.57 / 3.58 / 3.62 s; the repository's `hello` at the same time 2.69 / 1.86 / 1.88 s, so the game project is on par
  with the repository (the first run of each series pays a cold file cache; the PC budget of < 2 s is borderline on this
  machine for both, as at T0.2). `ninja -v` shows `ccache.exe` on the game's compile. Configure (fresh / again): local
  way 17.5 / 1.8 s, pinned archive 22.0 / 2.8 s. Build folders: pc-debug 164 MB, pc-dev 206 MB, 3ds-dev 59 MB,
  3ds-release 72 MB. The copy was deleted afterwards (E: 9.7 GB free).

### Stage 6 results (the repository unchanged)

- **T6.1** After stages 1–5, fresh builds of all four presets (0 warnings): target lists equal to T0.1's apart from
  `nnh_core_objects`; games equal; cache entries as listed at T2.4. `hello`: `.3dsx` SHA-256 `b0b243dd…70c3`, stripped
  `.elf` `91680297…a531` (both equal to T0.1's), `nm -C` equal, pc-release `.text` 0x189a0; the `hello` and `nnh_tests`
  link commands equal; `check_release_strings.sh` ok on `hello.3dsx`; longest 3DS command 2,533. Every example's
  `--pc` check script runs in ctest (the `*_script_pc` tests, goldens unchanged): pc-debug 382/382 at stage 4's end,
  rerun at stage 7's end with the docs.
- **T6.2** (3 runs) `hello` edit → simulator (pc-debug) 2.09 / 2.06 / 2.02 s (T0.2: 2.22 / 1.91 / 1.95), → `.3dsx`
  (3ds-dev) 3.62 / 3.61 / 3.53 s (3.81 / 3.57 / 3.66); clean engine pc-release 19.1 s (19.4), 3ds-release 47.2 s (48.1);
  fresh configure pc-debug 19.9 s (25.6), 3ds-dev 23.1 s (24.4). No measurable change.
- **T6.3** `docs/ROADMAP.md` baseline table: a "Spec 032" paragraph with these numbers and T5.6's.
- **T6.4** Mutation pass (`build/ref-032/mutate.py`, not committed: one mutant at a time, judged by the named tests):
  18 mutants, 18 killed. CMake (judged by `consumer`, `consumer_platform_*`, `consumer_pruned`): the platform check
  returning early; an empty platform taken as `pc`; `nnh_game` reading a directory variable again; examples always ON;
  the `link/` gate removed; the missing-folder check removed; the launcher not applied; C++20 forced over the game's
  choice; the version not cached. `nnh` (judged by `nnh_tool_tests` filters): the farthest root instead of the nearest;
  `nnh.toml` errors dropped; unknown keys refused; build presets not required; `NNH_HOST_CXX` in drive form; the override
  not converted; a quote not refused; the extras on every shell; the `PATH` order changed. Two CMake mutants (examples
  always ON, the version not cached) first **survived**: `consumer` reused its build folder, whose cache kept the
  earlier values. Fix: the local way deletes its `CMakeCache.txt` before every configure (objects kept; +7 s), then both
  were killed. Argued, not run: the response-file variable not set (the length check fails: T1.1's 27,354) or set in
  `engine/core/` (breaks every compile, the spike); `check_preset` and `open_project` skipped in the commands (glue,
  checked by hand at T4.5 and by stage 5's runs).
