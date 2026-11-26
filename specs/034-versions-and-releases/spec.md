# Spec 034: Versions, API stability and releases

Status: draft
Author: NNHGE maintainers · Date: 2026-11-26
Depends on: [spec 032](../032-game-projects/spec.md) (draft: the engine as a subproject, `NNHGE_VERSION`, a source
archive pinned by URL and SHA-256), [spec 033](../033-nnh-new-and-doctor/spec.md) (draft: `nnh --version`, `nnh new`
writing the pin this spec bakes into release `nnh` binaries, `nnh doctor` comparing versions),
[spec 027](../027-save-versions/spec.md) (the save format promised to players), [spec 013](../013-console-remote/spec.md)
(`.nnhs` scripts), [spec 010](../010-nnh-cli-run/spec.md) (`nnh` commands and exit codes),
[spec 009](../009-packaging/spec.md) (`.cia`), [spec 011](../011-nnh-link/spec.md) (NNH Link). Implemented after
spec 033. Third of the three release specs proposed by the [1.0 release study](../../docs/plans/2026-11-26-release-1.0.md)
(032 game projects, 033 `nnh new`/`nnh doctor`, 034 this spec); the study is a draft the maintainer has not reviewed, so
its decisions are open questions below, not settled. Lean path ([sdd.md](../../docs/process/sdd.md#lean-path)): mostly
process, CI and docs, plus one small public header.

> **Public and effectively irreversible steps.** Pushing a release tag and publishing a GitHub release are visible to
> everyone at once, and game projects pin the published archive by its SHA-256, so a published release can never be
> changed, only superseded. **Each of these two steps needs the maintainer's explicit OK, every time, for that tag.**
> No script, workflow or agent in this spec pushes a tag or publishes a release on its own (FR-25).

## Problem and goal

NNHGE has no version a game developer can rely on: the CMake project says `0.1.0`, there is no version a game can read
in C++, no tag, no release, no changelog and no written promise of what may change. A newcomer has nothing to download
but a clone of `main`, and a developer who wrote a game last month cannot tell whether pulling the engine again will
break it. After this spec the engine has one version number (SemVer), readable in CMake and C++ on PC and 3DS; a page
says exactly which parts are stable through 1.x, which are not, and which are experimental; deprecated APIs warn at
compile time before they are removed; and a repeatable, checked release process produces a GitHub release with a
hash-pinnable source archive, prebuilt `nnh` for Windows and Linux, the examples for the 3DS, checksums and notes taken
from a hand-written changelog. NNHGE 1.0 ships as `v1.0.0-rc.1`, is tried by building a first small game against it,
and then becomes `v1.0.0`.

## User stories

- As a game developer, I want to read the engine's version in C++ and CMake, so that my game can show it in a credits
  screen and my project can refuse an engine that is too old.
- As a game developer, I want one page that says what the engine promises not to break in 1.x, so that I know which
  APIs I can build on and which may still move.
- As a game developer, I want a deprecated function to warn me when I compile, with what to use instead, so that I fix
  my code before the next major version removes it.
- As a game developer, I want a save file written by my game on a player's SD card to stay readable after I update the
  engine, so that an update never wipes a player's progress.
- As a newcomer, I want to download a release (one `nnh` binary and a pinned engine) instead of cloning and building
  the engine, so that my first game is minutes away.
- As a game developer, I want to check that what I downloaded is what was published (`SHA256SUMS`), so that I can trust
  an unsigned binary.
- As a game developer, I want a changelog in plain words per release, so that I know what an upgrade brings and what
  I have to change.
- As a 3DS owner without a compiler, I want the examples as `.3dsx` (and a `.cia`), so that I can try the engine on
  my console at once.
- As the maintainer, I want the release built by a written checklist and repeatable scripts on clean machines, with
  nothing published without my word, so that a release is boring, auditable and never accidental.
- As an engine contributor, I want the build to tell me when a change breaks the stable surface, so that I don't break
  games by accident.

## Functional requirements

### The version

- **FR-1 One number.** The engine has exactly one version, `MAJOR.MINOR.PATCH` with an optional pre-release label
  (`1.0.0-rc.1`), following [SemVer 2.0.0](https://semver.org/). It is written in one place in the engine's build; the
  CMake variables of spec 032 FR-10 (`NNHGE_VERSION`, `_MAJOR`, `_MINOR`, `_PATCH`), the C++ header of FR-2, `nnh
  --version` (spec 033 FR-1), the release file names (FR-15) and the release tag (FR-6) all show this number, and the
  pre-release label where the form allows one (open question Q4).
- **FR-2 `<nnh/version.hpp>`.** A public header, usable in game code and in `#if`, the same on PC and 3DS:
  - macros `NNH_VERSION_MAJOR`, `NNH_VERSION_MINOR`, `NNH_VERSION_PATCH`, a single comparable integer `NNH_VERSION`
    (so `#if NNH_VERSION >= NNH_VERSION_OF(1, 2, 0)` works) and the text `NNH_VERSION_STRING` (`"1.0.0-rc.1"`);
  - `constexpr` functions in `nnh::` returning the same values (`nnh::version()` as a small value with the three parts
    and the label, `nnh::version_string()`), so code can compare versions without the preprocessor.
  It includes no other header beyond what it strictly needs, contains no commit id, date or machine-dependent text, and
  allocates nothing (constitution III, V). The umbrella header includes it.
- **FR-3 Header and build agree.** A build or test step fails when the C++ version and the CMake version differ (one
  source of truth, either by generating the header or by checking it against the build).
- **FR-4 Version in the log.** In Debug and Development builds, a game logs one line at start naming the engine version
  and flavour (Release builds stay silent, spec 002's rule), so a console log or a bug report always says which engine
  ran (open question Q19).
- **FR-5 Version rules.** A release's number follows SemVer applied to the stable surface of FR-7:
  - **PATCH**: bug fixes, performance, docs; no new or changed public API;
  - **MINOR**: additions to the stable surface, deprecations (FR-11), any change to experimental parts (FR-9), new
    commands or options of `nnh`, new keywords of `nnh_add_game()`, new `.nnhs` commands;
  - **MAJOR**: anything that breaks FR-7's promise.
  Pre-releases are `-rc.N` (N from 1) and are not covered by the promise between each other. Between releases `main`
  carries a version that cannot be mistaken for a release (Q4).
- **FR-6 Tags.** Each release is one **annotated** git tag `vX.Y.Z` or `vX.Y.Z-rc.N` on a commit of `main` whose engine
  version (FR-1) equals the tag. A tag is never moved; a tag whose release was published is never deleted.

### The 1.x promise

- **FR-7 Stable in 1.x.** A stability page states that, from `v1.0.0` until `v2.0.0`, the following are kept, so that a
  game that builds and runs with 1.x builds and runs the same with any later 1.y after recompiling:
  1. the game-facing public headers `<nnh/*.hpp>` (not `nnh/detail/`), their documented names, signatures and behaviour,
     except the parts marked experimental (FR-9); the 3DS extension headers `<nnh/3ds/*.hpp>` (Q2);
  2. `nnh_add_game()` and its keywords, the `nnh::engine` target, the `NNHGE_VERSION*` variables, the engine options a
     game may set, and the six preset names;
  3. the **source input formats** the build accepts: the images, sounds, Tiled `.tmj` subset, Aseprite JSON exports,
     glTF subset, fonts, and the data-asset and string-file TOML subsets: a source file a 1.x build accepts is accepted
     by every later 1.y with the same meaning;
  4. the **save formats** written on players' devices: spec 002's container, spec 027's save format and spec 017's
     bindings overrides: a file written by any 1.x is read by every later version (Q7);
  5. the `nnh` commands, their options and exit codes, `nnh.toml` keys (specs 032/033), and the `.nnhs` script language:
     a script that is valid in 1.x is valid and means the same in every later 1.y.
  The page says what a minor release may still do to stable parts without breaking the promise, and what game code is
  not covered (Q5).
- **FR-8 Not stable.** The same page lists what may change in any release, with the reason: everything under
  `nnh/detail/`; the platform contract (`nnh/platform/*`) and platform libraries; converted formats (`.nnhmap`,
  `.nnhmesh`, `.nnhsprite`, NNHD, font and string outputs: regenerated by every build); the game↔tool wire protocol
  (a game and the `nnh` of the same release always match; spec 033 FR-22 explains a mismatch); environment variables
  used for testing and screenshots; log and message text, except lines a page documents as stable; exact self-check
  goldens, pixels and timings across versions; sizes of engine types; binary (ABI) compatibility: games are always
  recompiled.
- **FR-9 Experimental in 1.x.** Parts no real game has used before 1.0 are marked **experimental in 1.x** (Q2; the
  study proposes 3D — `<nnh/mesh.hpp>`, `<nnh/gfx3d.hpp>` — and spec 031's hot-reload queries). An experimental part:
  - says so in its header comment, at the top of its API page and in the stability page;
  - may change in a minor release, and every such change is listed in the changelog's notes for that release;
  - becomes stable in a minor release by a changelog entry and a stability page edit, never silently.
- **FR-10 The stability page covers every header.** Every public header in the engine (game-facing and 3DS extension
  headers) is classified on the stability page as stable, experimental or not stable; a docs check fails when a public
  header is missing from the page or the page names one that doesn't exist.
- **FR-11 `NNH_DEPRECATED`.** A public macro `NNH_DEPRECATED("message")` marks a function, type, enumerator or variable
  as deprecated through the standard attribute, so every supported compiler (host GCC/clang/MSVC, devkitARM) warns at
  each use with the message, which names the replacement and the version that removes it. Rules:
  - a stable item is removed only in a major release, and only after at least one minor release in which it was
    deprecated (Q6);
  - deprecated `nnh_add_game()` keywords and engine options print a CMake deprecation warning and still work;
    deprecated `nnh` options and `.nnhs` commands print a one-line warning and still work;
  - every deprecation appears under "Deprecated" in the changelog.
- **FR-12 Breaking changes are caught.** A compile-only compatibility check, frozen at `v1.0.0`, uses the main entry
  points of every stable header and the `nnh_add_game()` keywords; every later 1.x must build it unchanged without
  errors (deprecation warnings allowed), on PC and in a 3DS preset. Changing the frozen check needs a major version
  (Q18).

### Changelog

- **FR-13 `CHANGELOG.md`.** A hand-written changelog at the repository root in the
  [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/) shape: an "Unreleased" section, then one section per
  release (version, date) with Added / Changed / Deprecated / Removed / Fixed / Security and an "Experimental" note for
  FR-9 changes; each entry is a sentence a game developer understands, linking to the spec or API page.
- **FR-14 Changelog draft.** The docs tool gets a subcommand that prints a draft of changelog entries from the specs
  index (implemented specs, their titles and links) and the ROADMAP ticks, limited to specs after a given one (or a
  given release); output is deterministic and it never writes `CHANGELOG.md` itself: the maintainer edits the draft
  into the real entry.

### Release artifacts

- **FR-15 The asset set.** A release `X.Y.Z` (label included) has exactly these assets, named so:
  - `nnhge-X.Y.Z-src.tar.gz`: the source archive (FR-16);
  - `nnh-X.Y.Z-windows-x64.zip` and `nnh-X.Y.Z-linux-x64.tar.gz`: prebuilt `nnh` (FR-17);
  - `nnhge-X.Y.Z-3ds.zip`: the examples and the NNH Link app for the 3DS (FR-19);
  - `nnhge-X.Y.Z-docs.zip`: a documentation snapshot (FR-20);
  - `SHA256SUMS`: the checksums of all the above (FR-21).
- **FR-16 Source archive.** The tracked files of the tagged commit under one top folder `nnhge-X.Y.Z/`, with no `.git`
  and no build output, including `LICENSE` and the third-party licences; its content set is open question Q8. It is
  **reproducible**: built twice from the same commit with the same tools, it has the same SHA-256 (fixed file order,
  times and owners). It is the archive spec 032 pins; projects must pin this uploaded asset, never GitHub's automatic
  "Source code" archives, whose bytes GitHub does not promise to keep.
- **FR-17 Prebuilt `nnh`.** Each package holds the `nnh` program, `LICENSE`, the third-party notices it needs and a short
  README (where the docs are, how to check `SHA256SUMS`). The Windows `nnh.exe` runs on a clean Windows 10/11 x64 with no
  compiler or runtime installed (only system DLLs). The Linux `nnh` runs on Ubuntu 22.04 and 24.04 x64 with no C++
  runtime installed (no dependency on `libstdc++`/`libgcc_s`; glibc symbols no newer than Ubuntu 22.04's). `nnh
  --version` in both says the release version and that it is a release build. macOS has no prebuilt `nnh` (built from
  the source archive, best effort).
- **FR-18 The pin inside release `nnh`.** A release `nnh` is built **after** the source archive, with the archive's
  download URL (the uploaded release asset, FR-16) and SHA-256 given to its build, so `nnh new` (spec 033 FR-9) writes a
  project that pins exactly that archive. The archive does not contain this pin, which avoids the circularity of an
  archive containing its own hash; an `nnh` built from the archive or a checkout has no pin (spec 033's checkout case).
  The pin baked in each release `nnh` equals the archive's line in `SHA256SUMS`.
- **FR-19 3DS package.** The Release `.3dsx` of every example and of the NNH Link app, the `.cia` of the examples that
  declare one (today `audio`) and of NNH Link if it declares one (Q10), a README (how to copy them to the SD card, how to
  install a `.cia` with FBI), and the notices the shipped binaries need (engine, vendored code, the 3DS toolchain's
  runtime libraries; spec 033 Q8). Every asset inside those binaries is engine-made or carries its licence (FR-24's
  audit).
- **FR-20 Docs snapshot.** The documentation as of the tag (format: Q9), for offline reading; the generated project's
  README (spec 033 FR-8) and the release notes link to the docs **at the tag** on GitHub, not to `main` or the wiki,
  which follow the latest.
- **FR-21 `SHA256SUMS`.** One line per other asset in the format `sha256sum -c` reads (hash, two spaces, file name),
  sorted by name. The README and the release notes show how to verify it on Linux (`sha256sum -c`) and Windows
  (PowerShell `Get-FileHash`). Assets are not signed (non-goal); GitHub's release attestations are open question Q13.

### Release tooling

- **FR-22 Release scripts.** Each artifact of FR-15 is made by a release script that runs the same way on a GitHub
  runner, on the maintainer's Windows machine (3DS steps through devkitPro's MSYS2) and on the build VM. Scripts take a
  commit or tag and an output folder, write only into that folder, and never tag, push, upload or publish. In release
  mode they refuse a dirty working tree or a commit that is not the tag's; in snapshot mode they label artifacts with
  the commit instead of a version. They also write the tool versions used (devkitARM, libctru, citro2d, citro3d, SDL3,
  CMake, compilers) for the release notes.
- **FR-23 Release workflow.** A GitHub Actions workflow started **only by hand** (`workflow_dispatch`; no tag, push or
  schedule trigger), with inputs: the tag and a dry-run switch. It:
  - checks the tag exists, is annotated and matches the engine version (FR-6), and stops otherwise;
  - builds every artifact with FR-22's scripts on GitHub-hosted runners: Linux `nnh` on Ubuntu 22.04, Windows `nnh` on
    Windows with static MinGW, 3DS in devkitPro's container **pinned by digest**, and a macOS job that only builds `nnh`
    and runs its unit tests (nothing shipped, Q16);
  - runs the PC unit tests on Linux and `nnh`'s tests on each OS before anything is uploaded;
  - in dry-run mode keeps everything as workflow artifacts and creates no release; otherwise creates a **draft** release
    for the tag with the assets and notes made from the changelog entry and the tool versions. It never publishes;
  - reads the repository only, except the one job that creates the draft (`contents: write`); uses only `GITHUB_TOKEN`
    (no other secret); pins every action by commit SHA (constitution X).
- **FR-24 Release checklist.** A process page `docs/process/releasing.md` holds the release checklist (yes/no lines),
  starting from the study's list: specs and docs ready, version bumped, changelog written; Windows PC and 3DS builds,
  tests, release-strings and no-3D-symbols checks, build budgets measured; build VM runs (GCC 13 and 14, clang
  sanitizers, the 32-bit SSE build, every fuzz target for N minutes (Q11), the 3DS build); the spec 032 consumer project
  and the spec 033 `nnh new` smoke built **from the source archive**, not the repository, on Windows and Linux; the asset
  and licence audit; on the Old 3DS XL (Development builds from the release source, the maintainer presses Y) every
  example's check script, the platformer's level/gate/doors scripts, the stereo script and the generated project's smoke
  script, a `.cia` installed through FBI and started from the HOME Menu, NNH Link pairing and one build, one release
  `.3dsx` copied to the SD card and opened from the Homebrew Launcher; then the draft's assets, hashes, prebuilt `nnh`
  (`--version`, `doctor`, `new`) and notes. The ticked copy for each release is kept where Q17 decides and linked from
  the release notes.
- **FR-25 Nothing public without the maintainer.** Pushing a release tag and publishing a release are done by the
  maintainer by hand, or by an agent only after the maintainer's explicit OK given for that tag in that session; the
  checklist marks both lines as such. Before publishing, a wrong draft is fixed by rerunning the workflow and a wrong
  unpublished tag may be deleted and recreated. After publishing, an asset is never replaced or deleted and the tag never
  moved: a broken release is fixed by the next patch (or `-rc.N+1`), and its notes say which release it replaces.
- **FR-26 CI pin.** The devkitPro container used by the manual CI workflow is pinned by the same digest as the release
  workflow (Q12); changing it is a deliberate change that records the new tool versions.

### The 1.0 release

- **FR-27 Release candidate first.** NNHGE 1.0 is released as `v1.0.0-rc.1` (a GitHub pre-release) after the full
  checklist; fixes found later go into `-rc.2`, … with the checklist re-run for what they touch; `v1.0.0` is tagged on a
  commit whose changes since the last candidate are only the version, the changelog and fixes already checked against a
  candidate, after the checklist's final subset (study step 8) passes (Q3).
- **FR-28 Newcomer test and first game.** Against the published candidate, on a clean Windows (Windows Sandbox) and a
  clean Linux machine, someone follows only the README and the guides: installs the tools, runs `nnh doctor`, `nnh new`,
  runs the project in the simulator and on the Old 3DS, and runs its smoke script. Then a small complete game is built
  in its own repository with the prebuilt `nnh` and the pinned archive (proposal: a single-screen 2D arcade game with a
  UI title menu, scene transitions, sprites, sounds and music, a saved high score, two languages, particles). A friction
  log with times is kept (targets: tools installed → simulator window < 30 min; `nnh new` → window < 5 min); every item
  is fixed, documented or listed as a known issue before `v1.0.0`.
- **FR-29 Docs and status.** After `v1.0.0`: the README's status says 1.0 and its quick start downloads the release
  (with the `SHA256SUMS` check and a note that Windows may warn about an unsigned `nnh.exe`); getting-started points to
  the release; the ROADMAP's "After 1.0" first item reflects that the first game came before the final tag; a decision
  row D57 records the version source, the stability classes, the deprecation rule, the release asset set, the pin
  injection and the "manual, draft, maintainer publishes" rule.

## Acceptance criteria

- **AC-1** Unit tests (PC) and a 3DS build: `nnh::version()`, `version_string()` and the `NNH_VERSION*` macros equal the
  CMake version passed to the test by the build; changing one side only makes the build or the test fail; `#if
  NNH_VERSION >= NNH_VERSION_OF(...)` compiles in a game. The header compiles alone with no other `<nnh/...>` include. A
  Development `hello` on the Old 3DS logs the version line, and the Release `hello` passes
  `scripts/check_release_strings.sh`. (FR-1 to FR-4)
- **AC-2** `NNH_DEPRECATED` compile checks with the host compiler (GCC, clang on the build VM) and devkitARM: a use of a
  deprecated function warns with the message (and fails under `-Werror=deprecated-declarations`), no use gives no
  warning; a deprecated `nnh_add_game()` keyword in a fixture prints a CMake deprecation warning and the game still
  builds. (FR-11)
- **AC-3** The docs check fails on a fixture stability page with an unlisted public header and with a header that
  doesn't exist, and passes on the real page; every experimental header's comment and API page carry the marker.
  (FR-7 to FR-10)
- **AC-4** The compatibility check of FR-12 builds in `pc-debug` and `3ds-release`; removing or renaming a public
  function it uses makes it fail (shown once by a mutation). (FR-12)
- **AC-5** The changelog draft subcommand on a fixture specs index prints the expected lines exactly, twice in a row the
  same; it leaves `CHANGELOG.md` untouched. `CHANGELOG.md` exists with an "Unreleased" section and, at each release, the
  release's section. (FR-13, FR-14)
- **AC-6** Release scripts on the maintainer's Windows machine and on the build VM: the source archive built twice from
  the same commit has the same SHA-256; its file list equals the tagged commit's tracked files under `nnhge-X.Y.Z/` (Q8);
  `sha256sum -c SHA256SUMS` passes in the output folder; release mode refuses a dirty tree and a non-tag commit; no
  file is written outside the output folder. (FR-15, FR-16, FR-21, FR-22)
- **AC-7** A dry run of the release workflow on a test tag produces every asset of FR-15 as workflow artifacts with
  correct names, creates no release and no tag (`gh release list` unchanged); the workflow file has only
  `workflow_dispatch`, `contents: write` on one job, actions pinned by SHA and the devkitPro image by digest; the macOS
  job builds `nnh` and passes its tests. (FR-23, FR-26)
- **AC-8** The Linux `nnh` from the dry run prints its version and runs `nnh doctor` on Ubuntu 22.04 and 24.04; its
  dynamic dependencies list no C++ runtime and its newest glibc symbol is no newer than Ubuntu 22.04's. The Windows
  `nnh.exe` runs `--version` and `doctor` in Windows Sandbox with nothing installed, and depends only on system DLLs.
  (FR-17)
- **AC-9** The URL and SHA-256 baked into each dry-run `nnh` equal the archive's asset URL and its `SHA256SUMS` line;
  `nnh new demo` with that `nnh` writes them into `demo/CMakeLists.txt`; with the local override pointing at the
  unpacked archive, `nnh run demo --pc --script tests/smoke.nnhs` exits 0. After `v1.0.0-rc.1` is published, the same
  without the override (the real download) exits 0 on a clean Windows and a clean Linux machine. (FR-18)
- **AC-10** From the rc's `nnhge-X.Y.Z-3ds.zip`: one example `.3dsx` and NNH Link copied to the SD card start from the
  Homebrew Launcher on the Old 3DS XL; the `audio` `.cia` installs through FBI and starts from the HOME Menu; the notices
  file is present. (FR-19)
- **AC-11** `docs/process/releasing.md` exists; for `v1.0.0-rc.1` and `v1.0.0` every checklist line is ticked in the
  record of Q17, linked from the release notes, including the console runs (verdicts and logs from `build/logs/`). The
  two public steps of each release were done after the maintainer's recorded OK. (FR-24, FR-25)
- **AC-12** GitHub shows `v1.0.0-rc.1` as a pre-release and `v1.0.0` as the latest release, each with the FR-15 assets
  and notes from its changelog section; both tags are annotated; the first game's repository builds against the rc,
  runs on PC and from its `.3dsx` and `.cia` on the Old 3DS, and its own `tests/*.nnhs` pass on both; the friction log
  exists and each item is resolved or listed as a known issue. (FR-27, FR-28)
- **AC-13** Docs: the stability page and `releasing.md` new; `CHANGELOG.md`; `docs/tech/cmake.md` (version, release
  scripts and workflow, CI pin); `docs/api/` page for the version header; README, getting-started and ROADMAP updated;
  decision row D57; `python -I scripts/docs_tool.py check` reports 0 broken links. (FR-20, FR-29)

## Non-goals

- Code signing of `nnh`, signing `SHA256SUMS`, notarisation; package managers (winget, Scoop, Homebrew, AUR).
- An installed CMake package or prebuilt engine libraries (`find_package(nnhge)`): games build the engine from source
  (spec 032).
- ABI stability: games are always recompiled against the engine they ship with.
- Prebuilt `nnh` for macOS or Windows on ARM; macOS and MSVC stay best effort.
- Long-term support branches and backports: only the latest minor gets patch releases (Q15).
- Tag-triggered or scheduled releases, automatic publishing, a release bot.
- Shipping, repackaging or mirroring devkitPro or SDL3 (devkitPro is installed separately; its trademark policy forbids
  calling a repackaged toolchain devkitPro).
- A generated HTML API reference (Doxygen site); the docs snapshot is the repository's docs.
- New 3DS checks (the maintainer has only an Old 3DS), Switch and later systems.
- Upgrading game projects between engine versions automatically (spec 033 non-goal).

## Constraints

- **I Same game code everywhere.** The version header and `NNH_DEPRECATED` are the same on PC and 3DS; no platform
  switch (AC-1, AC-2).
- **III Build speed.** The version header is tiny and has no commit id or date, so a commit does not rebuild the engine
  and a version bump happens only at release commits; release scripts and the workflow add nothing to the day-to-day
  build. Budgets are re-measured for the release checklist, not changed.
- **V No hidden costs.** The version API is `constexpr`, allocates nothing and adds no Release log text.
- **VI Spec first, test first.** Version agreement, the deprecation macro, the docs checks and the changelog draft are
  tested first (AC-1 to AC-5); the release scripts by reproducibility and checksum tests (AC-6); the artifacts on the
  console (AC-10).
- **VIII Documented API.** The stability promise is part of the API docs: every public header is classified (FR-10),
  experimental parts are marked where developers read (FR-9), deprecations name their replacement (FR-11). The docs
  snapshot and the generated project link to the docs at the tag.
- **IX Clean and legal.** Every shipped file is engine-made or carries its licence; the 3DS binaries carry the notices
  their linked code asks for; no Nintendo material; devkitPro is referenced, never shipped (FR-19, FR-24).
- **X Safe by default.** Dependencies and actions pinned (actions by SHA, devkitPro by digest, the engine archive by
  SHA-256); least-privilege workflow with no secret beyond `GITHUB_TOKEN`; published assets never change (FR-25);
  `SHA256SUMS` with verification instructions.
- **CI is manual** (`docs/tech/cmake.md`, maintainer's decision 2026-11-17): the release workflow is manual too; the
  repository is public, so its standard GitHub-hosted runs cost no Actions minutes.
- One implementation at a time, after spec 033 is merged; ≥ 10 GB free on the maintainer's `E:` before the release
  builds; the build VM's disk checked before Linux runs.

## Prior art

- [SemVer 2.0.0](https://semver.org/) (version and pre-release rules) and [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/).
- Abseil's [compatibility guidelines](https://abseil.io/about/compatibility) spell out which uses of a library's API a
  promise does not cover (forward declarations, taking addresses of functions, relying on `sizeof`); FR-7's "what game
  code is not covered" follows that idea.
- GitHub's automatic "Source code" archives are not promised byte-stable
  ([GitHub blog, 2023](https://github.blog/open-source/git/update-on-the-future-stability-of-source-code-archives-and-hashes/)),
  so the pin points at an uploaded asset (FR-16).
- GitHub [immutable releases](https://docs.github.com/en/code-security/concepts/supply-chain-security/immutable-releases)
  ([generally available since 2025-10-28](https://github.blog/changelog/2025-10-28-immutable-releases-are-now-generally-available/)):
  once published, assets can't be added, changed or deleted and the tag is locked; the recommended flow is draft →
  upload → publish, and `gh release verify-asset` checks a signed attestation (Q13).
- devkitPro trademark policy: <https://devkitpro.org/wiki/Trademarks> (quoted by the release study; it answers 403 from
  the maintainer's network and was not re-read for this spec).

## Open questions

Q1–Q3 are the [release study](../../docs/plans/2026-11-26-release-1.0.md)'s decisions 7, 7b and 9 (pending the
maintainer's review); the rest came up while writing this spec.

- [ ] **Q1 SemVer and the stable/not-stable lists** (study decision 7; FR-5, FR-7, FR-8). — *Recommendation (study):
  yes, as FR-7 and FR-8 list them; `nnh` commands, options and exit codes stable, their output text not (except lines a
  page documents).*
- [ ] **Q2 What is experimental in 1.x** (study decision 7b; FR-9). The study: `<nnh/mesh.hpp>` and `<nnh/gfx3d.hpp>`
  (3D) and spec 031's hot-reload queries, the modules no real game has used. Also to decide: `<nnh/math3d.hpp>` (pure,
  golden-tested math), `<nnh/live.hpp>` (spec 030, Development-only), and the 3DS extension headers. —
  *Recommendation: as the study, plus `<nnh/live.hpp>` (Development tooling, young, and nothing in a shipped game
  depends on it); `math3d` stable (plain value types with the same bits everywhere, little to get wrong); `<nnh/3ds/*>`
  stable. UI, fonts/localization and particles stay stable because the first game (FR-28) exercises them before
  `v1.0.0`.*
- [ ] **Q3 Release candidate before 1.0.0** (study decision 9; FR-27). — *Recommendation (study): yes, `v1.0.0-rc.1`
  → newcomer test and first game → fixes → `v1.0.0`. This changes the ROADMAP's "After 1.0" item 1 (the first game
  moves before the final tag).*
- [ ] **Q4 Pre-release label and the version on `main`.** CMake's `project(VERSION)` takes numbers only. Options: a)
  numbers in `project()` plus a separate label (`rc.1`, `dev` or empty) used for the strings; b) only numbers, no
  labels anywhere but the tag. — *Recommendation: a. `NNHGE_VERSION` stays numeric (so `VERSION_LESS` works), a
  `NNHGE_VERSION_LABEL` and a full-string variable carry the label; after each release `main` moves to the next minor
  with the label `dev` (`1.1.0-dev`), so a checkout `nnh` never claims to be a release and `nnh doctor` can tell them
  apart.*
- [ ] **Q5 What a minor may still do to stable parts.** — *Recommendation: allowed: new headers, functions, overloads
  that don't make existing calls ambiguous, new members at the end of `*Desc`/options structs with default values, new
  enumerators (games should not `switch` exhaustively without a `default`), new events, new keywords and options, new
  `.nnhs` commands, new optional `nnh.toml` keys. Not covered: code in `nnh::detail`, forward declarations of engine
  types, taking the address of engine functions, `sizeof`/layout of engine types, specialising engine templates,
  relying on undocumented behaviour, log text, goldens and exact pixels.*
- [ ] **Q6 Deprecation window** (FR-11). — *Recommendation (study): `[[deprecated]]` through `NNH_DEPRECATED` for at
  least one minor release before removal, and removal only in a major. No engine switch to silence the warnings: the
  compiler's own flag does it.*
- [ ] **Q7 Save files across major versions** (FR-7.4). — *Recommendation: the save promise outlives 1.x: every later
  engine version, 2.x included, reads every save format an earlier release wrote (spec 027 already migrates the game's
  own fields; this covers the engine's container and encoding). Players' progress is the one thing a game update must
  never lose.*
- [ ] **Q8 Source archive content** (FR-16; spec 032 Q6). Options: full (every tracked file) or slim (engine, cmake,
  third_party, tools, link, apps; no examples, tests, specs, docs). — *Recommendation: one full archive for 1.0: it is a
  few MB, it is what the checklist tests, and examples are learning material; FetchContent fetching it once per preset
  is acceptable (spec 032 Q8). Reconsider a slim archive if the newcomer test shows configure or download time hurts.*
- [ ] **Q9 Docs snapshot format** (FR-20). — *Recommendation: a zip of the repository's Markdown (`docs/`, `README.md`,
  `CHANGELOG.md`, `CONTRIBUTING.md`, `LICENSE`) as of the tag, no HTML build; links in projects and notes go to
  `https://github.com/<repo>/tree/vX.Y.Z/docs`. Alternative: drop the zip, since the source archive already has the
  docs.*
- [ ] **Q10 Which `.cia` files ship** (FR-19). Today only `audio` declares `CIA`. — *Recommendation: `audio` and the NNH
  Link app (a `.cia` is how a console user keeps NNH Link on the HOME Menu); NNH Link gets a `CIA` declaration in this
  spec if it lacks one. Other examples stay `.3dsx` only.*
- [ ] **Q11 Fuzz time in the checklist** (FR-24). — *Recommendation: every libFuzzer target 5 minutes on the build VM,
  30 minutes for targets whose parser changed since the last release; any crash blocks the release.*
- [ ] **Q12 Pin the CI container too** (FR-26). — *Recommendation: yes, one digest shared by both workflows, updated
  only by a deliberate change that records the new devkitARM/libctru/citro versions.*
- [ ] **Q13 GitHub immutable releases and attestations.** — *Recommendation: turn immutable releases on in the
  repository settings before `v1.0.0-rc.1` (maintainer's action): it enforces FR-25 (assets and tag locked once
  published) and gives free signed attestations checkable with `gh release verify-asset`, documented next to
  `SHA256SUMS`. Consequence to accept: a published release's tag name can never be reused, even after deleting it.*
- [ ] **Q14 A constitution article for the promise?** The stability page is a rule every plan must respect. —
  *Recommendation: add a short article XII "Keep the promise" (no breaking change to the stable surface within a
  major; deprecate first; experimental parts marked) pointing to the stability page, so the Constitution Check of every
  plan asks it. Maintainer's call, since the constitution changes only with approval.*
- [ ] **Q15 Patch releases and support.** — *Recommendation: only the latest minor receives patch releases; no LTS
  branches; security fixes the same way.*
- [ ] **Q16 macOS job in the release workflow** (FR-23). — *Recommendation (study): yes, build `nnh` and run its tests
  only (free on a public repository), nothing shipped; failures are reported in the notes, not blocking.*
- [ ] **Q17 Where the ticked checklist lives** (FR-24). Options: a GitHub issue per release from an issue template; a
  file committed after the release; the release notes. — *Recommendation: one issue per release (created from a
  "Release vX.Y.Z" template that copies the checklist), linked from the notes; it can be ticked after the tag without a
  commit.*
- [ ] **Q18 Scope of the compatibility check** (FR-12). — *Recommendation: one compile-only game source per stable
  header group, written against `v1.0.0`'s API pages, plus a CMake fixture with every `nnh_add_game()` keyword (spec
  032's consumer project can serve); built in `ctest` on PC and in the 3DS checklist build. It catches removals and
  signature breaks, not behaviour changes (those stay with each module's tests).*
- [ ] **Q19 Version log line** (FR-4). — *Recommendation: yes, Debug/Development only, one line at start (`NNHGE
  1.0.0 (Development)`), so console logs and bug reports carry it; Release unchanged (no text added).*
- [ ] **Q20 Decision row.** — *Recommendation: D57 as FR-29 lists.*
