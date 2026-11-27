# Spec 032: Game projects outside the engine repository

Status: approved 2026-11-27 (the maintainer, every open question as recommended; if FR-19 needs the core split, `hello` may change once, sizes and symbols checked, and FR-17's byte identity holds again from then on)
Author: NNHGE maintainers · Date: 2026-11-26 · Updated: 2026-11-27 (the study's step 1 spike results)
Depends on: [spec 010](../010-nnh-cli-run/spec.md) (`nnh build`/`run`/`install`, the games manifest, the presets
`nnh` picks), [spec 011](../011-nnh-link/spec.md) (`nnh serve`), [spec 007](../007-assets-and-romfs/spec.md),
[spec 009](../009-packaging/spec.md), [spec 012](../012-tilemaps-and-collision/spec.md),
[spec 014](../014-data-assets/spec.md), [spec 016](../016-sprite-animation/spec.md),
[spec 026](../026-3d-foundation/spec.md) and [spec 029](../029-game-fonts-and-localization/spec.md) (the
`nnh_add_game()` keywords and build steps that must work from a game's own project). Implemented after
[spec 031](../031-hot-reload-assets/spec.md), which also changes `nnh`. First of the three release specs proposed by the
[1.0 release study](../../docs/plans/2026-11-26-release-1.0.md) (032 game projects, 033 `nnh new`/`nnh doctor`, 034
versions and releases); that study is a draft the maintainer has not reviewed, so its decisions are open questions
below, not settled.

## Problem and goal

Today a game can only live inside the NNHGE repository: the getting-started guide tells the developer to add a folder
and `add_subdirectory` it from the engine's own `CMakeLists.txt`, and `nnh` refuses to work anywhere else ("run it
inside the NNHGE repository"). A game developer wants their game in its own folder and its own repository, with the
engine as a pinned dependency they can upgrade on purpose, without the engine's examples, tests and apps in their
build. After this spec the engine is a well-behaved CMake subproject: a game's own `CMakeLists.txt` brings it in (a
hash-pinned source archive, a local engine checkout, a git submodule or a copied folder), calls `nnh_add_game()` with any
of its keywords from any of its folders, and builds for the simulator and the 3DS; `nnh build`, `run`, `install` and
`serve` work from inside that game project exactly as they do inside the engine repository. The engine repository's own
build does not change.

## User stories

- As a game developer, I want to keep my game in its own repository and pull a pinned NNHGE release into its build,
  so that my repository holds only my game and an upgrade is a deliberate two-line change.
- As a game developer working on the engine too, I want my game project to use my local engine checkout instead of the
  download, without editing the game's `CMakeLists.txt`, so that I can try an engine change in my game at once.
- As a game developer who prefers git submodules or a vendored copy, I want those to work the same way, so that I
  choose how I depend on the engine.
- As a game developer, I want every `nnh_add_game()` keyword (assets, images, Tiled maps, Aseprite sprites, data
  assets, fonts and strings, meshes, icon, `.cia`) to work from my project's folders, so that nothing I read in the
  docs only works inside the engine repository.
- As a game developer, I want my build to contain my game and what it needs, not the engine's 30 examples, its unit
  tests, NNH Link and the `nnh` tool, so that configuring and building stay fast.
- As a game developer, I want `nnh run mygame` and `nnh run mygame --pc` to work from my project folder, including the
  first 3DS build of a game with maps or data, so that the console workflow is the same as in the engine repository.
- As a game developer, I want to read the engine's version in CMake, so that my project can refuse an engine that is
  too old for it.
- As an engine contributor, I want a test that builds a game project against the engine, so that a change that breaks
  out-of-repository games fails before it is merged.

## Functional requirements

### Bringing the engine into a game project

- **FR-1 One subproject contract.** A game project whose own top-level `CMakeLists.txt` calls `project()` can add the
  engine in any of these four ways and gets the same result (the same targets, functions, options and outputs):
  1. **pinned download**: CMake's `FetchContent` of an engine source archive given by URL and SHA-256, declared under
     the fixed content name `nnhge`;
  2. **local override** of (1): `FETCHCONTENT_SOURCE_DIR_NNHGE=<engine folder>` set outside the game's
     `CMakeLists.txt` (command line, `CMakeUserPresets.json`), which skips the download;
  3. **git submodule**: `add_subdirectory(<submodule folder>)`;
  4. **copied folder**: `add_subdirectory(<folder>)` of an unpacked source archive or a plain copy.
  After the engine is added, `nnh_add_game()` and the documented helpers are available with no further `include()`,
  path or variable set by the game.
- **FR-2 Pinned and checked.** With the pinned download, a wrong SHA-256 or an unreachable URL fails the configure step
  with CMake's own error naming the URL; nothing is built from an unverified archive. The engine's own pinned downloads
  (SDL3 for the simulator; makerom and bannertool for `.cia`) keep their pins and go into the game's build folder.
- **FR-3 No git, no network beyond the pins.** Configuring the engine as a subproject needs no git and no `.git`
  folder, and downloads nothing besides the archive the game pins and the engine's existing pinned downloads.
- **FR-4 Only what a game needs.** When the engine is not the top-level project:
  - its examples, unit tests, platform conformance tests, fuzz targets, the NNH Link app and the `nnh` tool are not
    configured: none of their targets exist in the game's build and none of the engine's tests appear in the game's
    `ctest`;
  - the host programs a game's build steps need (`nnh-assetc`, the data schema support and programs) are still
    provided, as they are today in PC and 3DS presets;
  - every one of these parts has an engine option that selects it: the existing `NNH_BUILD_EXAMPLES` (examples) and
    `NNH_BUILD_TESTS` (unit, conformance and fuzz tests), plus new options for the apps (NNH Link) and for the
    developer tools (the `nnh` tool and its tests), which today are always added with no option. All of them default to
    `PROJECT_IS_TOP_LEVEL` of the engine's project (ON in the engine repository, OFF as a subproject) and can still be
    set explicitly by the game, for example to build NNH Link from a game project for testing;
  - none of these options turns off the host programs of the second point: a game with maps, sprites, data or fonts
    builds with all of them OFF.
- **FR-5 Works without the parts it skips.** The engine configures and builds as a subproject when its examples, tests,
  apps, specs and docs folders are absent (a pruned copy), so the content of the release archive (spec 034) and of a
  vendored copy is not constrained by this spec.

### `nnh_add_game()` from anywhere

- **FR-6 Caller-independent helpers.** `nnh_add_game()` with every keyword it accepts (`SOURCES`, `ASSETS`, `IMAGES`,
  `DATA_SCHEMA`, `LANGUAGES`/`BASE`/`STRICT_STRINGS`, `TITLE`, `DESCRIPTION`, `AUTHOR`, `VERSION`, `ICON`, `CIA` with
  `UNIQUE_ID`, `PRODUCT_CODE`, `BANNER`, `BANNER_AUDIO`), and every build step an assets folder can trigger (images,
  Tiled maps, Aseprite exports, data files, fonts, string files, glTF meshes, sounds), behaves the same when called
  from the game project's top-level folder, from a nested folder of it, and from inside the engine repository. Which
  system the preset targets (PC or 3DS) and where the engine's own files are come from the engine, never from
  variables of the caller's directory.
  **Unknown platform is an error.** If `nnh_add_game()` (or a helper it calls) cannot tell which system it builds for,
  or the value is not one of the systems the engine supports, the configure fails with an error that names the game,
  the value found (or "empty") and the supported systems. It never takes another system's branch silently: no game
  target without its system's packaging (on 3DS the SMDH, `.3dsx` and `dist`), no asset copied unconverted, no
  cross-preset host-tool path on a system that builds the program directly.
- **FR-7 Same game build settings.** A game's targets in a game project compile with the same language standard,
  warnings, flags, definitions and compiler cache (when one is installed) as the same game inside the engine
  repository, so build times and behaviour do not depend on where the game lives.
- **FR-8 No side effects on the rest of the project.** Adding the engine does not change how the game project's other
  targets (its own libraries and tools that are not games) are compiled, and does not change global settings of the
  game project beyond the cache entries and targets the engine documents. The target names the engine claims in a
  game's build (for example `dist` and `nnh_assetc`) are listed in the docs.
- **FR-9 Outputs in the game's build folder.** Everything the engine and `nnh_add_game()` produce — the games manifest
  `nnh` reads, game executables, `.3dsx`, romfs, `dist/`, `.cia`, fetched tools, host programs — is written under the
  game project's build folder. Nothing is written into the engine's source folder, so one local engine checkout can
  serve several game projects and a fetched copy stays as downloaded.

### Version

- **FR-10 Engine version in CMake.** After the engine is added, the game project's scope has `NNHGE_VERSION` and
  `NNHGE_VERSION_MAJOR`, `NNHGE_VERSION_MINOR`, `NNHGE_VERSION_PATCH`, equal to the engine's project version, so a game
  can write `if(NNHGE_VERSION VERSION_LESS 1.1)`. The same variables exist in the engine repository's own build. The
  version number itself and its C++ counterpart belong to spec 034.

### `nnh` in a game project

- **FR-11 Game project marker.** A folder containing a file named `nnh.toml` is the root of a game project. `nnh`
  looks from the current folder upward and uses the **nearest** folder that is either a game project root or the engine
  repository root (as found today), so a game project nested inside another folder, or a test project inside the engine
  repository, is found as itself. Outside both, `nnh` exits 1 with a message naming `nnh.toml` and the engine
  repository.
- **FR-12 What `nnh.toml` holds in this spec.** The file is written in the TOML subset the engine already uses for
  data assets; an empty file is valid. `nnh` reads it and refuses a file it cannot parse with `nnh.toml:<line>: reason`
  and exit 1. The keys it recognises in this spec, and what `nnh` does with unknown keys, are settled by open question
  Q3; checking the engine version against `nnh`'s own is spec 033 (`nnh doctor`).
- **FR-13 Same commands, same layout.** In a game project, `nnh build`, `run`, `install` and `serve` (and the
  `--pc`, `--release`, `--no-build`, `--script` options, `NNH_PRESET`, and spec 031's hot reload once implemented) work
  as in the engine repository: the same six preset names, `build/<preset>/` under the project root, the games manifest
  there, logs and screenshots in `<project>/build/logs/`. A project without a `CMakePresets.json` that defines the
  preset `nnh` needs gets a message naming the missing preset and exit 1, without running CMake.
- **FR-14 Host compiler for 3DS configures.** When `nnh` configures a 3DS preset — in a game project **and in the
  engine repository**, where a fresh `nnh build` of any 3DS preset fails today (the examples' Aseprite exports need
  `nnh-assetc`) — it passes the host C++ compiler to CMake as `NNH_HOST_CXX`, chosen like `scripts/dkp.sh` does: the
  user's `NNH_HOST_CXX` if set, else `g++`, `c++` or `clang++` found on `nnh`'s own `PATH` (devkitPro's MSYS2 shell
  has none on its own). On Windows the value is in MSYS form (`/c/...`): a drive form (`C:/...`) configures but the
  `nnh-assetc` sub-build then fails. The first `nnh build` of a 3DS preset, with a fresh build folder and without
  `scripts/dkp.sh` having run first, then configures and builds, for a game with Tiled maps, Aseprite exports, data
  assets or fonts and for the engine repository with its examples. With no host compiler found, `nnh` passes nothing
  and the **configure** fails with today's message (set `NNH_HOST_CXX` or `NNH_ASSETC`) for a project that needs
  `nnh-assetc`; a project that doesn't configures and builds as today.
- **FR-15 Paths on Windows.** Every path `nnh` hands to CMake in a 3DS preset on Windows is in MSYS form (`/e/...`),
  because devkitPro's MSYS2 CMake reads a drive path (`E:/...`) as relative to the working or build folder: the source
  and build folders, `NNH_HOST_CXX` (FR-14), and the local engine override `FETCHCONTENT_SOURCE_DIR_NNHGE` (FR-1.2),
  which `nnh` converts when it configures a 3DS preset. The developer writes the override once, in one form, in the
  place the docs give (open question Q11), and it works in all six presets on Windows when configured through `nnh`;
  the docs give the `/e/...` form for configuring a 3DS preset by hand through `scripts/dkp.sh`. No path to the engine
  is written in the game's `CMakeLists.txt`.

### Proof and documentation

- **FR-16 Consumer test project.** The engine has a small consumer game project, kept with its tests, that brings the
  engine in through FR-1 and calls `nnh_add_game()` from a nested folder with every keyword of FR-6 and at least one
  asset of each kind (an image, a Tiled map, an Aseprite export, a data asset with a schema, a font and two languages, a
  mesh, a sound, a custom icon, `CIA` with a banner). In PC presets `ctest` configures and builds it in a separate
  build folder and runs it headless; the game prints a self-check line computed from what it loaded. It is also built
  for `3ds-dev` and `3ds-release` with `nnh build` from its own folder, on Windows and on Linux.
- **FR-17 The in-repository build is unchanged.** Inside the engine repository the same targets are configured with the
  same option defaults; `nnh_games.txt` lists the same games; every example's self-check golden is unchanged; the
  `3ds-release` `hello` `.3dsx` is byte-identical before and after (the PC comparison is open question Q5); the build
  budgets of constitution III hold and the clean engine build time does not grow measurably.
- **FR-18 Documented.** The docs explain a game project written by hand (until `nnh new` in spec 033): the minimal
  `CMakeLists.txt` for each of the four ways of FR-1, a `CMakePresets.json` with the six presets, `nnh.toml`, where
  outputs go, how to use a local engine checkout, how to upgrade the pin, the reserved target names, and what changes
  on Windows for 3DS builds. The getting-started guide's "Make your own game" section points to it.

### Command-line length on Windows

- **FR-19 3DS builds stay under the Windows command-line limit, with margin.** On Windows, every command a 3DS preset
  runs to build the engine's libraries and a game's targets (compile, archive, link, packaging) is shorter than the
  Windows limit of 32,767 characters by a set margin (open question Q12), both in the engine repository and in a game
  project where the engine's build folder is nested under the game's (`_deps/nnhge-build/`), with project and build
  folder paths of a stated length (Q12). Today the archive step of the core library already fails out of the tree
  (`Argument list too long`: 35,454 characters for 450 objects) and uses about 80% of the limit inside the repository
  (25,834 characters), so it would fail there too as the core grows. Response files are acceptable only where every path
  in them is readable by the tool that reads them (a general Ninja response file breaks every compile, because it holds
  `/e/...` include paths the native compiler cannot read). The fix does not change PC or Linux builds, nor the
  `3ds-release` `hello` `.3dsx` (FR-17), and the margin is checked so a growing core fails a check, not a developer's
  build.

  *Notes for the plan, not requirements:* candidate approaches seen in the spike are a response file for the archive
  step only (with paths the archiver reads), splitting the core library into a few libraries, and archiving in chunks
  in 3DS presets. A shorter engine binary folder (`FetchContent_Declare(... BINARY_DIR ...)`) builds but only moves the
  limit. The choice is measured (longest command before and after, in and out of the tree) and recorded.

## Acceptance criteria

- **AC-1** `ctest --preset pc-debug -R consumer` (Windows and Linux on VM 120): the consumer project configures and
  builds against the engine sources and its headless run prints its self-check line; the configure log and the
  project's target list contain no example, engine test, `nnh-link` or `nnh` target; its `ctest -N` lists no engine
  test even though the consumer project calls `enable_testing()`; its `nnh_games.txt` lists only its own game(s).
  (FR-1, FR-4, FR-6, FR-16)
- **AC-2** The consumer project built in each way of FR-1 — a `file://` URL to a `git archive` tarball with its
  SHA-256, the same with `FETCHCONTENT_SOURCE_DIR_NNHGE`, `add_subdirectory` of a git submodule checkout, and of a
  pruned copy without examples/tests/apps/specs/docs — gives the same self-check line; with a wrong SHA-256 the
  configure fails naming the URL and nothing is built. (FR-1, FR-2, FR-3, FR-5)
- **AC-3** From the consumer project's folder on Windows: `nnh build <game>` and `nnh build <game> --release` (3DS,
  configured by `nnh`, no prior `dkp.sh` run, a fresh build folder) produce the `.3dsx`; `nnh run <game> --pc --script
  <its .nnhs>` exits 0; the same with the local engine override set as the docs say (Q11), in a drive-form path, for
  `pc-debug` and `3ds-dev`. In the engine repository, a fresh `build/3ds-dev` (folder deleted, no prior `dkp.sh` run)
  configured and built by `nnh build animation` succeeds. With no host compiler on `nnh`'s `PATH` and no
  `NNH_HOST_CXX`, the consumer project's 3DS configure fails with the `NNH_HOST_CXX`/`NNH_ASSETC` message. The same
  `nnh build` on VM 120 with `/opt/devkitpro`. (FR-11, FR-13, FR-14, FR-15)
- **AC-4** `nnh run <game>` from the consumer project on the maintainer's Old 3DS XL: built, sent, the log shown and
  saved under the project's `build/logs/`; its script's verdict PASS (maintainer presses Y). `nnh install <game>`
  installs its `.cia` through FBI. (FR-13, FR-9)
- **AC-5** After building the consumer project in every preset, the engine source folder it used has no new or
  changed file (checked by a file listing with sizes and times, or `git status` on a checkout). (FR-9)
- **AC-6** `nnh_tool_tests` cases: root detection (engine repository; game project; a game project inside the engine
  repository; nested folders below each; nothing found), `nnh.toml` parsing (empty, valid, malformed with its line),
  missing preset message, preset choice in a game project, the `NNH_HOST_CXX` argument (from the environment, found on
  `PATH`, absent) and the MSYS path forms of the 3DS configure command on Windows (source and build folders,
  `NNH_HOST_CXX`, `FETCHCONTENT_SOURCE_DIR_NNHGE`; drive letters upper and lower case, spaces) against the unchanged
  forms on Linux and in PC presets. (FR-11 to FR-15)
- **AC-7** A CMake check in the consumer project: `NNHGE_VERSION` and its three parts equal the engine's project
  version; a project-own plain library target compiles with the flags it had before the engine was added; the game's
  compile commands match an in-repository game's apart from paths. (FR-7, FR-8, FR-10)
- **AC-8** In-repository regression: every example's self-check golden unchanged (`nnh run <example> --pc --script`
  checks of the examples that have them), `nnh_games.txt` the same set, `3ds-release` `hello.3dsx` SHA-256 equal to
  the value measured before the change, `scripts/check_release_strings.sh` passes on `hello`, `ctest --preset pc-debug`
  passes; build budgets measured before and after (one-file edit in `hello`: simulator < 2 s, `.3dsx` < 5 s; clean
  engine < 30 s) and recorded in the ROADMAP table. (FR-17)
- **AC-9** The same one-file edit in the consumer project's game source rebuilds under the same budgets (simulator
  < 2 s, `.3dsx` < 5 s), measured on a game of `hello`'s size, with `ninja -v` showing the compiler cache launcher on
  the game's compile when one is installed; configure time of a fresh consumer build recorded for each way of FR-1
  (spike baseline: fresh 20 s, reconfigure 1.9 s on PC; fresh 20 s on 3DS). (FR-7, constitution III)
- **AC-10** Docs: the hand-written game project page and getting-started §5 updated; `docs/tech/cmake.md` (the
  subproject contract, gated parts, reserved target names, Windows path forms); `docs/guides/nnh-tool.md` (game
  projects, `nnh.toml`, `NNH_HOST_CXX`); a decision row D55; `python -I scripts/docs_tool.py check` reports 0 broken
  links. Following the page verbatim in an empty folder builds and runs a game on PC. (FR-18)
- **AC-11** Command-line margin (Windows, 3DS): a check run in `3ds-dev` and `3ds-release` reads the commands of the
  build (for example from Ninja's command list) and prints the longest one's length and its share of 32,767; it fails
  above the margin of Q12. It passes for the engine repository and for the consumer project built through
  `FetchContent` under a project path of the length Q12 states; the before/after lengths of the archive step are
  recorded (today 25,834 in the repository, 35,454 out of the tree). The consumer `3ds-dev` and `3ds-release` builds of
  AC-3 finish with no `Argument list too long`. (FR-19, FR-17)
- **AC-12** Unknown platform: a CMake check configures a scratch project whose engine platform value is empty or
  unsupported (a test-only override) and expects the configure to fail with the message of FR-6 naming the game and the
  value, before any game target is generated. (FR-6)
- **AC-13** Explicit options: the consumer project configured with the examples, tests, apps and tools options set ON
  has those targets (and the engine's tests in its `ctest -N`); configured with all of them OFF and maps, sprites, data
  and fonts in its assets, it still builds (the host programs stay). (FR-4)

## Non-goals

- `nnh new` (a generated project and its template), `nnh doctor`, `nnh --version` and the newcomer guide: spec 033.
- The version number 1.0.0, `<nnh/version.hpp>`, the stability promise, `CHANGELOG.md`, release archives and their
  hosting, release workflows, prebuilt `nnh` binaries: spec 034. This spec only needs *a* source archive (made locally
  for the tests) and the CMake version variables.
- An installed CMake package with prebuilt engine libraries (`find_package(nnhge)`), package managers, a CMake
  presets file shipped by the engine for games to `include`.
- Upgrading a game project between engine versions automatically; several engine versions in one build.
- Installing or repackaging devkitPro, SDL3 or compilers.
- Building NNH Link or the `nnh` tool from a game project (they come from an engine checkout until spec 034's prebuilt
  `nnh`).
- macOS and MSVC as checked hosts (they are not checked today either).

## Constraints

- **III Build speed.** The game's one-file edit budgets (simulator < 2 s, `.3dsx` < 5 s) hold in a game project, which
  is why FR-4 keeps the engine's extras out and FR-7 keeps the compiler cache; the clean engine build (< 30 s) and the
  in-repository build are measured before and after (AC-8, AC-9). The consumer test adds one engine build per PC
  preset to `ctest`; its cost is open question Q4.
- **I Same game code everywhere.** A game's sources and `nnh_add_game()` call are the same inside and outside the
  engine repository, on PC and on 3DS (FR-6, FR-7); no platform switch is asked of the game, and no Windows-only path
  form is written in the game's `CMakeLists.txt` (FR-15).
- **Windows as a 3DS build host.** devkitPro's toolchain runs from its MSYS2 on Windows: MSYS paths for its CMake
  (FR-14, FR-15) and the native 32,767-character command-line limit for its tools (FR-19) are constraints of the
  maintainer's main host, checked by AC-3 and AC-11.
- **VI Spec first, test first.** The CMake contract is proven by the consumer test (FR-16), `nnh`'s new behaviour by
  `nnh_tool_tests` written first (AC-6); the console behaviour by AC-4.
- **VII Small, organized files** and the Modern CMake rules in `docs/tech/cmake.md` (targets and properties, explicit
  source lists, `cmake_parse_arguments`) apply to every helper changed.
- **VIII Documented API.** `nnh_add_game()`, the subproject contract and the `nnh.toml` marker are public surface:
  documented with the change (FR-18).
- **IX Clean and legal.** The consumer project's assets are engine-made (zlib) or test fixtures already in the
  repository; no devkitPro files are fetched or shipped by the engine.
- **X Safe by default.** Dependencies pinned: the documented recipe pins the engine archive by SHA-256 and the engine
  keeps its own pins (FR-2); `nnh` only reads `nnh.toml` (a bounded parser of external data, fuzzed like the other TOML
  subset readers) and still talks only to LAN addresses (spec 010 FR-7).
- **XI Opt-in.** Nothing changes for a game that stays inside the engine repository; the four ways of FR-1 are equal
  choices.
- CMake ≥ 3.25 and presets version 6 stay the minimum (as `CMakePresets.json` says today), so the recipe cannot rely on
  newer `FetchContent` options such as `EXCLUDE_FROM_ALL` (3.28).
- One implementation at a time: after spec 031 is merged (both change `nnh`); ≥ 10 GB free on the maintainer's `E:`
  drive before the consumer builds.

## Context (the study's step 1 spike, 2026-11-27)

The [release study](../../docs/plans/2026-11-26-release-1.0.md#spike-results-2026-11-27) built a scratch game outside
the repository against `main` (`3f4b4c8`), on Windows only (native CMake 4.4.2 + WinLibs GCC 16.2; devkitPro's MSYS2
CMake 4.0.2): `FetchContent_Declare(nnhge SOURCE_DIR <engine>)` + `FetchContent_MakeAvailable`, a scene drawing text,
a PNG sprite and a data asset value, a copy of the engine's `CMakePresets.json`; `pc-debug` and `3ds-dev`, through
`scripts/dkp.sh` and through devkitPro's bash as `nnh` does. Workarounds lived only in the scratch project; the engine
checkout's `git status` stayed clean after every build (FR-9 holds for the engine source today). Results:

- **Helpers read directory variables that are empty in the game's scope (confirmed; FR-6).** PC and 3DS configures
  fail with `No download info given for 'mygame_data_schema' and its source directory: /tools/data_schema`; without
  `DATA_SCHEMA`, the PC build fails on `'/nnh_check_image.cmake' ... missing`, and `build.ninja` holds
  `/nnh_check_icon.cmake` and `/default_icon.png`. Nine values are involved: `NNH_PLATFORM`, `_NNH_GAME_CMAKE_DIR`,
  `_NNH_ASSETS_DIR`, `_NNH_CIA_DIR`, `_NNH_CTR_TOOLS_DIR`, `_NNH_IMAGES_DIR`, `_NNH_PACKAGE_ART`, `_NNH_DATA_ROOT` and
  `_NNH_DATA_HOST_SOURCES` (the last one missing from the study's list). Setting the nine in the game's scope was enough
  for both systems to build and run (PC screenshot with text, sprite and data string; `mygame.3dsx` made).
- **An empty platform silently takes wrong branches (FR-6's error).** On PC the data schema program took the
  cross-preset ExternalProject path instead of a plain target; on 3DS without `DATA_SCHEMA` the configure succeeded but
  produced only `mygame.elf` (no SMDH, no `.3dsx`, no `dist`) and copied PNGs unconverted.
- **Examples, tests and tools are built with the game (confirmed; FR-4).** The game's `pc-debug` had 46 executables
  (26 examples, the engine's tests, NNH Link and its tests, `nnh` and its tests, `nnh-assetc` and its tests, 6 data
  schema programs, the game). Building only the game: 606 steps, 87 s; a plain `cmake --build`: 734 more steps, 224 s
  more, 818 MB in all. `nnh_games.txt` listed 27 games, so `nnh serve` would offer the examples. `ctest -N` showed the
  engine's 381 tests once the game called `enable_testing()`. On 3DS the examples and NNH Link added 55 s.
  `-DNNH_BUILD_EXAMPLES=OFF` already works; NNH Link and `nnh` have no option.
- **`nnh` doesn't find a game project (confirmed; FR-11).** `nnh run mygame --pc` exits 1 with "run it inside the
  NNHGE repository (no CMakePresets.json above here)" although the project has one: the missing marker is
  `engine/core`, so the message is misleading too. With a stand-in marker, `nnh run mygame --pc` built, ran with hot
  reload, attached the remote and saved its log under `<project>/build/logs/`; `nnh build` in `3ds-dev` built the
  `.3dsx`. The root is the only blocker on PC.
- **No `NNH_HOST_CXX` from `nnh` breaks 3DS configures, in the engine repository too (FR-14).** devkitPro's bash has no
  host compiler on its `PATH`; a fresh `3ds-dev` configure run as `nnh` runs it fails **at configure time** ("This game
  has Aseprite exports (.ase.json), which need nnh-assetc: no host C++ compiler", raised by `examples/animation`; with
  examples off, by the game's own data assets). `-DNNH_HOST_CXX=/c/.../g++.exe` builds; `C:/.../g++.exe` configures,
  then the `nnh-assetc` sub-build fails.
- **Drive paths don't work in MSYS CMake (FR-15, Q9).** `FETCHCONTENT_SOURCE_DIR_NNHGE=E:/...` and
  `FetchContent_Declare(SOURCE_DIR E:/...)` are read as relative paths (`<cwd>/E:/...` or `<build>/E:/...`); `/e/...`
  works. The `/e/...` form is untested in native PC CMake, so no single spelling is known to work in all six presets.
- **3DS out-of-tree builds exceed the Windows command-line limit (new blocker; FR-19).** Archiving the core library
  failed with `arm-none-eabi-gcc-ar.exe: Argument list too long`: 35,454 characters for 450 objects, against 32,767;
  the `_deps/nnhge-build/` prefix adds 17 characters per object. Inside the repository that list is 25,834 characters
  (about 80%). MSYS CMake doesn't switch to a response file (it assumes the POSIX limit);
  `CMAKE_NINJA_FORCE_RESPONSE_FILE=ON` breaks every compile (the `.rsp` files hold `/e/...` include paths the native
  compiler can't read); a shorter engine binary folder builds but only moves the limit.
- **Game targets get no compiler cache and are absent from `compile_commands.json` (FR-7, Q7).** The launcher is set
  only in the engine's scope. PC one-file edit of the game: 2.56–2.68 s without the cache, 2.05–2.16 s with the
  launcher set in the game's scope (of which a 1.26 s Debug link and a 0.16 s check of the globbed asset folder),
  against the < 2 s budget (`hello` in the repository: 1.63–1.77 s). 3DS: 4.29–4.44 s, within < 5 s (devkitPro's MSYS2
  has no ccache either way).
- **Configure and clean times (baseline for AC-9).** PC: fresh 20 s (mostly compiler detection), reconfigure 1.9 s.
  3DS through `dkp.sh`: fresh 20 s. The game alone from clean: PC 87 s, 3DS 65 s (450–600 engine steps). Peak disk of
  the spike 1.1 GB.
- The manifest, `dist/` and fetched tools already go to the top build folder (FR-9 keeps it so).

**Not covered by the spike** (left to the consumer fixture, FR-16, AC-2 and AC-3): the `URL file://` + `URL_HASH`
form, `add_subdirectory` of a `git archive` or a submodule, an `https://` download from MSYS CMake, Linux on VM 120,
the `/e/...` override form in native PC CMake, and the keywords and asset kinds `IMAGES`, `CIA`, `LANGUAGES`, `ICON`,
Tiled maps, Aseprite sprites, fonts and meshes.

Prior art: CMake's own guidance for dependencies is `FetchContent` with `FETCHCONTENT_SOURCE_DIR_<NAME>` as the local
override and `PROJECT_IS_TOP_LEVEL` (3.21+) to keep a dependency's tests and examples out
([FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html),
[PROJECT_IS_TOP_LEVEL](https://cmake.org/cmake/help/latest/variable/PROJECT_IS_TOP_LEVEL.html)). GitHub's automatic
"Source code" archives are not promised byte-stable, while files uploaded to a release are
([GitHub blog, 2023](https://github.blog/open-source/git/update-on-the-future-stability-of-source-code-archives-and-hashes/),
[changelog](https://github.blog/changelog/2023-01-30-git-archive-checksums-may-change/)), so a hash pin should point
at an uploaded release asset (spec 034), not at an automatic archive.

## Open questions

Answered 2026-11-27: the maintainer approved every recommendation below; if FR-19 needs the core split, `hello` may change once, sizes and symbols checked, and FR-17's byte identity holds again from then on.

The first three are the release study's decisions 1–3 (pending the maintainer's review of the study); the rest came up
while writing this spec. Q7 and Q9 were closed by the spike (2026-11-27); Q11 and Q12 came from it.

- [x] **Q1 Default way of bringing the engine in** (study decision 1). Options: A `FetchContent` of a release source
  archive by URL + SHA-256 with the `FETCHCONTENT_SOURCE_DIR_NNHGE` override; B git submodule; C vendored copy;
  D installed package (`find_package`). — *Recommendation (study): A as the documented default (and what `nnh new`
  generates in spec 033), on one contract, "the engine works as an `add_subdirectory()`/FetchContent subproject", so B
  and C work too and get a paragraph each; D stays a non-goal. Until spec 034 publishes a release asset, the docs show
  the URL form with a placeholder and the tests use a locally made archive; never pin GitHub's automatic archives.*
- [x] **Q2 Presets in a game project** (study decision 2). Options: A a full `CMakePresets.json` in the project with the
  same six names and `build/<preset>` layout; B `include` a presets file shipped by the engine; C only `pc-dev` and
  `3ds-dev`. — *Recommendation (study): A, with the local engine override kept in the git-ignored
  `CMakeUserPresets.json`. B cannot work with a fetched engine (presets are read before the fetch); C would make
  `--release` and `NNH_PRESET` behave differently from the engine repository. In this spec the file is documented to
  copy; spec 033 generates it.*
- [x] **Q3 What marks a game project, and what `nnh.toml` holds** (study decision 3). Options: A an `nnh.toml` at the
  root; B recognise a `CMakePresets.json` with the preset names; C a CMake cache entry read after configure. —
  *Recommendation (study): A, nearest marker wins (FR-11). For this spec: optional keys `name` (text) and `engine`
  (the engine version the project was made for, text), unknown keys ignored with a one-line warning so later specs can
  add keys without breaking older `nnh` builds; `engine` is only stored and shown in this spec, compared by
  `nnh doctor` in spec 033.*
- [x] **Q4 Cost of the consumer test in `ctest`.** Building the consumer project four ways (AC-2) means four more
  engine builds per run. — *Recommendation: the default `ctest` builds it one way (the local source folder, which is
  also what a contributor uses), reusing the compiler cache; the other ways of AC-2 run under a ctest label
  (`consumer-full`) that the release checklist and VM 120 runs use. 3DS builds of it stay out of `ctest` (3DS presets
  have no tests) and are run by `nnh build` (AC-3).*
- [x] **Q5 "hello byte-identical" on PC.** Windows executables carry a link timestamp and Debug builds carry source
  paths, so byte identity may fail for reasons unrelated to this spec. — *Recommendation: byte-identical `.3dsx` for
  `3ds-release` `hello` (and its `.elf` `.text` section); on PC, the same `.text` size for `pc-release` `hello` plus
  every self-check golden unchanged.*
- [x] **Q6 Must the engine configure without its examples/tests/apps folders (FR-5)?** — *Recommendation: yes. It costs
  little (the same gates as FR-4) and leaves spec 034 free to ship a slim engine archive, and vendored copies free to
  drop those folders; the full source archive still contains everything.*
- [x] **Q7 Compiler cache and compile commands for game targets (FR-7).** Options: the engine applies its detected
  compiler cache to the targets `nnh_add_game()` creates; or the game project sets it itself. — **Closed by the spike
  (2026-11-27), as recommended:** the engine applies it to the targets `nnh_add_game()` creates only (not to the
  project's other targets, FR-8), with `NNH_CCACHE` empty to turn it off; `CMAKE_EXPORT_COMPILE_COMMANDS` is left to the
  game project (the presets of Q2 turn it on). Evidence: the game's PC one-file edit took 2.56–2.68 s without the cache
  and 2.05–2.16 s with the launcher, against `hello`'s 1.63–1.77 s in the repository; without it a game project misses
  the < 2 s budget by more. AC-9 measures it on a game of `hello`'s size.
- [x] **Q8 One download per preset.** `FetchContent` downloads into each preset's build folder, so six presets fetch
  the archive six times (a few MB each). — *Recommendation: accept it for this spec; document `FETCHCONTENT_SOURCE_DIR_NNHGE`
  pointing at one unpacked copy for offline or slow connections; revisit a shared download folder only if the newcomer
  test (study step 7) shows it hurts.*
- [x] **Q9 Path form of the local override in 3DS presets on Windows (FR-15).** devkitPro's MSYS2 CMake may misread
  `E:/...`. Options: document an MSYS form for 3DS presets; have the engine normalise the override path; have `nnh`
  pass it in the right form. — **Closed by the spike (2026-11-27):** MSYS CMake reads `E:/...` as a relative path in
  both `FETCHCONTENT_SOURCE_DIR_NNHGE` and `FetchContent_Declare(SOURCE_DIR ...)`; `/e/...` works. So `nnh` converts the
  override, like the source and build folders and `NNH_HOST_CXX`, to MSYS form (`/e/...`) when it configures a 3DS
  preset on Windows; the docs give the `/e/...` form for configuring by hand through `scripts/dkp.sh` and keep engine
  paths out of the game's `CMakeLists.txt` (a hard-coded `E:/` there breaks the 3DS configure). Where the developer
  writes the override so `nnh` can convert it is Q11.
- [x] **Q10 Three release specs (study pending decision 4).** — *Recommendation (study): three specs, 032 → 033 → 034,
  implemented in that order after 031; drafted together. The study's other pending decisions (7b experimental modules,
  9 release candidate) belong to spec 034 and are not decided here.*
- [x] **Q11 Where the local engine override is written (FR-1.2, FR-15).** Since no single spelling works in both
  native and MSYS CMake, `nnh` must see the value to convert it. Options: A in the git-ignored `CMakeUserPresets.json`
  (a cache variable in drive form); `nnh` reads it from the chosen preset and, for a 3DS configure on Windows, passes
  `-DFETCHCONTENT_SOURCE_DIR_NNHGE=/e/...` (a command-line `-D` overrides the preset's value); B a per-preset value, the
  developer writing `/e/...` in the 3DS presets and `E:/...` in the PC ones; C an environment variable `nnh` reads and
  turns into `-D` for every preset; D a key in a git-ignored local file next to `nnh.toml`. — *Recommendation: A. It is
  CMake's own place for the override (Q2), works for a developer who runs `cmake --preset` by hand on PC, and needs one
  spelling; B is error-prone, C is invisible to plain `cmake`, D adds a file. Confirm in the plan that `/e/...` is
  never needed in native PC CMake (not tested by the spike).*
- [x] **Q12 Margin and test paths for the command-line limit (FR-19, AC-11).** — *Recommendation: the longest 3DS
  command must stay at or below 50% of 32,767 (16,383 characters) in the engine repository and in the consumer project
  built through `FetchContent`, measured with the consumer project at a path of at least 64 characters (the maintainer's
  scratch path was about 30); 50% leaves room for the core to roughly double before the check fails, and the check
  fails at that point rather than a developer's build at 100%. If the chosen approach makes the length independent of
  the object count (an archive-step response file), the check still runs and the margin is met trivially.*
