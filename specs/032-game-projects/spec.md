# Spec 032: Game projects outside the engine repository

Status: draft
Author: NNHGE maintainers · Date: 2026-11-26
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
  - the engine's options that select these parts (`NNH_BUILD_EXAMPLES`, `NNH_BUILD_TESTS` and any new one this spec
    adds) default to whether the engine is the top-level project and can still be set explicitly by the game.
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
- **FR-14 Host compiler for 3DS configures.** When `nnh` configures a 3DS preset (in a game project or the engine
  repository), it passes the host C++ compiler to CMake as `NNH_HOST_CXX`, chosen like `scripts/dkp.sh` does: the
  user's `NNH_HOST_CXX` if set, else `g++`, `c++` or `clang++` found on `nnh`'s own `PATH`, in the path form
  devkitPro's MSYS2 CMake reads on Windows. The first `nnh build` of a 3DS game with Tiled maps, Aseprite exports, data
  assets or fonts then succeeds without `scripts/dkp.sh` having run first. With no host compiler, the configure goes on
  and the build fails with today's message (set `NNH_HOST_CXX` or `NNH_ASSETC`).
- **FR-15 Paths on Windows.** Every path `nnh` hands to CMake in a 3DS preset on Windows (source and build folders,
  `NNH_HOST_CXX`) is in the form devkitPro's MSYS2 CMake reads, and the documented way of writing the local engine
  override (FR-1.2) works in all six presets on Windows, including the 3DS ones configured through devkitPro's MSYS2.

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

## Acceptance criteria

- **AC-1** `ctest --preset pc-debug -R consumer` (Windows and Linux on VM 120): the consumer project configures and
  builds against the engine sources and its headless run prints its self-check line; the configure log and the
  project's target list contain no example, engine test, `nnh-link` or `nnh` target; its `ctest -N` lists no engine
  test. (FR-1, FR-4, FR-6, FR-16)
- **AC-2** The consumer project built in each way of FR-1 — a `file://` URL to a `git archive` tarball with its
  SHA-256, the same with `FETCHCONTENT_SOURCE_DIR_NNHGE`, `add_subdirectory` of a git submodule checkout, and of a
  pruned copy without examples/tests/apps/specs/docs — gives the same self-check line; with a wrong SHA-256 the
  configure fails naming the URL and nothing is built. (FR-1, FR-2, FR-3, FR-5)
- **AC-3** From the consumer project's folder on Windows: `nnh build <game>` and `nnh build <game> --release` (3DS,
  configured by `nnh`, no prior `dkp.sh` run, a fresh build folder) produce the `.3dsx`; `nnh run <game> --pc --script
  <its .nnhs>` exits 0. The same `nnh build` on VM 120 with `/opt/devkitpro`. (FR-11, FR-13, FR-14, FR-15)
- **AC-4** `nnh run <game>` from the consumer project on the maintainer's Old 3DS XL: built, sent, the log shown and
  saved under the project's `build/logs/`; its script's verdict PASS (maintainer presses Y). `nnh install <game>`
  installs its `.cia` through FBI. (FR-13, FR-9)
- **AC-5** After building the consumer project in every preset, the engine source folder it used has no new or
  changed file (checked by a file listing with sizes and times, or `git status` on a checkout). (FR-9)
- **AC-6** `nnh_tool_tests` cases: root detection (engine repository; game project; a game project inside the engine
  repository; nested folders below each; nothing found), `nnh.toml` parsing (empty, valid, malformed with its line),
  missing preset message, preset choice in a game project, the `NNH_HOST_CXX` argument and path forms of the 3DS
  configure command on Windows and Linux. (FR-11 to FR-15)
- **AC-7** A CMake check in the consumer project: `NNHGE_VERSION` and its three parts equal the engine's project
  version; a project-own plain library target compiles with the flags it had before the engine was added; the game's
  compile commands match an in-repository game's apart from paths. (FR-7, FR-8, FR-10)
- **AC-8** In-repository regression: every example's self-check golden unchanged (`nnh run <example> --pc --script`
  checks of the examples that have them), `nnh_games.txt` the same set, `3ds-release` `hello.3dsx` SHA-256 equal to
  the value measured before the change, `scripts/check_release_strings.sh` passes on `hello`, `ctest --preset pc-debug`
  passes; build budgets measured before and after (one-file edit in `hello`: simulator < 2 s, `.3dsx` < 5 s; clean
  engine < 30 s) and recorded in the ROADMAP table. (FR-17)
- **AC-9** The same one-file edit in the consumer project's game source rebuilds under the same budgets (simulator
  < 2 s, `.3dsx` < 5 s); configure time of a fresh consumer build recorded for each way of FR-1. (FR-7, constitution III)
- **AC-10** Docs: the hand-written game project page and getting-started §5 updated; `docs/tech/cmake.md` (the
  subproject contract, gated parts, reserved target names, Windows path forms); `docs/guides/nnh-tool.md` (game
  projects, `nnh.toml`, `NNH_HOST_CXX`); a decision row D55; `python -I scripts/docs_tool.py check` reports 0 broken
  links. Following the page verbatim in an empty folder builds and runs a game on PC. (FR-18)

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
  engine repository, on PC and on 3DS (FR-6, FR-7); no platform switch is asked of the game.
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

## Context (from the study; the spike of the study's step 1 has not been run yet)

What the [release study](../../docs/plans/2026-11-26-release-1.0.md) found in the code, to be confirmed by its step 1
spike (an out-of-tree game built against `main` on Windows PC, Windows 3DS through devkitPro's MSYS2, and VM 120) before
the plan is written; its findings are added here then.

- The helpers read variables set in the engine's directory when its files were included (the paths of the engine's
  CMake scripts, art and data sources) and the platform variable set in the engine's top-level directory; a game
  calling `nnh_add_game()` from its own directory would see them empty.
- The engine's top-level directory also sets the C++ standard defaults and the compiler cache launcher for its own
  directory; the language standard reaches games through the engine target, the compiler cache would not (FR-7).
- Examples and tests default to ON; NNH Link and, in PC presets, the `nnh` tool are always added.
- `nnh` requires `CMakePresets.json` and `engine/core` to find its root, and does not pass `NNH_HOST_CXX`
  (`scripts/dkp.sh` does), so a first 3DS configure by `nnh` of a game with maps or data may lack a host compiler.
- The manifest, `dist/` and fetched tools already go to the top build folder, which in a game project is the game's
  (FR-9 keeps it so).
- devkitPro's MSYS2 CMake misreads `C:/...` paths in some places (`docs/tech/cmake.md`, host tools), which matters for
  the local override in 3DS presets on Windows (FR-15).

Prior art: CMake's own guidance for dependencies is `FetchContent` with `FETCHCONTENT_SOURCE_DIR_<NAME>` as the local
override and `PROJECT_IS_TOP_LEVEL` (3.21+) to keep a dependency's tests and examples out
([FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html),
[PROJECT_IS_TOP_LEVEL](https://cmake.org/cmake/help/latest/variable/PROJECT_IS_TOP_LEVEL.html)). GitHub's automatic
"Source code" archives are not promised byte-stable, while files uploaded to a release are
([GitHub blog, 2023](https://github.blog/open-source/git/update-on-the-future-stability-of-source-code-archives-and-hashes/),
[changelog](https://github.blog/changelog/2023-01-30-git-archive-checksums-may-change/)), so a hash pin should point
at an uploaded release asset (spec 034), not at an automatic archive.

## Open questions

The first three are the release study's decisions 1–3 (pending the maintainer's review of the study); the rest came up
while writing this spec.

- [ ] **Q1 Default way of bringing the engine in** (study decision 1). Options: A `FetchContent` of a release source
  archive by URL + SHA-256 with the `FETCHCONTENT_SOURCE_DIR_NNHGE` override; B git submodule; C vendored copy;
  D installed package (`find_package`). — *Recommendation (study): A as the documented default (and what `nnh new`
  generates in spec 033), on one contract, "the engine works as an `add_subdirectory()`/FetchContent subproject", so B
  and C work too and get a paragraph each; D stays a non-goal. Until spec 034 publishes a release asset, the docs show
  the URL form with a placeholder and the tests use a locally made archive; never pin GitHub's automatic archives.*
- [ ] **Q2 Presets in a game project** (study decision 2). Options: A a full `CMakePresets.json` in the project with the
  same six names and `build/<preset>` layout; B `include` a presets file shipped by the engine; C only `pc-dev` and
  `3ds-dev`. — *Recommendation (study): A, with the local engine override kept in the git-ignored
  `CMakeUserPresets.json`. B cannot work with a fetched engine (presets are read before the fetch); C would make
  `--release` and `NNH_PRESET` behave differently from the engine repository. In this spec the file is documented to
  copy; spec 033 generates it.*
- [ ] **Q3 What marks a game project, and what `nnh.toml` holds** (study decision 3). Options: A an `nnh.toml` at the
  root; B recognise a `CMakePresets.json` with the preset names; C a CMake cache entry read after configure. —
  *Recommendation (study): A, nearest marker wins (FR-11). For this spec: optional keys `name` (text) and `engine`
  (the engine version the project was made for, text), unknown keys ignored with a one-line warning so later specs can
  add keys without breaking older `nnh` builds; `engine` is only stored and shown in this spec, compared by
  `nnh doctor` in spec 033.*
- [ ] **Q4 Cost of the consumer test in `ctest`.** Building the consumer project four ways (AC-2) means four more
  engine builds per run. — *Recommendation: the default `ctest` builds it one way (the local source folder, which is
  also what a contributor uses), reusing the compiler cache; the other ways of AC-2 run under a ctest label
  (`consumer-full`) that the release checklist and VM 120 runs use. 3DS builds of it stay out of `ctest` (3DS presets
  have no tests) and are run by `nnh build` (AC-3).*
- [ ] **Q5 "hello byte-identical" on PC.** Windows executables carry a link timestamp and Debug builds carry source
  paths, so byte identity may fail for reasons unrelated to this spec. — *Recommendation: byte-identical `.3dsx` for
  `3ds-release` `hello` (and its `.elf` `.text` section); on PC, the same `.text` size for `pc-release` `hello` plus
  every self-check golden unchanged.*
- [ ] **Q6 Must the engine configure without its examples/tests/apps folders (FR-5)?** — *Recommendation: yes. It costs
  little (the same gates as FR-4) and leaves spec 034 free to ship a slim engine archive, and vendored copies free to
  drop those folders; the full source archive still contains everything.*
- [ ] **Q7 Compiler cache and compile commands for game targets (FR-7).** Options: the engine applies its detected
  compiler cache to the targets `nnh_add_game()` creates; or the game project sets it itself. — *Recommendation: the
  engine applies it to game targets only (not to the project's other targets, FR-8), with `NNH_CCACHE` empty to turn
  it off; `CMAKE_EXPORT_COMPILE_COMMANDS` is left to the game project (the presets of Q2 turn it on).*
- [ ] **Q8 One download per preset.** `FetchContent` downloads into each preset's build folder, so six presets fetch
  the archive six times (a few MB each). — *Recommendation: accept it for this spec; document `FETCHCONTENT_SOURCE_DIR_NNHGE`
  pointing at one unpacked copy for offline or slow connections; revisit a shared download folder only if the newcomer
  test (study step 7) shows it hurts.*
- [ ] **Q9 Path form of the local override in 3DS presets on Windows (FR-15).** devkitPro's MSYS2 CMake may misread
  `E:/...`. Options: document an MSYS form for 3DS presets; have the engine normalise the override path; have `nnh`
  pass it in the right form. — *Recommendation: decide from the study's step 1 spike; prefer one form that works in all
  six presets (forward-slash drive paths if the spike shows MSYS CMake accepts them for `FETCHCONTENT_SOURCE_DIR_*`),
  else `nnh` converts it when it configures a 3DS preset, as it already does for the source folder.*
- [ ] **Q10 Three release specs (study pending decision 4).** — *Recommendation (study): three specs, 032 → 033 → 034,
  implemented in that order after 031; drafted together. The study's other pending decisions (7b experimental modules,
  9 release candidate) belong to spec 034 and are not decided here.*
