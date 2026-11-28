# Plan 034: Versions, API stability and releases

Status: approved 2026-11-27 (pre-approved by the maintainer for the night: every question as recommended; part A only — part B's public steps (questions 9-11, tags, issues, publishing, new repositories, a new VM) wait for the maintainer's explicit OK each time)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decision D57 (proposed,
[decisions.md](../../docs/architecture/decisions.md))
Uses spec 032 and D55 (the engine as a subproject, `NNHGE_VERSION*` in `cmake/nnh_version.cmake`, the consumer project
`tests/consumer/`, `scripts/check_command_length.sh`), spec 033 and D56 (`nnh --version`, `build_info`, the release pin
`NNH_RELEASE_ENGINE_URL`/`_SHA256` checked by `tools/nnh/nnh_release_pin.cmake` and the `nnh_pin_check` ctest, `nnh new`'s
engine location and version readers, `doctor`'s project checks), spec 027 (the save format), spec 009 (`CIA`), spec 011
(NNH Link). Lean path: mostly process, scripts, CI and docs, plus one small public header.

Two parts, built one after the other:

- **Part A, the tooling** (stages 0–11): the version, the promise, deprecation, the compatibility check, the changelog,
  the release scripts and workflow, the checklist. Built in one worktree (branch `spec-034`) from `main` at `eacf005` or
  later, checked like every spec (one review and one `vm_check.sh final` together, fixes with `fast`, console runs,
  merge).
- **Part B, the 1.0 release** (stages R1–R7): the workflow dry run, `v1.0.0-rc.1`, the newcomer test, the first game,
  `v1.0.0`. Every public step in it needs the maintainer's explicit OK for that tag (spec FR-25); nothing in this plan
  starts a workflow, pushes or publishes on its own.

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | `<nnh/version.hpp>` and `NNH_DEPRECATED` are plain C++ with no platform switch; the version line is logged by core on every system; the compatibility check and the deprecation cases compile with the host compilers and devkitARM |
| II 3DS constraint | ✅ | Nothing at run time: `constexpr` values and one Debug/Development log line. The 3DS package ships Release builds that already meet the limits |
| III Build speed | ⚠️→✅ | Pressure points: (a) a version in every compile would rebuild everything at each bump: the header holds only the release number (no commit id, no date) and the one core source that checks it against CMake gets the CMake values as definitions on that file only; (b) the compatibility check adds about ten compile-only sources to builds with examples: measured against the clean-build numbers; (c) release scripts and the workflow are outside the day-to-day build |
| IV Event-driven | ✅ | No events. The version is a query (`nnh::version()`); nothing is published |
| V No hidden costs | ✅ | No allocation, no code in Release (the log line is compiled out like the other `NNH_LOG_INFO` lines; `check_release_strings.sh` gains its format) |
| VI Spec + test first | ✅ | The version parser, the header/CMake agreement, `NNH_DEPRECATED`, the deprecated keyword, the stability check, the changelog draft, the notes builder and the release scripts' rules are tested first (AC-1 to AC-6); the workflow by a dry run (AC-7); the binaries by checks on clean systems (AC-8, AC-9); the 3DS package on the console (AC-10) |
| VII Small files | ✅ | One concept per file: the release steps are separate scripts (each ≤ 150 lines planned), the docs tool's new parts are two modules beside it (`docs_tool.py` itself stays ≤ 250 lines), the C++ is one header and one source |
| VIII Documented API | ✅ | New pages: `docs/api/version.md`, `docs/api/stability.md` (every public header classified, checked), `docs/process/releasing.md`, `CHANGELOG.md`; experimental parts marked in headers and API pages; D57; the docs snapshot and the generated project link to the docs at the tag |
| IX Clean and legal | ⚠️→✅ | Pressure point: shipped binaries carry third-party code (vendored libraries, static C/C++ runtimes, MinGW's winpthreads, libctru/citro2d/citro3d, newlib) and example assets. Each package gets a `NOTICES.txt` from an audit task; the asset audit is a checklist line; devkitPro is referenced, never shipped |
| X Safe by default | ⚠️→✅ | Pressure points: (a) the workflow: `workflow_dispatch` only, `dry_run` on by default, one job with `contents: write` that only creates a draft, `GITHUB_TOKEN` only, actions by SHA, devkitPro by digest, a WinLibs toolchain by SHA-256; (b) the pin: hash-checked twice (before the `nnh` build and by running `nnh new`); (c) published assets never change (immutable releases, FR-25); (d) a static check of the workflow files runs in `ctest` |
| XI Opt-in, never in the way | ✅ | Games need nothing new; `NNH_VERSION`, `NNH_DEPRECATED` and the stability page are there to read. No engine switch to silence deprecations (the compiler's own flag does it, spec Q6) |
| XII Keep the promise (added by this spec, Q14) | ✅ | This spec writes the promise down; nothing in it breaks the surface it lists. The change to `nnh new`'s engine-folder test (below) is in a tool that has never been released |

## Design

### Overview

```
engine/core/include/nnh/version.hpp     THE version (the one place, FR-1): MAJOR, MINOR, PATCH, LABEL, STRING
        │ parsed before project() by cmake/nnh_version_read.cmake (also by `cmake -P` in the release scripts)
        ▼
CMake: project(nnhge VERSION M.m.p), NNHGE_VERSION (numeric), NNHGE_VERSION_LABEL, NNHGE_VERSION_STRING
        │                                   │
        ▼                                   ▼
core version_check.cpp: static_assert      tools/nnh: NNH_TOOL_VERSION = NNHGE_VERSION_STRING (`nnh --version`)
that the compiler sees what CMake read

release (scripts/release/, the same on a runner, Windows and the build VM):
  source_archive.sh  git archive <ref> → gzip -n → nnhge-V-src.tar.gz ──► SHA-256 ─┐
                                                                                    │ URL = …/releases/download/vV/nnhge-V-src.tar.gz
  build_nnh.sh       unpack the archive → configure with -DNNH_RELEASE_ENGINE_URL/_SHA256 → nnh → nnh-V-<os>-x64.*
  build_3ds.sh       unpack the archive → 3ds-release + dist → nnhge-V-3ds.zip
  docs_snapshot.sh   git archive --format=zip of the docs → nnhge-V-docs.zip
  checksums.sh       SHA256SUMS (sorted, `sha256sum -c` format)
  release_notes.py   CHANGELOG section + pin lines + verification + tool versions → notes.md

.github/workflows/release.yml (manual): check → source → {tests-linux, nnh-linux, nnh-windows, nnh-macos, three-ds,
  docs} → assemble (sums, notes, cross-checks; read-only) → draft (only if dry_run is false; contents: write)
the maintainer: tag (with his OK) → run the workflow → checks the draft → publish (with his OK)
```

### The version (FR-1 to FR-6)

**One place: `<nnh/version.hpp>`.** The header is a checked-in source file with five lines in a fixed shape:

```cpp
#define NNH_VERSION_MAJOR 1
#define NNH_VERSION_MINOR 0
#define NNH_VERSION_PATCH 0
#define NNH_VERSION_LABEL "dev"          // "" for a release, "rc.N" for a candidate, "dev" between releases
#define NNH_VERSION_STRING "1.0.0-dev"
```

`cmake/nnh_version_read.cmake` defines `nnh_version_read(<header> <prefix>)`: it reads those five lines (each must
appear exactly once, at the start of a line), checks the numbers (no leading zeros), the label (`""`, `dev` or `rc.N` with
N ≥ 1: the labels FR-5 allows; anything else is refused) and that `STRING` equals `M.m.p` or `M.m.p-LABEL`, and fails
the configure naming the header and the line otherwise. The top-level `CMakeLists.txt` calls it **before** `project()`
and passes the numbers to `project(nnhge VERSION ...)` (CMake's `VERSION` takes numbers only, spec Q4); the header is
added to `CMAKE_CONFIGURE_DEPENDS`, so editing it reconfigures. `cmake/nnh_version.cmake` then adds two `CACHE INTERNAL`
entries to spec 032's four: `NNHGE_VERSION_LABEL` and `NNHGE_VERSION_STRING`, and its status line shows the string
(`NNHGE 1.0.0-dev: <folder>`). `NNHGE_VERSION` stays numeric so `VERSION_LESS` keeps working in game projects.

Why the header and not `project()`: it is the one file every consumer can read without CMake (the C++ compiler, the
release scripts through `cmake -P`, `nnh`'s readers), so no second copy needs a check. The same pattern as fmt, whose
`CMakeLists.txt` parses its version from a header. Rejected: (a) the number in `project()` plus a label line and a
hand-written header checked by a test (two places to edit at every bump); (b) a header generated by `configure_file`
(a public header that exists only in a build folder, an extra include directory every game project gets, and no header
to read in the source archive).

**FR-3, header and build agree.** `engine/core/src/core/version_check.cpp` is compiled into `nnh_core` with three
definitions on that file only (`NNH_CMAKE_VERSION_MAJOR/_MINOR/_PATCH` and `NNH_CMAKE_VERSION_STRING`) and
`static_assert`s that the compiler's `NNH_VERSION_*` equal them (the string compared by a `constexpr` loop). It catches a
parser bug or a stale include path on every build, PC and 3DS. It holds no code (an empty object in Release). A
version bump recompiles that file only, plus every file that includes the header (few: the umbrella and the log line).

**Public API** (`<nnh/version.hpp>`, no `#include` at all):

```cpp
#define NNH_VERSION_OF(major, minor, patch) ((major) * 1000000 + (minor) * 1000 + (patch))
#define NNH_VERSION NNH_VERSION_OF(NNH_VERSION_MAJOR, NNH_VERSION_MINOR, NNH_VERSION_PATCH)
/// Marks a function, type, enumerator or variable as deprecated: every use warns with `message`, which names the
/// replacement and the release that removes it ("use draw_text(screen, text, options); removed in 2.0").
#define NNH_DEPRECATED(message) [[deprecated(message)]]

namespace nnh {
struct Version {
    int major;          ///< 1 in 1.2.3
    int minor;
    int patch;
    const char* label;  ///< "" for a release, "rc.1" for a candidate, "dev" for a build between releases
};
constexpr Version version();                 ///< {NNH_VERSION_MAJOR, ..., NNH_VERSION_LABEL}
constexpr const char* version_string();      ///< NNH_VERSION_STRING, "1.0.0-rc.1"
constexpr int version_number();              ///< NNH_VERSION
/// This engine is major.minor.patch or later (the label is ignored: a candidate counts as its release).
constexpr bool version_at_least(int major, int minor, int patch);
} // namespace nnh
```

Minor and patch stay below 1000 (checked by the CMake parser); the integer fits `int` on every target. The label is not
part of `NNH_VERSION`: `1.0.0-rc.1` and `1.0.0` compare equal, documented on the API page (pre-releases are outside the
promise, FR-5). The umbrella `<nnh/nnh.hpp>` includes the header. `major`/`minor` as member names are safe with glibc's
old `major()`/`minor()` function-like macros (a member name is not followed by `(`); a test compiles them with
`<sys/types.h>` included first on Linux.

**FR-4, the log line.** `nnh::init()` logs `NNHGE 1.0.0-rc.1 (Development)` (`NNH_LOG_INFO(app, "NNHGE %s (%s)", ...)`)
right before its `started "..."` line. Release builds compile it out; `check_release_strings.sh` gains the format.

**FR-5, the version on `main`.** Between releases `main` carries the next version with the label `dev` (spec Q4). The
sequence for 1.0:

| Commit on `main` | Header | Tag |
|------------------|--------|-----|
| this spec's merge | `1.0.0-dev` | — |
| the candidate commit | `1.0.0-rc.1` (+ the changelog section) | `v1.0.0-rc.1` |
| right after the tag | `1.0.0-dev` | — |
| a later candidate, if needed | `1.0.0-rc.2` | `v1.0.0-rc.2` |
| the release commit | `1.0.0` | `v1.0.0` |
| right after the tag | `1.1.0-dev` | — |

A bump is `cmake -DVERSION=1.0.0-rc.1 -P scripts/release/set_version.cmake`, which rewrites the five lines through the
same parser (refusing a bad version) and nothing else.

**FR-6, tags.** `scripts/release/check_tag.sh <tag>`: the tag exists, is annotated (`git cat-file -t` says `tag`), its
commit is on `origin/main` (`git merge-base --is-ancestor`), and `v` + the header's string at that commit equals the tag
(read with `git show <tag>:engine/core/include/nnh/version.hpp` piped to the parser). Used by the workflow and by the
scripts in release mode.

### `nnh` and the full version string

Spec 033 compiled `NNHGE_VERSION` (numeric) into `nnh`. With labels, three small changes keep `nnh --version`,
`nnh new` and `nnh doctor` consistent:

- `tools/nnh/CMakeLists.txt`: `NNH_TOOL_VERSION` = `NNHGE_VERSION_STRING`. `nnh --version` prints
  `nnh 1.0.0-rc.1 (…, release build)`; `nnh new` writes the string into `nnh.toml`'s `engine` and the
  `…/blob/v1.0.0-rc.1/docs` links (D56's `@docs_base@`); the template's `NNHGE_VERSION VERSION_LESS` line stays numeric.
- `tools/nnh/src/new/engine_location.{hpp,cpp}`: an engine folder is one whose `engine/core/include/nnh/version.hpp`
  holds a valid version (C++ port of the CMake parser's rules, mini-fuzzed like the other readers) and `cmake/nnh_game.cmake`;
  `engine_version_from_cmakelists` becomes `engine_version_from_header`. The old `project(nnhge VERSION x.y.z` reader
  can't work once `project()` takes a variable. `nnh` has never been released, so nothing outside this repository
  depends on the old test.
- `tools/nnh/src/doctor/project_versions.{hpp,cpp}`: `cache_engine_version` prefers `NNHGE_VERSION_STRING:` and falls
  back to `NNHGE_VERSION:` (a project configured by an older engine).
- Portable runtime: a new cache option `NNH_TOOL_STATIC_RUNTIME` (default OFF) adds `-static-libstdc++ -static-libgcc` to
  `nnh` on Linux (MinGW already links `-static`). Release scripts turn it on; day-to-day and sanitizer builds don't
  change.

### The stability promise (FR-7 to FR-10)

**`docs/api/stability.md`** (the developers' side of the docs, where FR-9 wants the markers read): the promise, the rules
of FR-5, what a minor may still do (spec Q5's list), what game code is not covered, the save promise beyond 1.x (Q7),
and one table per class. Every public header appears exactly once as a row whose first cell is the header in the form
`<nnh/name.hpp>`:

| Class | Headers (1.0) |
|-------|---------------|
| stable | every `engine/core/include/nnh/*.hpp` not below, `<nnh/3ds/console.hpp>`, `<nnh/3ds/launch.hpp>`, `<nnh/3ds/network.hpp>` |
| stable with experimental parts | `<nnh/assets.hpp>` (the hot-reload queries: `reload_count`, `reloaded`, `reloaded_paths`, `AssetsReloaded`) |
| experimental | `<nnh/mesh.hpp>`, `<nnh/gfx3d.hpp>`, `<nnh/live.hpp>` |
| not stable | the folders `nnh/detail/` and `nnh/platform/` (contract), `<nnh/platform_fake.hpp>` (test support) |

`<nnh/math3d.hpp>` stays stable (spec Q2). The non-header promises (CMake, formats, saves, `nnh`, `.nnhs`) and FR-8's
list are prose sections of the same page.

**The marker.** An experimental header's file comment and an experimental declaration's comment carry the exact text
`Experimental in 1.x` with a link to the page (`docs/api/stability.md`); the API page starts with
`> Experimental in 1.x: ...` (or carries it at the top of the section, for partial ones).

**FR-10 check.** `scripts/docs_stability.py`, called by `docs_tool.py check` (so the Stop hook and CI run it):
- lists the public headers on disk: `engine/core/include/nnh/*.hpp` and `engine/platforms/*/include/nnh/**/*.hpp`, minus
  the two folders the page classifies as folders;
- reads the page's table rows (`| <nnh/...> | class | ... |`);
- fails on a header missing from the page, a row naming a header that doesn't exist, a header in two rows, an unknown
  class, and an experimental (or partly experimental) header or API page without the marker.

Fixtures for AC-3 are `.txt` files under `scripts/tests/data/stability/` (not `.md`, so `check` doesn't read them as
pages), with a fake header tree.

**Constitution article XII** (spec Q14), added to `docs/process/constitution.md` with this spec:

> **XII. Keep the promise.** From 1.0 on, a release never breaks the stable surface listed in
> `stability.md` (a link to `../api/stability.md` in the constitution) within a major version: a game that builds and runs with 1.x builds and runs the
> same with every later 1.y. A stable item is deprecated (`NNH_DEPRECATED`, a changelog entry) for at least one minor
> release before a major removes it. Experimental parts are marked where developers read. Save files written by any
> release stay readable by every later one. Every public header is classified on the stability page.

The plan template (`specs/_templates/plan.md`), the review skill and agent gain the row.

### Deprecation and the frozen check (FR-11, FR-12)

- **C++**: `NNH_DEPRECATED` as above (standard attribute; GCC, clang, MSVC and devkitARM warn at each use). Test cases
  `tests/version/deprecated/uses.cpp` (a deprecated function, type, enumerator and variable, each used once) and
  `unused.cpp` (declared, not used) are compiled by `tests/version/check_deprecated.sh <compiler>`: `uses.cpp` must fail
  under `-Werror=deprecated-declarations` with the message text in the output, `unused.cpp` must compile under
  `-Werror`. A ctest runs it with the host compiler; the Windows stage ends and `vm_check.sh final` run it with
  devkitARM's `arm-none-eabi-g++`.
- **CMake keywords**: `cmake/nnh_deprecations.cmake` holds the table of deprecated `nnh_add_game()` keywords
  (`OLD|NEW|since`), empty in 1.0, and `_nnh_apply_deprecated_keywords(<game> <out> ARGN)`, which `nnh_add_game()` calls
  before `cmake_parse_arguments`: each old keyword prints
  `message(DEPRECATION "nnh_add_game(<game>): OLD is deprecated since 1.y: use NEW (removed in 2.0)")` and is replaced.
  Tested by a `cmake -P` ctest on the function with a test table (default `ctest`), and by a small project
  `tests/version/deprecated_keyword/` that registers a test alias through `_nnh_deprecate_game_keyword()` before calling
  `nnh_add_game()`, configures and builds (label `consumer-full`: one engine build).
- **Engine options, `nnh` options and `.nnhs` commands**: the rule is written on the stability page; the code to warn
  is written with the first real deprecation (nothing is deprecated in 1.0; question 5).
- **FR-12, the frozen check** `tests/compat/v1/`: one compile-only source per stable header group (core, input and
  actions, 2D drawing and text and fonts, audio, assets and data and saves, collision and tilemaps and pathfinding,
  entities and pools and prefabs and hierarchy and transforms, tweens and animation and particles and camera and
  transitions, UI and localization, maths; `v1_3ds.cpp` for the 3DS headers in 3DS presets), each calling the documented
  entry points inside functions that are never run, written against the API pages. An OBJECT library
  `nnh_compat_v1`, added with the examples (`NNH_BUILD_EXAMPLES`, so every repository preset, PC and 3DS, compiles it;
  a game project never does), with deprecation warnings allowed and every other warning as the examples treat them.
  Frozen at `v1.0.0`: the folder's README says it changes only in a major (or to add a file for a new header, which
  doesn't weaken it). The keyword half of Q18 is the consumer project's `nnh_add_game()` call
  (`tests/consumer/game/CMakeLists.txt`), which gains `STRICT_STRINGS` (the only keyword it lacks) and the same frozen
  comment.

### Changelog (FR-13, FR-14)

- `CHANGELOG.md` at the root, Keep a Changelog 1.1.0: `## [Unreleased]`, then `## [1.0.0-rc.1] - 2026-MM-DD`, with
  Added / Changed / Deprecated / Removed / Fixed / Security and `### Experimental` for FR-9 changes. Each spec's merge
  adds its line under Unreleased from now on (a `documentation.md` row and a `nnh-docs` skill row).
- `python -I scripts/docs_tool.py changelog [--after NNN | --after-release X.Y.Z]` (code in `scripts/docs_changelog.py`)
  prints, for each spec whose row in `specs/README.md` says implemented and whose number is above NNN (or above the
  highest spec linked in CHANGELOG's section for X.Y.Z), in number order, one line under `### Added` with the spec's
  title and a Markdown link "spec NNN" to `specs/NNN-slug/spec.md`, followed by the ROADMAP's ticked lines that link to that spec, indented as notes. Pure function of
  the two files' text; never writes a file. Tested on fixtures (`scripts/tests/data/changelog/*.txt`) against an
  expected output, run twice.
- `python -I scripts/docs_tool.py checklist X.Y.Z` prints `releasing.md`'s checklist with the version filled in, for the
  release issue (spec Q17): the issue template `.github/ISSUE_TEMPLATE/release.md` says to paste it, so the checklist
  has one home.

### Release artifacts and scripts (FR-15 to FR-22)

**Modes.** Every script takes `--ref <tag|commit|branch>` and `--out <dir>` and one mode:

| Mode | For | Names | Checks | Pin URL |
|------|-----|-------|--------|---------|
| `--release` | the real release (workflow with `dry_run: false`, or a local rebuild of a tag) | `X.Y.Z[-rc.N]` | clean tree, the ref is a tag passing `check_tag.sh` | `https://github.com/<repo>/releases/download/vV/nnhge-V-src.tar.gz` |
| `--preview` | a dry run of a candidate before its tag exists (question 2) | the header's version | clean tree; the header's label is `""` or `rc.N`; no tag `vV` exists yet, or it points at the ref | the same https URL (for `vV`) |
| `--snapshot` | local tests of the tooling | `g<12-hex commit>` | none | `file://<out>/nnhge-g<commit>-src.tar.gz` |

They write only under `<out>` (their temporary files in `<out>/work/`, removed at the end), never tag, push, upload or
publish (no `git tag`, `git push`, `gh` in them: a ctest greps for those words), and append the tool versions they used
to `<out>/tools-<part>.txt`. Shared code in `scripts/release/common.sh` (arguments, modes, the version through
`cmake -P scripts/release/print_version.cmake`, the clean-tree check, tool versions). `<repo>` is `rafarvns/nnhge` by
default (`--repo`; the workflow passes `github.repository`).

| Script | Makes | How |
|--------|-------|-----|
| `source_archive.sh` | `nnhge-V-src.tar.gz` (FR-16) | `git archive --format=tar --prefix=nnhge-V/ <commit>` piped to `gzip -n -9`: the tracked files of that commit only (no `.git`, no build output, the pin never in it), fixed order, the commit's time, owner 0; `.gitattributes` already forces LF (`* text=auto eol=lf`), so the bytes don't depend on `core.autocrlf`. Prints the SHA-256 and writes `nnhge-V-src.tar.gz.sha256` |
| `build_nnh.sh` | `nnh-V-windows-x64.zip` or `nnh-V-linux-x64.tar.gz` (FR-17, FR-18) | checks the archive's SHA-256 against `--sha256`, unpacks it in `<out>/work`, configures `pc-release` there with `-DNNH_RELEASE_ENGINE_URL=<url> -DNNH_RELEASE_ENGINE_SHA256=<sha> -DNNH_TOOL_STATIC_RUNTIME=ON -DNNH_BUILD_EXAMPLES=OFF -DNNH_BUILD_APPS=OFF`, builds `nnh` and `nnh_tool_tests`, runs the tests, `check_pin.sh`, `check_nnh_binary.sh`, and packs `nnh-V-<os>-x64/` = `nnh[.exe]`, `LICENSE`, `NOTICES.txt`, `README.txt` |
| `build_3ds.sh` | `nnhge-V-3ds.zip` (FR-19) | unpacks the archive, configures `3ds-release` (through `scripts/dkp.sh` on Windows, plain in the container or on the VM with `NNH_HOST_CXX`), builds everything and `dist`, runs `check_release_strings.sh` and `check_no_3d_symbols.sh` on `hello`, `check_command_length.sh` on the folder (default and `dist`), and packs `3ds/nnhge/<game>.3dsx` for every game in `nnh_games.txt`, `cia/audio.cia`, `cia/nnh-link.cia`, `README.txt`, `NOTICES.txt` |
| `docs_snapshot.sh` | `nnhge-V-docs.zip` (FR-20) | `git archive --format=zip --prefix=nnhge-V-docs/ <commit> docs README.md CHANGELOG.md CONTRIBUTING.md LICENSE` (spec Q9) |
| `checksums.sh` | `SHA256SUMS` (FR-21) | `sha256sum` of every other asset, sorted by name, then `sha256sum -c` in the folder |
| `check_pin.sh <nnh> <url> <sha>` | AC-9 | `nnh new pin-probe` in `<out>/work`, then the project's `set(nnhge_url ...)`/`set(nnhge_sha256 ...)` must equal the two values |
| `check_nnh_binary.sh <nnh>` | AC-8 | Linux: `readelf -d` NEEDED lists no `libstdc++`/`libgcc_s`, the newest `GLIBC_` symbol version ≤ `--max-glibc` (2.35 by default); Windows: `objdump -p` DLL names all in an allow-list of system DLLs (KERNEL32, WS2_32, `api-ms-win-crt-*`, …, fixed by a measurement in stage 6) |
| `check_tag.sh` | FR-6 | above |
| `tool_versions.sh` | FR-22 | CMake, Ninja, compilers, git, gzip; devkitARM (`arm-none-eabi-gcc --version`), libctru, citro2d, citro3d (devkitPro's pacman database folders, the reader `nnh doctor` uses); SDL3 (`cmake/nnh_sdl3_pin.cmake`) |
| `make_all.sh` | a local run of all of the above in order | for Windows (PC + 3DS through MSYS2) and the build VM (Linux, 3DS with `/opt/devkitpro`); skips what the host can't build (`--only` lists parts) |
| `release_notes.py` | `notes.md` (FR-23) | the CHANGELOG section of V (refused if missing, outside snapshot mode), the two pin lines to paste into a project (`set(nnhge_url ...)`, `set(nnhge_sha256 ...)`), how to verify `SHA256SUMS` (Linux `sha256sum -c`, PowerShell `Get-FileHash`) and the attestation (`gh release verify-asset`), "devkitPro is installed separately", the tool versions, the macOS result, the checklist issue link, the docs at the tag |
| `set_version.cmake`, `print_version.cmake` | the bump, the version for shell scripts | `cmake -P`, through `nnh_version_read` |

Notices and READMEs are text files under `scripts/release/data/` (`nnh-README.txt.in`, `3ds-README.txt.in`,
`NOTICES-nnh.txt`, `NOTICES-3ds.txt`), written by the audit task from `docs/tech/third-party.md` plus the runtimes each
package links (static libstdc++/libgcc: GCC runtime exception; MinGW-w64 CRT and winpthreads; Ryu; libctru, citro2d,
citro3d; newlib; the OFL font of the localization example), recorded in `third-party.md`.

**The chicken and egg (FR-18).** The archive is made first, from git objects only, so it cannot contain the pin; its
SHA-256 and the release URL (known in advance: `releases/download/<tag>/<name>`) are then given to the `nnh` build as
cache entries, and that build runs **from the unpacked archive**, not the checkout. So the shipped `nnh` is built from
exactly the bytes it pins, and building the archive is tested by every `nnh` build. Three checks close the loop:
`build_nnh.sh` refuses an archive whose hash differs from `--sha256`; `check_pin.sh` reads the pin back through `nnh new`;
`assemble` (workflow) and `make_all.sh` compare it with the archive's `SHA256SUMS` line. A test also greps the unpacked
archive for its own hash (never found).

**3DS package.** NNH Link gets `CIA UNIQUE_ID 0xE0A10` (spec Q10; the `audio` example has `0xE0A01`). A `.cia` NNH Link
can pair, build and receive, but can't start the game (`n3ds::can_launch()` is false outside the Homebrew Launcher, spec
011): it shows where the game was saved, as today. The package README says so (question 8).

### Release workflow (FR-23, FR-26)

`.github/workflows/release.yml`, `on: workflow_dispatch` only, inputs `ref` (a tag, or a branch/commit for a preview) and
`dry_run` (boolean, **default true**), `permissions: contents: read` at the top, `concurrency` per ref:

| Job | Runner | Does |
|-----|--------|------|
| `check` | `ubuntu-22.04` | full checkout of the ref; a tag → `check_tag.sh` (release mode), anything else → preview mode, allowed only with `dry_run: true`; outputs version, mode, URL |
| `source` | `ubuntu-22.04` | `source_archive.sh`; outputs the SHA-256; artifact `source` |
| `tests-linux` | `ubuntu-24.04` | from the unpacked archive: SDL3 build dependencies (ci.yml's list), `pc-debug`, `ctest -LE consumer-full` headless (the tests that need `.git` report themselves skipped) |
| `nnh-linux` | `ubuntu-22.04` | `build_nnh.sh` with `g++-12` (glibc 2.35); `nnh --version` and `nnh doctor` |
| `nnh-linux-2404` | `ubuntu-24.04` | runs that `nnh` (`--version`, `doctor`, `check_nnh_binary.sh`) on 24.04 (AC-8) |
| `nnh-windows` | `windows-2022` | a WinLibs MinGW-w64 UCRT release (GCC, CMake, Ninja) downloaded by URL and checked by SHA-256 (question 6); `build_nnh.sh` in Git Bash; `nnh --version`, `doctor` |
| `nnh-macos` | `macos-15` | builds `nnh` from the archive and runs `nnh_tool_tests`; `continue-on-error`, writes `pass`/`fail` into an artifact for the notes (spec Q16); ships nothing |
| `three-ds` | `ubuntu-22.04`, container `devkitpro/devkitarm@sha256:<digest>` | installs `cmake`, `ninja-build`, `g++` (host compiler for `nnh-assetc`); `build_3ds.sh` |
| `docs` | `ubuntu-22.04` | `docs_tool.py check`, `docs_snapshot.sh` |
| `assemble` | `ubuntu-22.04` | downloads every artifact; `checksums.sh`; checks each `nnh`'s recorded pin equals the archive's line; `release_notes.py` (finds the issue titled `Release vV`, `issues: read`; required in release mode); artifact `release` (all assets + notes). In a dry run, the end |
| `draft` | `ubuntu-22.04`, `contents: write`, only if `dry_run` is false and the mode is release | refuses when a **published** release exists for the tag; deletes an existing **draft** for it (FR-25: a wrong draft is fixed by rerunning); `gh release create <tag> --draft --verify-tag [--prerelease for -rc.N] --title "NNHGE V" --notes-file notes.md <assets>`. Never `--latest` or publish |

Every `uses:` pinned by a 40-hex commit SHA with a `# vN` comment (checkout, upload-artifact, download-artifact), only
`GITHUB_TOKEN`. `ci.yml`'s `nintendo-3ds` job uses the same digest (FR-26, spec Q12). `scripts/release/check_workflows.sh`
(a ctest) checks: `release.yml` has only `workflow_dispatch`; `dry_run` defaults to true; every `uses:` has a 40-hex SHA;
every container is `@sha256:`; `ci.yml` and `release.yml` name the same digest; `contents: write` appears exactly once
in `release.yml`, on `draft`. Changing the digest is a deliberate commit that records the new tool versions in
`docs/tech/cmake.md` (the image's `devkitARM`, `libctru`, `citro2d`, `citro3d` versions, read with the same
`tool_versions.sh`).

`workflow_dispatch` needs the file on the default branch, so the first dry run happens after Part A's merge (stage R1);
until then the same steps run locally through `make_all.sh` (stage 10).

### The release checklist (FR-24, FR-25, FR-27)

`docs/process/releasing.md`: the release roles (who may do what: the two public steps are the maintainer's, or an
agent's after his OK for that tag in that session), the order, and the checklist (yes/no lines) from the spec's FR-24
and the study's list, in three groups: before the tag (specs and docs, version, changelog, Windows builds and checks,
build budgets, the VM runs with the fuzz times of spec Q11, the consumer project and the `nnh new` smoke **from the source
archive** on Windows and Linux, the asset and licence audit, the console runs, `check_command_length.sh` on the
repository's and the consumer's 3DS folders), the public steps (tag push: maintainer's OK; workflow run; publish:
maintainer's OK), after the draft (assets, hashes, prebuilt `nnh` `--version`/`doctor`/`new` on clean Windows and Linux,
notes); plus the rc rules of FR-27 (what a later candidate re-runs; `v1.0.0`'s final subset: the examples touched since
the last candidate, `hello`, `platformer/level`, `ui/check`, the generated project) and FR-25's fixes (before publishing:
rerun or delete-and-recreate an unpublished tag; after: only the next patch or candidate). Immutable releases are on
before `v1.0.0-rc.1` (spec Q13, a repository setting the maintainer turns on, told by the checklist).

### Events introduced

None.

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/version.hpp` | new | The version (the one place), `NNH_VERSION_OF`, `NNH_DEPRECATED`, `nnh::Version`, `version()`, `version_string()`, `version_number()`, `version_at_least()` (≈ 70 lines) |
| `engine/core/include/nnh/nnh.hpp` | changed | Includes `version.hpp` |
| `engine/core/src/core/version_check.cpp` | new | FR-3: `static_assert`s against the CMake definitions on this file only (≈ 30 lines) |
| `engine/core/CMakeLists.txt` | changed | The new source and its definitions |
| `engine/core/src/app/app.cpp` | changed | FR-4's line |
| `engine/core/include/nnh/{mesh,gfx3d,live,assets}.hpp` | changed | The experimental marker (comments only) |
| `CMakeLists.txt` | changed | `nnh_version_read` before `project()`, `CMAKE_CONFIGURE_DEPENDS`, the compat folder with the examples |
| `cmake/nnh_version_read.cmake` | new | The parser and its rules (also for `cmake -P`) |
| `cmake/nnh_version.cmake` | changed | `NNHGE_VERSION_LABEL`, `NNHGE_VERSION_STRING`; the status line |
| `cmake/nnh_deprecations.cmake` | new | The deprecated-keyword table and its application |
| `cmake/nnh_game.cmake` | changed | Calls it before parsing |
| `tools/nnh/CMakeLists.txt` | changed | `NNH_TOOL_VERSION` = the string; `NNH_TOOL_STATIC_RUNTIME` |
| `tools/nnh/src/new/engine_location.{hpp,cpp}` | changed | Engine folder and version from the header |
| `tools/nnh/src/doctor/project_versions.{hpp,cpp}` | changed | `NNHGE_VERSION_STRING` first |
| `tools/nnh/tests/*` | changed | The readers' tests, fuzz seeds, `test_build_info.cpp` |
| `apps/nnh-link/CMakeLists.txt` | changed | `CIA UNIQUE_ID 0xE0A10` |
| `scripts/check_release_strings.sh` | changed | The version line's format |
| `tests/core/test_version.cpp` | new | FR-1, FR-2 values against the CMake definitions given to the test, `#if` use, `version_at_least` |
| `tests/version.cmake` | new | The ctests below (included by `tests/CMakeLists.txt`, which stays as it is otherwise) |
| `tests/version/alone.cpp`, `tests/version/glibc_macros.cpp` | new | The header compiled alone (an include folder holding only it) and after `<sys/types.h>` |
| `tests/version/read_check.cmake` + cases | new | `nnh_version_read` on good and bad headers |
| `tests/version/check_deprecated.sh`, `deprecated/{uses,unused}.cpp` | new | AC-2 C++ |
| `tests/version/deprecated_keyword/` | new | AC-2 CMake (`check.cmake` for `-P`, a small project for the build) |
| `tests/compat/v1/` | new | FR-12: README (frozen), `CMakeLists.txt`, ≈ 11 sources (≤ 150 lines each) |
| `tests/consumer/game/CMakeLists.txt` | changed | `STRICT_STRINGS`, the frozen comment |
| `scripts/docs_tool.py` | changed | Loads the two modules by path (works under `python -I`); `check` runs the stability check; `changelog`, `checklist` |
| `scripts/docs_stability.py`, `scripts/docs_changelog.py` | new | FR-10, FR-14 (≤ 200 lines each) |
| `scripts/tests/test_docs_tool.py` + `scripts/tests/data/` | new | Their unit tests (`unittest`, stdlib) on `.txt` fixtures |
| `scripts/release/*.sh`, `*.cmake`, `release_notes.py`, `data/` | new | The table above |
| `scripts/release/tests/test_scripts.sh`, `test_release_notes.py` | new | AC-6 on a scratch git repository; the notes builder |
| `tests/release.cmake` | new | Registers the Python and script tests when `Python3` and `bash` are found (PC presets) |
| `scripts/vm_check.sh` | changed | `final`: the deprecation cases with devkitARM, the release script tests, `source_archive.sh --snapshot` twice |
| `.github/workflows/release.yml` | new | FR-23 |
| `.github/workflows/ci.yml` | changed | The devkitPro digest (FR-26) |
| `.github/ISSUE_TEMPLATE/release.md` | new | The release issue (paste `docs_tool.py checklist`) |
| `CHANGELOG.md` | new | FR-13 |
| `docs/api/version.md`, `docs/api/stability.md` | new | The header; the promise |
| `docs/api/{mesh,gfx3d,live,assets}.md`, `README.md` (its API list) | changed | Markers; the two new pages listed |
| `docs/process/releasing.md` | new | The checklist and the release roles |
| `docs/process/constitution.md`, `docs/process/README.md`, `docs/process/documentation.md` | changed | Article XII; the new page; CHANGELOG and stability rows |
| `specs/_templates/plan.md`, `.claude/skills/nnh-review/`, `.claude/agents/nnh-reviewer.md`, `.claude/skills/nnh-docs/SKILL.md` | changed | Row XII; the docs rows |
| `docs/tech/cmake.md`, `docs/tech/third-party.md`, `docs/tech/devkitpro-3ds.md` | changed | Version, release scripts and workflow, CI pin; notices; NNH Link `.cia` |
| `docs/architecture/decisions.md`, `project-structure.md`, `security.md` | changed | D57; `scripts/release/`, `tests/compat/`, `CHANGELOG.md`; the release workflow's privileges |
| `README.md`, `docs/guides/getting-started.md`, `docs/guides/game-projects.md`, `docs/guides/nnh-link.md`, `docs/ROADMAP.md`, `CLAUDE.md`, `CONTRIBUTING.md` | changed | Part A: the version, the scripts, upgrading a project from a release's notes, the `.cia`; Part B: 1.0 status and the release download (FR-29) |
| `scripts/docs_tool.py` `SINGLE_PAGES` | changed | `CHANGELOG.md` → wiki page `Changelog` |

### Data and memory

Nothing at run time: `constexpr` values and string literals the linker drops when unused; one log line in
Debug/Development. 3DS: no heap, no VRAM; Release `hello.3dsx` expected byte-identical before the version bump (the log
line is compiled out, `version_check.cpp` has no code) and is compared in stage 1.

Release sizes (estimates, measured in stage 10): source archive 4–6 MB, each `nnh` package 1–3 MB, 3DS zip 20–40 MB, docs
zip ≈ 1 MB. Local disk for a full `make_all.sh` run on Windows: about 2–3 GB under `<out>` (one `pc-release` and one
`3ds-release` build from the archive), deleted after; `E:` has 8.9 GB free today, below the spec's 10 GB precondition
(question 12).

### Platform work

- pc: the version line (core); the `nnh` static runtime option on Linux; the Windows and Linux `nnh` packages.
- 3ds: the version line (core); the compatibility check and `version_check.cpp` compiled by `3ds-*` presets; the
  deprecation cases with devkitARM; NNH Link's `.cia`; the 3DS package from `3ds-release`.
- fake (tests): the version line captured by the fake platform's log in a unit test.

## Test strategy

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `read_check.cmake` (good/bad headers: missing line, duplicate, leading zero, bad label, string ≠ parts, minor ≥ 1000); `test_version.cpp` (values = CMake's); `version_check.cpp` builds on PC and 3DS; `test_build_info.cpp` (the string); release names from `print_version.cmake` in `test_scripts.sh` |
| FR-2 | `test_version.cpp`; `alone.cpp` (only the header on the include path, `-Werror`, `#if NNH_VERSION >= NNH_VERSION_OF(...)`); `glibc_macros.cpp` (Linux) |
| FR-3 | `version_check.cpp` + a mutation (edit `NNH_VERSION_STRING` only, then `MAJOR` only: configure or build fails, recorded) |
| FR-4 | a unit test on the fake platform's log (Debug flavour); the Development `hello` on the Old 3DS; `check_release_strings.sh` on Release `hello` (PC and 3DS) |
| FR-5 | `read_check.cmake` (allowed labels); `releasing.md`'s bump lines; stage R2/R4/R7 commits |
| FR-6 | `test_scripts.sh` (scratch repo: lightweight tag refused, annotated tag on a commit with another version refused, the right one accepted); the workflow's `check` job |
| FR-7, FR-8 | the stability page reviewed against the headers (review); FR-10's check |
| FR-9 | the check's marker rules; review of the four headers and pages |
| FR-10 | `test_docs_tool.py` (fixtures: an unlisted header, a missing one, a duplicate, an unknown class, a missing marker → fail; the real tree passes); `docs_tool.py check` in every run |
| FR-11 | `check_deprecated.sh` (host GCC/clang in ctest; devkitARM at stage ends and in `final`); `deprecated_keyword` `-P` test (default) and build (`consumer-full`) |
| FR-12 | `nnh_compat_v1` in `pc-debug` and `3ds-release`; a mutation (rename one public function it uses: the build fails, recorded) |
| FR-13 | `CHANGELOG.md` exists (docs check link); its section read by `test_release_notes.py` |
| FR-14 | `test_docs_tool.py` (fixture → expected lines, twice; no file written) |
| FR-15, FR-16 | `test_scripts.sh` (scratch repo: two runs same SHA-256; the file list equals `git ls-tree -r` under `nnhge-V/`; no `.git`; the hash absent from the archive); `source_archive.sh --snapshot` twice on the real repository on Windows and the VM (stage 10, `final`) |
| FR-17 | `check_nnh_binary.sh` in the workflow (22.04, 24.04, Windows) and Windows Sandbox by hand (AC-8) |
| FR-18 | `check_pin.sh` in `build_nnh.sh`; `assemble`'s cross-check; AC-9 runs (stage 10 snapshot with a real `file://` download; R1 with the local override; R5 the real download) |
| FR-19 | `build_3ds.sh`'s package list check; console runs (stage 11: from a local preview zip; R3: from the draft) |
| FR-20 | `docs_snapshot.sh`'s file list; the generated README's links (`test_template_render.cpp` with a release pin) |
| FR-21 | `checksums.sh` (format, sorted, `sha256sum -c`); `test_scripts.sh` |
| FR-22 | `test_scripts.sh`: dirty tree refused in release/preview, a non-tag commit refused in release mode, nothing written outside `<out>` (a listing of the sandbox before and after), no `git tag`/`git push`/`gh` in the scripts; `make_all.sh` on Windows and the VM (stage 10) |
| FR-23, FR-26 | `check_workflows.sh` (ctest); the dry run (R1, AC-7) with `gh release list` unchanged |
| FR-24 | `releasing.md` reviewed; the ticked issue for rc.1 and 1.0 (AC-11) |
| FR-25 | the scripts' no-publish test; `draft` refusing a published release (read in review; exercised at R3 only by its happy path); the recorded OKs |
| FR-27 | R3, R6, R7 |
| FR-28 | R5, R6: the friction log |
| FR-29 | R7's docs; `docs_tool.py check` |

Script tests run where `bash`, `git` and `gzip` exist (Git Bash on Windows, the VM); the Python ones where a `Python3`
interpreter is found (else the ctest isn't registered, and the configure says so). They take seconds: the scratch
repository is a few files with fixed author and dates.

## Risks and alternatives

1. **The Linux `nnh` on Ubuntu 22.04.** The runner's default compiler is GCC 11; spec 033 found GCC 12 to be the
   engine's minimum on VM 120 (D56 change log), and Ubuntu 22.04 ships `g++-12` in its own archive (the runner image
   has it). So the job uses `g++-12`. Signal: the `nnh-linux` job fails to compile. Fallback: GCC 13 from the
   `ubuntu-toolchain-r` PPA (question 7). Static libstdc++ keeps the glibc floor at 2.35 either way.
2. **Reproducibility across tools.** `git archive` output and `gzip -n` are stable for the same versions, which is what
   FR-16 asks; across git or gzip versions the bytes may differ. The release's archive is made once, by the workflow,
   and projects pin that upload, so this only matters to someone re-deriving it: the notes name the git and gzip
   versions. Windows and the VM are compared in stage 10 (informational).
3. **`workflow_dispatch` needs the file on `main`**: the first dry run is after the merge (R1). Mitigation: `make_all.sh`
   runs the same scripts on Windows and the VM before the merge; `check_workflows.sh` checks the file statically; a
   fix after R1 is a normal small change (failing test first when it is a script bug).
4. **A wrong pin in a published `nnh`** would break every new project. Mitigation: the three checks above, the
   checklist's prebuilt-`nnh` line on clean machines before publishing; after publishing, only the next candidate.
5. **The version line changes log output** some tests may count. Signal: a unit or script test that matches the first
   lines. Mitigation: run the full suite at stage 1's end; `.nnhs` scripts match lines by pattern.
6. **Changing `nnh new`'s engine-folder test** (header instead of `project()`) touches spec 033 code. Mitigation: the
   tests and fuzz seeds are updated first (red), `nnh_new_smoke` runs in `final`.
7. **devkitPro image digests change upstream** ("latest only"): pinning means we stay on an older devkitARM until a
   deliberate bump; the VM's `/opt/devkitpro` came from an image too and may differ. Signal: `tool_versions.sh` output
   in the notes vs the VM's. Mitigation: pick the digest whose tool versions match the VM's or the maintainer's
   Windows install, recorded in `cmake.md`; mismatches go into the notes.
8. **WinLibs download from GitHub in the workflow**: an upstream release could disappear. Mitigation: pinned by
   SHA-256; if it goes, choosing another is a deliberate change (like the digest).
9. **The first game is a real project** (FR-28) and may find engine bugs late. Mitigation: that is the point of the
   candidate; fixes follow the bug-fix rule and the rc re-run rule; scope kept to the spec's single-screen game.
10. **Disk** (8.9 GB free on `E:`): a full local release run adds 2–3 GB. Mitigation: question 12 before stage 0.
11. **Wiki and docs at the tag**: the wiki follows `main`; the notes and the generated README link to `tree/vV/docs`, which
    exists once the tag is pushed. A preview draft's links point at a tag that doesn't exist yet: expected, noted in the
    dry run's notes.

Alternatives considered: a `VERSION` file at the root (as simple to parse, but it is not readable by the C++ compiler, so
the header would need generating or checking again); a tag-triggered workflow (ruled out by the spec); building the
release `nnh` from the checkout (then the archive's buildability is not proven by the binaries that pin it); signing
(non-goal).

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | Unchanged: one header, no platform switch; the 3DS-only compat source is picked by CMake like NNH Link's platform file |
| II | ✅ | No run-time cost |
| III | ✅ | The version in one header without commit or date; definitions on one file; the compat sources measured in stage 3 (target: clean `pc-debug` all-targets build +≤ 3 %, one-file edit unchanged) |
| IV, V | ✅ | No events, no allocation, nothing in Release |
| VI | ✅ | Every rule tested first; mutations for FR-3 and FR-12 |
| VII | ✅ | Small scripts and modules; `docs_tool.py` split instead of grown |
| VIII | ✅ | Pages listed above; every public header classified and checked |
| IX | ✅ | Notices per package from the audit; asset audit in the checklist |
| X | ✅ | Least-privilege workflow checked by a test; pins everywhere; immutable releases |
| XI | ✅ | Nothing new required of a game |
| XII | ✅ | The promise and its check exist from this spec on |

No exception is requested. The design adds one decision, D57 (proposed).

## Questions for the maintainer

1. **The version lives in `<nnh/version.hpp>`, parsed by CMake** (not in `project()`, not in a root `VERSION` file).
   Consequence: `nnh new`/`doctor` find an engine folder by that header instead of `project(nnhge VERSION ...)`.
   *Recommendation: yes* (one place every tool can read; no generated header).
2. **Dry runs without a test tag: a "preview" mode** (the workflow on a branch or commit, `dry_run` only, names from the
   header's version) instead of pushing a throwaway tag to GitHub, which FR-6 wouldn't allow on `main` anyway.
   *Recommendation: yes.*
3. **`main` between candidates**: `1.0.0-dev` (the table in "FR-5"), `1.1.0-dev` after `v1.0.0`. *Recommendation: yes.*
4. **Stability page in `docs/api/stability.md`** (next to the API pages developers read; the constitution links it)
   rather than `docs/process/`. *Recommendation: `docs/api/`.*
5. **Deprecation code for `nnh` options, `.nnhs` commands and engine options waits for the first real deprecation** (the
   rule is written now; the `nnh_add_game()` keyword mechanism is built now because AC-2 tests it). *Recommendation:
   yes* (nothing to deprecate in 1.0; code without a user would be untested in practice).
6. **Windows `nnh` in the workflow built with a pinned WinLibs MinGW-w64 UCRT release** (URL + SHA-256, the same family as
   your machine's GCC 16.2) rather than the runner image's MinGW. *Recommendation: WinLibs pinned.*
7. **Linux `nnh` compiler on Ubuntu 22.04**: Ubuntu's own `g++-12` (spec 033's measured minimum), with GCC 13 from the
   `ubuntu-toolchain-r` PPA (a third-party apt source inside the release job only) as the fallback if a later change
   needs GCC 13. *Recommendation: `g++-12`; accept the PPA only as the fallback.*
8. **NNH Link `.cia`**: from the HOME Menu it pairs, builds and receives but can't start the game (only the Homebrew
   Launcher can). Ship it anyway with a README line ("to start games from NNH Link, open its `.3dsx`")?
   *Recommendation: ship it, as spec Q10 decided, with the line.*
9. **The release issue** (spec Q17) is a public write: created by you, or by an agent after your OK, from
   `docs_tool.py checklist`; the `draft` job refuses to run without it. *Recommendation: yes.*
10. **The first game** (FR-28): its own new repository (public, under your account, created by you), its design as a short
    note in that repository rather than an engine spec, built after rc.1 is published. *Recommendation: yes; you pick
    the name.*
11. **Clean machines for the newcomer test**: Windows Sandbox on your PC (needs the Windows feature enabled; your
    Windows 11 Pro has it) and, for Linux, a temporary clean Ubuntu 24.04 VM from template 9000 asked of Rotom (Ubuntu
    22.04 is covered by the workflow's runner). *Recommendation: yes to both.*
12. **Disk before stage 0**: `E:` has 8.9 GB free (spec precondition: ≥ 10 GB). Remove the merged `spec-033` worktree and
    its build folders first? *Recommendation: yes (it is merged; its build folders are several GB).*
13. **The compatibility check is built with the examples** (`NNH_BUILD_EXAMPLES`) instead of a new engine option.
    *Recommendation: yes* (no new option on the promised list).
14. **Spec status**: after Part A's merge, `specs/README.md` says "tooling merged, rc.1 next"; `implemented` only after
    `v1.0.0` (AC-12). *Recommendation: yes.*
