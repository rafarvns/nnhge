# Tasks 034: Versions, API stability and releases

Status: approved 2026-11-27 (with plan.md; part A only)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops (test first, red seen, then green). `[P]` = can be done in parallel with the previous
task. While developing run only the touched filters (`build/pc-debug/tests/nnh_tests version`,
`build/pc-debug/tools/nnh/nnh_tool_tests <filter>`, `ctest -R <name>`); the default `ctest -LE consumer-full` once per
stage end. One implementation at a time, in the worktree `spec-034`.

**Public steps.** Pushing any branch to GitHub, creating the release issue, starting a workflow run, pushing a tag and
publishing a release each need the maintainer's explicit OK, given for that step (for a tag: for that tag, in that
session). No task does them on its own; the tasks marked **(OK)** say who does what. GitHub CI is manual: no task waits
for a run the maintainer hasn't started.

# Part A: the tooling

## Stage 0: start

- [ ] **T0.1** Disk and worktree: `df -h /e` ≥ 10 GB (plan question 12: remove the merged `spec-033` worktree's build
  folders first, with the maintainer's OK); worktree `spec-034` from `main`; fresh `pc-debug`, `3ds-release` builds.
  Baselines in `build/ref-034/` (not committed): `hello.3dsx` (3ds-release) SHA-256, `nnh_tests`/`nnh_tool_tests`
  counts, clean `pc-debug` all-targets build time, `hello` one-file edit (3 runs, simulator and `.3dsx`).

## Stage 1: the version (FR-1 to FR-4)

- [ ] **T1.1** `nnh_version_read(<header> <prefix>)`: five lines, numbers, labels `""`/`dev`/`rc.N`, string = parts,
  minor/patch < 1000; failures name the header and line — files: `cmake/nnh_version_read.cmake`,
  `tests/version/read_check.cmake` + case headers, `tests/version.cmake` — test: ctest `version_read` — covers FR-1, FR-5
- [ ] **T1.2** `<nnh/version.hpp>` (still `0.1.0`, label `""`, so nothing else changes yet): macros, `NNH_VERSION_OF`,
  `NNH_DEPRECATED`, `Version`, `version()`, `version_string()`, `version_number()`, `version_at_least()`; the umbrella
  includes it — files: `engine/core/include/nnh/version.hpp`, `nnh.hpp` — test: `tests/core/test_version.cpp` (values
  against `NNH_TEST_CMAKE_VERSION_*` given by the tests' CMake), ctests `version_header_alone` (`alone.cpp`, an include
  folder holding only the header, `-Werror`) and, on Linux, `version_glibc_macros` — covers FR-2
- [ ] **T1.3** CMake reads the header before `project()`; `CMAKE_CONFIGURE_DEPENDS`; `NNHGE_VERSION_LABEL`,
  `NNHGE_VERSION_STRING`; status line — files: `CMakeLists.txt`, `cmake/nnh_version.cmake` — test: `version_read` cases
  through the top-level path (a scratch configure in `read_check.cmake`), the consumer test's `NNHGE_VERSION` check still
  green — covers FR-1, FR-3
- [ ] **T1.4** `version_check.cpp` with the CMake definitions on that file only — files:
  `engine/core/src/core/version_check.cpp`, `engine/core/CMakeLists.txt` — test: the build itself; the two mutations of
  plan "FR-3" (string only, major only) recorded in plan.md "Stage 1 results" — covers FR-3
- [ ] **T1.5** [P] The log line `NNHGE <string> (<flavour>)` before `started "..."`; `check_release_strings.sh` gains its
  format — files: `engine/core/src/app/app.cpp`, `scripts/check_release_strings.sh` — test: a unit test reading the fake
  platform's log (`tests/app/`), `check_release_strings.sh` on Release `hello` (PC and 3DS) — covers FR-4
- [ ] **T1.6** Stage end: full `nnh_tests` (risk 5: log-line counts), `hello.3dsx` still byte-identical to T0.1's,
  `3ds-release` and `3ds-dev` 0 warnings; results in plan.md.

## Stage 2: deprecation (FR-11)

- [ ] **T2.1** `NNH_DEPRECATED` cases and their driver: `uses.cpp` fails under `-Werror=deprecated-declarations` with
  each message, `unused.cpp` compiles under `-Werror` — files: `tests/version/check_deprecated.sh`,
  `tests/version/deprecated/{uses,unused}.cpp`, `tests/version.cmake` — test: ctest `version_deprecated` (host compiler);
  by hand with `/c/devkitPro/devkitARM/bin/arm-none-eabi-g++` at the stage end — covers FR-11, AC-2
- [ ] **T2.2** Deprecated `nnh_add_game()` keywords: the table (empty), `_nnh_deprecate_game_keyword()`,
  `_nnh_apply_deprecated_keywords()`, called by `nnh_add_game()` before parsing — files: `cmake/nnh_deprecations.cmake`,
  `cmake/nnh_game.cmake`, `tests/version/deprecated_keyword/check.cmake` — test: ctest `version_deprecated_keyword`
  (`cmake -P`: the message, the replacement, unknown keywords untouched) — covers FR-11
- [ ] **T2.3** [P] The same through a real configure and build: `tests/version/deprecated_keyword/` project — test: ctest
  `version_deprecated_keyword_build` (label `consumer-full`: a `DEPRECATION` warning in the configure log, the game
  builds) — covers FR-11, AC-2

## Stage 3: the promise (FR-7 to FR-10, FR-12; Q14)

- [ ] **T3.1** The stability check: headers on disk vs the page's rows, classes, duplicates, markers in headers and API
  pages; `docs_tool.py` loads `docs_stability.py` by path — files: `scripts/docs_stability.py`, `scripts/docs_tool.py`,
  `scripts/tests/test_docs_tool.py`, `scripts/tests/data/stability/` (`.txt`), `tests/release.cmake` — test: ctest
  `docs_tool_tests` (each failing fixture, then a passing one) — covers FR-10, AC-3
- [ ] **T3.2** `docs/api/stability.md`: the promise (FR-7), the not-stable list (FR-8), experimental (FR-9), minors (Q5),
  not covered, saves beyond 1.x (Q7), the deprecation rule (FR-11, Q6), one row per header; markers in
  `mesh.hpp`, `gfx3d.hpp`, `live.hpp`, the hot-reload queries of `assets.hpp` and their API pages — files: those —
  test: `docs_tool.py check` passes on the real tree (it failed before the page existed) — covers FR-7, FR-8, FR-9
- [ ] **T3.3** [P] Constitution article XII, its row in the plan template, the review skill and agent — files:
  `docs/process/constitution.md`, `specs/_templates/plan.md`, `.claude/skills/nnh-review/SKILL.md`,
  `.claude/agents/nnh-reviewer.md` — test: `docs_tool.py check` — covers spec Q14
- [ ] **T3.4** The frozen compatibility check `tests/compat/v1/` (README: frozen at `v1.0.0`; one source per stable group;
  `v1_3ds.cpp` in 3DS presets), added with the examples — files: `tests/compat/v1/*`, `CMakeLists.txt` — test: builds in
  `pc-debug` and `3ds-release`; mutation (rename one public function it uses → build fails) recorded; clean build time vs
  T0.1 (plan target +≤ 3 %) — covers FR-12, AC-4
- [ ] **T3.5** [P] The consumer project's call as the frozen keyword fixture: `STRICT_STRINGS` added, the frozen comment
  — files: `tests/consumer/game/CMakeLists.txt` (strings complete for `STRICT_STRINGS`) — test: ctest `consumer` — covers
  FR-12

## Stage 4: changelog (FR-13, FR-14)

- [ ] **T4.1** `docs_tool.py changelog [--after NNN | --after-release X.Y.Z]` — files: `scripts/docs_changelog.py`,
  `scripts/docs_tool.py`, `scripts/tests/data/changelog/` (`specs_index.txt`, `roadmap.txt`, `changelog.txt`,
  `expected_*.txt`) — test: `docs_tool_tests` (expected lines exactly, twice the same, no file written, an unknown spec
  number refused) — covers FR-14, AC-5
- [ ] **T4.2** [P] `docs_tool.py checklist X.Y.Z` (the checklist section of `releasing.md`, version filled in) — files:
  `scripts/docs_tool.py` — test: `docs_tool_tests` on a fixture page — covers FR-24
- [ ] **T4.3** `CHANGELOG.md` with Unreleased (seeded from the draft for specs 001–034, edited into a game developer's
  words); `CHANGELOG.md` as the wiki page `Changelog`; the rule "each spec's merge adds its Unreleased line" —
  files: `CHANGELOG.md`, `scripts/docs_tool.py` (`SINGLE_PAGES`), `docs/process/documentation.md`,
  `.claude/skills/nnh-docs/SKILL.md` — test: `docs_tool.py check`; wiki preview (`docs_tool.py wiki build/wiki-preview`)
  lists it — covers FR-13

## Stage 5: `nnh` and the version string

- [ ] **T5.1** `NNH_TOOL_VERSION` = `NNHGE_VERSION_STRING` — files: `tools/nnh/CMakeLists.txt`,
  `tools/nnh/tests/test_build_info.cpp` — test: `test_build_info.cpp`, ctest `nnh_new_files` (`--version` line) —
  covers FR-1
- [ ] **T5.2** Engine folder and version from `version.hpp` (C++ reader with the parser's rules; fuzz seeds) — files:
  `tools/nnh/src/new/engine_location.{hpp,cpp}`, `tools/nnh/tests/test_engine_location.cpp`,
  `tools/nnh/tests/test_fuzz_tool.cpp` — test: those (red first: a folder with a header and a `project()` taking a
  variable is an engine) — covers FR-1
- [ ] **T5.3** [P] `doctor` reads `NNHGE_VERSION_STRING` first — files: `tools/nnh/src/doctor/project_versions.{hpp,cpp}`,
  `tools/nnh/tests/test_doctor_project.cpp` — test: those (a label on both sides → equal; numeric-only cache →
  fallback) — covers FR-1
- [ ] **T5.4** [P] `NNH_TOOL_STATIC_RUNTIME` (Linux `-static-libstdc++ -static-libgcc`) — files:
  `tools/nnh/CMakeLists.txt` — test: on the VM, `readelf -d` of an `nnh` built with it lists no `libstdc++`/`libgcc_s`
  (stage 6's `check_nnh_binary.sh`) — covers FR-17
- [ ] **T5.5** Stage end: `nnh_tool_tests`, ctests `nnh_tool`, `nnh_pin_check`, `nnh_new_files`, `nnh_template_smoke`.

## Stage 6: release scripts (FR-15 to FR-22)

- [ ] **T6.1** `common.sh` (arguments, the three modes, clean tree, `<out>/work`, tool versions), `print_version.cmake`,
  `set_version.cmake`, `check_tag.sh` — files: `scripts/release/` — test: `scripts/release/tests/test_scripts.sh` on a
  scratch repository (fixed author and dates; local tags only, no remote): dirty tree refused in release/preview, a
  lightweight tag and a tag on a commit with another version refused, the right annotated tag accepted;
  `set_version.cmake` refuses `1.0` and `1.0.0-beta` and rewrites only the five lines; ctest `release_scripts` in
  `tests/release.cmake` — covers FR-5, FR-6, FR-22
- [ ] **T6.2** `source_archive.sh` — test: `test_scripts.sh` (two runs, one SHA-256; file list = `git ls-tree -r` under
  `nnhge-V/`; no `.git`; a CRLF-committed `-text` file kept byte for byte; nothing outside `<out>`); on the real repository
  `--snapshot` twice on Windows (same hash, recorded) — covers FR-16, AC-6
- [ ] **T6.3** [P] `checksums.sh`, `docs_snapshot.sh` — test: `test_scripts.sh` (sorted lines, `sha256sum -c` passes, a
  changed file fails it; the docs zip's file list) — covers FR-20, FR-21
- [ ] **T6.4** `check_pin.sh`, `check_nnh_binary.sh` (allow-list of Windows system DLLs measured on a `pc-release` `nnh.exe`;
  glibc floor argument) — test: `check_nnh_binary.sh` on today's Windows `nnh.exe` (passes) and on a non-static Linux build
  on the VM (fails naming `libstdc++`), recorded — covers FR-17, FR-18
- [ ] **T6.5** `build_nnh.sh` (hash check, unpack, configure with the pin and the static runtime, build, `nnh_tool_tests`,
  `check_pin.sh`, `check_nnh_binary.sh`, package) — test: `make_all.sh --snapshot --only nnh` on Windows: the package
  holds the four files, the pin is the `file://` archive, the unpacked archive doesn't contain the hash — covers FR-17,
  FR-18
- [ ] **T6.6** GCC 12 for the Linux release `nnh` (plan risk 1, question 7): on the VM, with the GCC 12 spec 033 used for
  its minimums, `build_nnh.sh` from a snapshot archive with `NNH_TOOL_STATIC_RUNTIME=ON`; `check_nnh_binary.sh` reports
  the glibc floor (2.39 there: the 2.35 check itself runs on the runner); record the result in plan.md — covers FR-17
- [ ] **T6.7** NNH Link `CIA UNIQUE_ID 0xE0A10` — files: `apps/nnh-link/CMakeLists.txt` — test: `3ds-release` `dist`
  makes `nnh-link.cia` — covers FR-19
- [ ] **T6.8** `build_3ds.sh` (archive, `3ds-release`, `dist`, release-strings, no-3D-symbols, command length, package) —
  test: `make_all.sh --snapshot --only 3ds` on Windows: every game of `nnh_games.txt` in `3ds/nnhge/`, both `.cia`,
  README, NOTICES; `check_command_length.sh` PASS — covers FR-19, FR-24
- [ ] **T6.9** [P] `tool_versions.sh` (Windows, VM, the container's pacman database) — test: its output on Windows and the
  VM recorded in plan.md — covers FR-22
- [ ] **T6.10** `release_notes.py` (section extraction, pin lines, verification, tools, macOS line, issue link) — files:
  `scripts/release/release_notes.py`, `scripts/release/tests/test_release_notes.py` — test: on fixtures (a missing section
  refused outside snapshot mode, `-rc.N` sections, the pin lines exactly) — covers FR-13, FR-21, FR-23
- [ ] **T6.11** `make_all.sh` (order, `--only`, the cross-check of the pin against `SHA256SUMS`) — test: a full
  `make_all.sh --snapshot` on Windows; `nnh new demo` with the snapshot `nnh` (pinned to the `file://` archive) configures
  by **downloading and hash-checking** the archive, builds, and `nnh run demo --pc --script tests/smoke.nnhs` exits 0 —
  covers FR-18, FR-22, AC-9 (local form)
- [ ] **T6.12** [P] The no-publish guard: no `git tag`, `git push`, `gh ` in `scripts/release/` — test: in
  `test_scripts.sh` — covers FR-22, FR-25

## Stage 7: the workflow (FR-23, FR-26)

- [ ] **T7.1** `check_workflows.sh` (only `workflow_dispatch`, `dry_run` default true, SHA-pinned `uses:`, `@sha256:`
  containers, the same digest in both files, one `contents: write` on `draft`) — files: `scripts/release/check_workflows.sh`
  — test: ctest `release_workflows` on fixture workflow files (each rule broken once) and on the real ones — covers
  FR-23, FR-26, AC-7 (static part)
- [ ] **T7.2** The devkitPro digest: choose the image whose devkitARM/libctru/citro versions match the maintainer's or the
  VM's (read-only: `docker buildx imagetools inspect` or the registry API), record them; `ci.yml` uses it — files:
  `.github/workflows/ci.yml`, `docs/tech/cmake.md` — test: `release_workflows` — covers FR-26
- [ ] **T7.3** `release.yml` (the jobs of plan "Release workflow"; action SHAs looked up read-only with `gh api`) — files:
  `.github/workflows/release.yml` — test: `release_workflows`; `actionlint` if available (read-only) — covers FR-23
- [ ] **T7.4** [P] The release issue template — files: `.github/ISSUE_TEMPLATE/release.md` — test: `docs_tool.py check`
  — covers FR-24

## Stage 8: notices and audit (FR-17, FR-19, FR-24; constitution IX)

- [ ] **T8.1** Licence audit of what each package links: static libstdc++/libgcc, MinGW-w64 CRT and winpthreads, Ryu,
  stb_vorbis, libctru, citro2d, citro3d, newlib, devkitARM's libstdc++; the notices texts — files:
  `scripts/release/data/NOTICES-nnh.txt`, `NOTICES-3ds.txt`, `docs/tech/third-party.md` — test: review; `build_nnh.sh` and
  `build_3ds.sh` refuse to pack without them — covers FR-17, FR-19
- [ ] **T8.2** [P] Asset audit: every file in `examples/*/assets`, `apps/`, the template and `tests/consumer` assets the
  3DS package embeds is engine-made (provenance noted) or carries its licence; the example sounds' provenance recorded —
  files: the assets' `README`/licence files, `docs/tech/third-party.md` — test: review; checklist line — covers FR-19,
  FR-24
- [ ] **T8.3** [P] Package READMEs (`nnh`: docs at the tag, `SHA256SUMS` on Linux and PowerShell, the unsigned
  `nnh.exe` warning; 3DS: SD card layout, FBI, the NNH Link `.cia` note) — files: `scripts/release/data/*.txt.in` —
  test: `make_all.sh --snapshot` output read — covers FR-17, FR-19, FR-21

## Stage 9: docs

- [ ] **T9.1** `docs/process/releasing.md` (roles, order, checklist with every FR-24 line, the fuzz times of Q11, the rc
  rules of FR-27, FR-25's fixes, immutable releases on before rc.1), `docs/process/README.md` row — test:
  `docs_tool.py checklist 1.0.0-rc.1` prints it; `docs_tool.py check` — covers FR-24, FR-25, FR-27
- [ ] **T9.2** [P] `docs/api/version.md` (the header, `#if` use, labels outside the number, `NNH_DEPRECATED` positions)
  and both new pages in the README's API list — covers FR-2, FR-11
- [ ] **T9.3** [P] `docs/tech/cmake.md` (the version source and bump, `NNH_TOOL_STATIC_RUNTIME`, the release scripts and
  modes, the workflow, the digest and its tool versions, the new ctests), `docs/tech/devkitpro-3ds.md` (NNH Link `.cia`),
  `docs/guides/game-projects.md` (upgrading from a release's two notes lines), `docs/guides/nnh-link.md`,
  `docs/architecture/project-structure.md`, `security.md`, `CLAUDE.md` (summary, layout, build lines), `CONTRIBUTING.md`
  (changelog line) — covers FR-20, FR-29 (Part A share)
- [ ] **T9.4** `scripts/vm_check.sh final` gains: the deprecation cases with devkitARM, `release_scripts`,
  `source_archive.sh --snapshot` twice on the VM's tree (a scratch `git init` of the synced tree, since the VM's copy has
  no `.git`), `check_nnh_binary.sh` on the VM's static `nnh` (glibc floor reported, not enforced: the VM is 24.04) —
  files: `scripts/vm_check.sh`, `docs/tech/cmake.md` — covers FR-22, AC-6

## Stage 10: the version for `main`, and a local preview

- [ ] **T10.1** `set_version.cmake 1.0.0-dev` (plan question 3) — files: `version.hpp` — test: full Windows guards:
  `pc-debug`, `pc-release`, `3ds-dev`, `3ds-release` 0 warnings; `ctest -LE consumer-full`; `hello.3dsx` hash vs T1.6
  (recorded; expected unchanged unless a game default reads the engine version)
- [ ] **T10.2** A preview of the whole release locally: on a scratch branch commit with `1.0.0-rc.1` (not pushed, not
  merged), `make_all.sh --preview` on Windows (everything but the Linux `nnh`) and on the VM (the Linux `nnh` and a 3DS
  build with `/opt/devkitpro`); sizes and times recorded in plan.md; `nnh new demo` with the preview `nnh` writes the
  https pin; with `NNHGE_ENGINE_DIR` at the unpacked archive the smoke script exits 0 — covers AC-6, AC-9 (local form)

## Stage 11: review, Linux pass, console runs, merge

- [ ] **T11.1** One independent review (`nnh-reviewer`: the stability page against the headers, the scripts' safety, the
  workflow's privileges) **and** `FUZZ="tests/fuzz_remote" scripts/vm_check.sh final`, together (VM 120 started with
  the usual checks: Bazzite off, `free -m`; the new version reader in `nnh` is covered by the mini-fuzz in
  `nnh_tool_tests`, which `final` runs under ASan)
- [ ] **T11.2** Fixes, each re-checked with `scripts/vm_check.sh fast`
- [ ] **T11.3** Console runs back to back (the maintainer presses Y; scripts and the local preview's 3DS zip): Development
  `hello` logs `NNHGE 1.0.0-dev (Development)` (AC-1); one example `.3dsx` and `nnh-link.3dsx` from the preview zip
  copied to the SD card start from the Homebrew Launcher; `nnh-link.cia` and `audio.cia` installed through FBI start
  from the HOME Menu; NNH Link pairs and builds one game (from its `.3dsx`) — covers FR-4, FR-19, AC-10 (preview form)
- [ ] **T11.4** Merge to `main` locally; `specs/README.md` row → "tooling merged, rc.1 next" (question 14); D57 →
  approved/built as the maintainer decided; ROADMAP line updated. Pushing `main` **(OK)**: the maintainer, or an agent
  after his OK.

# Part B: the 1.0 release

Every stage runs from `releasing.md`; each public step is marked **(OK)**.

## R1: the workflow's dry run (AC-7, AC-8, AC-9)

- [ ] **R1.1** Immutable releases turned on in the repository settings **(OK: the maintainer, told by the checklist)** —
  spec Q13
- [ ] **R1.2** Dry run on `main` in preview mode, `dry_run: true` **(OK: the maintainer starts it, or an agent after his
  OK)**: every FR-15 asset as a workflow artifact with the right names; `gh release list` unchanged; macOS result
  recorded — covers FR-23, AC-7
- [ ] **R1.3** From the dry run's artifacts: Linux `nnh` `--version`/`doctor` (22.04 and 24.04 from the jobs; the VM by
  hand); Windows `nnh.exe` `--version`/`doctor` in Windows Sandbox (maintainer) with only system DLLs; the pin equals the
  archive's `SHA256SUMS` line; `nnh new demo` + `NNHGE_ENGINE_DIR` at the unpacked archive → smoke exits 0 — covers
  FR-17, FR-18, AC-8, AC-9
- [ ] **R1.4** Fixes from R1 (a script bug starts with a failing test in `test_scripts.sh`), re-run R1.2 **(OK)**

## R2: preparing `v1.0.0-rc.1`

- [ ] **R2.1** The release issue `Release v1.0.0-rc.1` from `docs_tool.py checklist 1.0.0-rc.1` **(OK: created by the
  maintainer, or an agent after his OK)** — covers FR-24
- [ ] **R2.2** The candidate commit: `set_version.cmake 1.0.0-rc.1`, `CHANGELOG.md` section `[1.0.0-rc.1]` from the
  draft and Unreleased — covers FR-5, FR-13
- [ ] **R2.3** The full checklist on that commit (Windows, VM `final` and fuzz per Q11, the consumer project and `nnh new`
  from the source archive on Windows and Linux, audits, console runs, command length), ticked in the issue — covers
  FR-24, AC-11

## R3: tag, draft, publish

- [ ] **R3.1** Push `main` with the candidate commit **(OK)**; create and push the annotated tag
  `git tag -a v1.0.0-rc.1 -m "NNHGE 1.0.0-rc.1"` **(OK: the maintainer, or an agent only after his explicit OK for this
  tag in this session)** — covers FR-6, FR-25
- [ ] **R3.2** Workflow on the tag with `dry_run: false` **(OK)** → a draft pre-release with the assets and notes
- [ ] **R3.3** The checklist's draft lines: download every asset, `sha256sum -c SHA256SUMS`, prebuilt `nnh`
  `--version`/`doctor`/`new` on Windows and Linux, the notes read; the 3DS zip on the Old 3DS (one `.3dsx`, NNH Link, the
  `audio` `.cia` from the HOME Menu, the notices file) — covers FR-19, FR-24, AC-10
- [ ] **R3.4** Publish as a pre-release **(OK: the maintainer, or an agent only after his explicit OK for this tag)** —
  covers FR-25, FR-27
- [ ] **R3.5** `main` back to `1.0.0-dev` (push **(OK)**)

## R4: the real download (AC-9)

- [ ] **R4.1** On a clean Windows (Windows Sandbox) and a clean Linux machine (plan question 11), with the published
  `nnh` and no override: `nnh new demo` → `nnh run demo --pc --script tests/smoke.nnhs` exits 0 — covers FR-18, AC-9

## R5: the newcomer test (FR-28)

- [ ] **R5.1** On the same clean machines, following only the README and the guides: tools installed, `nnh doctor`,
  `nnh new`, simulator, Old 3DS, smoke script; a friction log with times (targets: < 30 min to a simulator window,
  < 5 min from `nnh new`) — covers FR-28

## R6: the first game (FR-28, AC-12)

- [ ] **R6.1** The game's repository **(OK: created by the maintainer, plan question 10)**; a short design note there
- [ ] **R6.2** The game built with the prebuilt `nnh` and the pinned rc archive (single-screen 2D arcade: UI title menu,
  transitions, sprites, sounds and music, a saved high score, two languages, particles), its own `tests/*.nnhs` passing on
  PC and the Old 3DS from its `.3dsx` and `.cia`; friction log continued
- [ ] **R6.3** Every friction item fixed (bug fixes: failing test first), documented, or listed as a known issue; if
  engine code changed, `-rc.2` repeats R2–R3 for what the fixes touch (FR-27)

## R7: `v1.0.0`

- [ ] **R7.1** The release commit: `set_version.cmake 1.0.0`, `CHANGELOG.md` `[1.0.0]`; only the version, the changelog
  and fixes already checked against a candidate since the last one (FR-27); the checklist's final subset; the issue
  `Release v1.0.0` **(OK)**
- [ ] **R7.2** Push, tag `v1.0.0` **(OK, for this tag)**, workflow `dry_run: false` **(OK)**, draft checked, publish as the
  latest release **(OK, for this tag)**; the freeze of `tests/compat/v1/` starts here — covers FR-6, FR-12, FR-25, AC-12
- [ ] **R7.3** `main` → `1.1.0-dev`; README status 1.0 and the quick start through the release download (with
  `SHA256SUMS` and the unsigned `nnh.exe` note); getting-started; ROADMAP "After 1.0" item 1 (the first game came before
  the final tag), the Release 1.0 line ticked; D57 → implemented; spec, plan and tasks status → implemented;
  `specs/README.md`; `docs_tool.py check` (push **(OK)**) — covers FR-29, AC-13
