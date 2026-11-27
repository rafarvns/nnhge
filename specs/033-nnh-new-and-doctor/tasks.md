# Tasks 033: `nnh new`, `nnh doctor`, `nnh --version` and the newcomer docs

Status: approved 2026-11-27 (with plan.md)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops (test first, red seen, then green). `[P]` = can be done in parallel with the previous
task. While developing run only the touched filters (`build/pc-debug/tools/nnh/nnh_tool_tests <filter>`); the default
`ctest -LE consumer-full` once per stage end. One implementation at a time, in the worktree `spec-033`.

## Stage 0: baseline and design measurements

- [x] **T0.1** Start: `main` with spec 032 merged; worktree `spec-033`; `df -h /e` ≥ 10 GB; fresh `pc-debug` build;
  `nnh_tool_tests` count and time, `nnh` binary size, `hello` one-file edit times (3 runs, simulator and `.3dsx`) kept in
  `build/ref-033/` (not committed).
- [x] **T0.2** [P] Measurements in a temporary folder outside the repository (deleted afterwards), recorded in plan.md
  "Stage 0 results": the no-pin `CMakeLists.txt` shape (empty `URL`/`URL_HASH` with the override; the guard without it)
  on VM 120's CMake (3.28) and, if obtainable without a system change, CMake 3.25; `AUTHOR ""` and CMP0174 on 3.25/3.28;
  `file(DOWNLOAD <url> RANGE_START 0 RANGE_END 0)` with no output file in native CMake 4.4.2, devkitPro's 4.0.2 and the
  VM's (GitHub release URL, redirect, one byte); exclusive create on MinGW (`_wopen` `_O_EXCL`) and Linux; one
  `bash -lc "command -v cmake ninja; cmake --version; ninja --version"` in devkitPro's MSYS2 (time); pacman database
  folders on Windows and on VM 120 (`/opt/devkitpro/...`); `cl` banner (if an MSVC is reachable, else a recorded sample).
- [x] **T0.3** Compiler minimums (question 4; only with the maintainer's OK for `apt`): on VM 120 build the engine's
  `pc-debug` core and the template check with GCC 12 and clang 16/17; record the oldest that pass in
  `doctor/doctor_limits.hpp` and `nnh-tool.md`. Without the OK: keep the provisional values, marked "not verified".

## Stage 1: `nnh --version` and the command line (FR-1, FR-2)

- [x] **T1.1** `build_info`: version, remote and link protocol versions, host OS and architecture, build kind (pin
  present), pin, checkout folder; `version_line()` — files: `tools/nnh/src/build_info.{hpp,cpp}`, `tools/nnh/CMakeLists.txt`
  (definitions on `build_info.cpp` only: `NNHGE_VERSION`, `NNH_RELEASE_ENGINE_URL/_SHA256` cache entries, the checkout
  folder only without a pin) — test: `test_build_info.cpp` — covers FR-1
- [x] **T1.2** [P] The pin's configure-time check (both or neither, `https://` or `file://`, 64 hex digits) — files:
  `tools/nnh/CMakeLists.txt`, `tools/nnh/tests/pin_check/` — test: ctest `nnh_pin_check` — covers FR-9
- [x] **T1.3** CLI: `Command::{New, Doctor, Version}`, `--version`, `new` options (`--title`, `--author`, `--engine`,
  `--unique-id`), `doctor` options (`--online`, `--console <LAN ip>`, `--pc`), unknown options and a non-LAN console →
  error (exit 2), help text — files: `cli.{hpp,cpp}`, `cli_new_doctor.cpp`, `main.cpp` — test: `test_cli.cpp` — covers
  FR-2, FR-1
- [x] **T1.4** Stage end: `nnh --version` on Windows by hand; guards.

## Stage 2: `nnh new`'s core (FR-3 to FR-12)

- [x] **T2.1** Name and text rules: FR-4's rules with the named rule and a suggestion; title/author rules (UTF-8, ≤ 64,
  forbidden characters); `--unique-id` forms and range — files: `new/project_name.{hpp,cpp}` — test:
  `test_project_name.cpp` — covers FR-4, FR-11
- [x] **T2.2** [P] `random_unique_id()` — files: `new/unique_id.{hpp,cpp}` — test: `test_project_name.cpp` (10,000 draws
  in range) — covers FR-11
- [x] **T2.3** Engine location: pin or none; `--engine` (MSYS form converted) or the checkout default; engine-folder
  check and version; set-lines per shell; variable already equal / different — files: `new/engine_location.{hpp,cpp}` —
  test: `test_engine_location.cpp` — covers FR-9, FR-10
- [x] **T2.4** The embed step with a two-file stand-in template (one text with a placeholder, one binary): `-P` script,
  LF normalisation, fixed order, `DEPENDS` per file — files: `tools/nnh/embed_template.cmake`, `new/template_files.hpp`,
  `tools/nnh/CMakeLists.txt`, root `.gitattributes` — test: `test_template_render.cpp` (bytes equal the stand-in) —
  covers FR-7
- [x] **T2.5** Rendering: placeholders, the author line, pin/no-pin values, `@docs_base@`, unknown or leftover
  placeholder → test failure, deterministic output — files: `new/template_render.{hpp,cpp}` — test:
  `test_template_render.cpp` — covers FR-7, FR-8, FR-9
- [x] **T2.6** Writing: target checks (missing, empty folder, file, non-empty folder, link), sub-folders, exclusive
  creates, rollback at each failing index — files: `new/project_writer.{hpp,cpp}`, `new/new_fs_system.cpp` — test:
  `test_project_writer.cpp` (real folders under `NNH_TOOL_TEST_TMP`, a failing `NewFs`) — covers FR-5, FR-6
- [x] **T2.7** Stage end: guards; the module filters green; mutation spot-check of one rule per file.

## Stage 3: the template (FR-8, FR-12 to FR-14, FR-16)

- [x] **T3.1** The template's non-C++ files: `CMakeLists.txt.in`, `CMakePresets.json` (equal to
  `tests/consumer/CMakePresets.json`), `nnh.toml.in`, `gitignore`, `gitattributes` — files: `tools/nnh/template/` —
  test: `test_template_render.cpp` (exact file list; presets equal; `nnh.toml` read back without warnings) — covers FR-8
- [x] **T3.2** The game: `src/main.cpp`, `src/scenes/play_scene.{hpp,cpp}` (plan "The template"), assets
  `art/player.png` (engine-made, provenance in `tools/nnh/template/README.md`) and `sfx/blip.wav` — files:
  `tools/nnh/template/src/`, `assets/`; `tools/nnh/CMakeLists.txt` (`nnh_template_check`, engine warnings as errors) —
  test: `nnh_template_check` builds with 0 warnings in `pc-debug` and `pc-release` — covers FR-13, FR-14
- [x] **T3.3** `tests/smoke.nnhs` and the ctest `nnh_template_smoke` (headless, non-Release PC presets) — files:
  `tools/nnh/template/tests/smoke.nnhs`, `tools/nnh/tests/template_smoke.cmake` — test: the ctest exits 0; a broken
  expectation makes it fail (seen once) — covers FR-16
- [x] **T3.4** [P] Screenshot check: `NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=...` of the check game read back (both
  screens' text, the sprite) — covers FR-13
- [x] **T3.5** [P] `README.md.in`: build/run on PC and 3DS, the smoke script, `.cia` and the unique ID, "The engine"
  (pin, upgrade, `NNHGE_ENGINE_DIR`), `git init`, docs links, the licence note checked per library against
  `third_party/*/LICENSE*` and devkitPro's `licenses/` (files recorded in plan.md) — test: `test_template_render.cpp`
  (sections present) — covers FR-8, FR-12
- [x] **T3.6** Stage end: guards; `nnh_template_smoke` in `pc-debug` and `pc-dev`.

## Stage 4: `nnh new` end to end (FR-3, FR-15, FR-17)

- [x] **T4.1** `commands/new.cpp`: validate → choose engine → render → write → print (folder, file count, unique ID,
  set-lines, next commands) — files: `commands/new.cpp`, `commands.hpp`, `main.cpp` — test: ctest `nnh_new_files`
  (`tools/nnh/tests/new_files.cmake`) — covers FR-3, FR-1, FR-2
- [x] **T4.2** `nnh_new_smoke` (label `consumer-full`, `RESOURCE_LOCK consumer`): local way and pinned way (plan "Test
  strategy") — files: `tools/nnh/tests/new_smoke.cmake`, `tools/nnh/CMakeLists.txt` — test: the ctest on Windows
  `pc-debug` — covers FR-8, FR-9, FR-10, FR-13 to FR-16
- [x] **T4.3** Windows 3DS from a generated project outside the repository (no `scripts/dkp.sh`): `nnh build demo`,
  `nnh build demo --release`, `cmake --build build/3ds-dev --target dist` (the `.cia`), `check_command_length.sh` on
  both folders; 0 warnings — covers FR-15 (AC-3, Windows)
- [x] **T4.4** Budgets (AC-5): one edit of `play_scene.cpp`, 7 warm runs each, interleaved with the repository's `hello`,
  simulator and `.3dsx`; disk of the four build folders (the `doctor` thresholds re-set from it) — files:
  `docs/ROADMAP.md` (baseline table), `doctor/doctor_limits.hpp` — covers FR-17, FR-19
- [x] **T4.5** Stage end: guards; generated projects deleted (disk).

## Stage 5: `nnh doctor`'s core (FR-18, FR-20, FR-21)

- [x] **T5.1** `run_program` with a time limit and bounded output (a sleeping helper killed; output cut at the bound;
  a missing program) — files: `process_run.{hpp,cpp}` — test: `test_process_run.cpp` — covers FR-18
- [x] **T5.2** [P] Version parsers on recorded outputs (CMake 3.25 and 4.x, Ninja, GCC/MinGW-w64/WinLibs, clang, Apple
  clang, MSVC `cl`, `arm-none-eabi-gcc`, pacman folder names) + mini-fuzz — files: `doctor/tool_versions.{hpp,cpp}`,
  `tools/nnh/tests/data/doctor/` — test: `test_tool_versions.cpp`, `test_fuzz_tool.cpp` — covers FR-19
- [x] **T5.3** [P] Report: types, format, `fix:` lines, verdict lines, colour off/on, exit codes with `--pc` — files:
  `doctor/doctor_report.{hpp,cpp}` — test: `test_doctor_report.cpp` — covers FR-20, FR-21
- [x] **T5.4** Probes seam and fakes — files: `doctor/probes.hpp`, `doctor/probes_system.cpp`,
  `tools/nnh/tests/doctor_fakes.hpp` — test: used by stage 6; one real-probe smoke case (`cmake` found on this machine)
  — covers FR-18, FR-23

## Stage 6: `nnh doctor`'s checks (FR-18, FR-19, FR-23)

- [ ] **T6.1** About and Simulator (CMake, Ninja, compiler with minimum/checked, SDL3 per host, macOS and MSVC `warn`)
  — files: `doctor/checks_host.cpp`, `doctor/checks.hpp`, `doctor/doctor_limits.hpp` — test: `test_doctor_host.cpp`
  (AC-6: CMake 3.20 `FAIL`, Ninja missing `FAIL`, untested compiler `warn`, macOS `warn`) — covers FR-19, FR-23
- [ ] **T6.2** 3DS (devkitPro per OS, libraries, MSYS2 with CMake/Ninja inside, host compiler for 3DS) and Optional —
  files: `doctor/checks_3ds.cpp`, `doctor/checks_optional.cpp` — test: `test_doctor_3ds.cpp` (AC-6: `DEVKITPRO` unset
  `FAIL` + link, MSYS2 without Ninja `FAIL` + `pacman` line, no host compiler `FAIL`; optional never `FAIL`) — covers
  FR-19, FR-23
- [ ] **T6.3** This project (`nnh.toml`, engine version from the cache or `nnh.toml` or the repository, presets,
  `NNHGE_ENGINE_DIR`, disk) — files: `doctor/checks_project.cpp` — test: `test_doctor_project.cpp` (AC-6: version ≠ →
  `warn`) — covers FR-19
- [ ] **T6.4** Console and Downloads (only with the options; LAN only; pinned URLs listed; CA-certificate failure fix;
  the temporary script removed) — files: `doctor/checks_network.cpp`, `cmake/nnh_sdl3_pin.cmake`, `cmake/nnh_sdl3.cmake`
  — test: `test_doctor_network.cpp` (AC-8: no network probe without the options; the URL list equals the pins) — covers
  FR-18, FR-19
- [ ] **T6.5** Fix lines per problem and host, package names and links verified (winget IDs, Ubuntu packages,
  devkitPro's pages) — files: `doctor/fix_lines.{hpp,cpp}` — test: the check tests assert each fix — covers FR-20
- [ ] **T6.6** `commands/doctor.cpp` glue; real runs on Windows (AC-7: inside a generated project every required check
  `ok`, exit 0; `DEVKITPRO_MSYS2` pointing at an MSYS2 without Ninja → `FAIL` + fix, exit 1, Simulator still ready;
  `--pc`), offline time measured, project and user-config listings unchanged (AC-8), every fix line pasted in
  PowerShell, cmd and Git Bash — covers FR-18 to FR-21, FR-23
- [ ] **T6.7** Stage end: guards.

## Stage 7: versions on a protocol refusal (FR-22)

- [ ] **T7.1** The message builder — files: `protocol_mismatch.{hpp,cpp}` — test: `test_protocol_mismatch.cpp` — covers
  FR-22
- [ ] **T7.2** `ScriptRunner`: Hello answered with another version, Hello rejected before attaching; the project's engine
  version through `RunnerOptions`; `run.cpp`/`run_pc.cpp` pass it; `script_runner.cpp` stays ≤ 400 lines — files:
  `remote/script_runner.{hpp,cpp}`, `commands/run.cpp`, `commands/run_pc.cpp` — test: `test_script_runner.cpp` (fake
  game answering version 2; fake game rejecting the Hello) — covers FR-22
- [ ] **T7.3** [P] `nnh serve`: the link-version refusal printed with the versions — files: `commands/serve.cpp` — test:
  `test_serve.cpp` — covers FR-22

## Stage 8: mutants, docs and the verbatim walk (FR-24)

- [ ] **T8.1** Mutation pass (plan "Planned hand mutants"), one mutant at a time, judged by the named tests; survivors
  fixed or argued in plan.md.
- [ ] **T8.2** `docs/guides/your-first-game.md` (new): tools, `nnh doctor`, `nnh new`, simulator, console, smoke script,
  change the scene (budgets), add an asset, `.cia`, next steps; until spec 034, building `nnh` from a checkout and
  `NNHGE_ENGINE_DIR` — covers FR-24
- [ ] **T8.3** [P] `getting-started.md` rewritten around `doctor` and `new` (tools per system, then the guide; working
  inside the engine repository moves to a contributor section); `nnh-tool.md` (`new`, `doctor`, `--version`, options,
  report, exit codes, the limits table); `README.md` quick start; `game-projects.md` (`nnh new` generates it, the root
  layout); `docs/tech/cmake.md` (the check game, the ctests, the pin cache entries, `nnh_sdl3_pin.cmake`);
  `docs/architecture/project-structure.md`; `CLAUDE.md` (summary, layout, build lines); `decisions.md` (D56 change-log
  line) — covers FR-24
- [ ] **T8.4** Follow "Your first game" verbatim in an empty folder on Windows (reaches the simulator window and a
  passing smoke script); fix the page where it differs; `python -I scripts/docs_tool.py check` 0 broken links — covers
  FR-24 (AC-10)
- [ ] **T8.5** `scripts/vm_check.sh final` gains: a generated project under `/tmp` built for `3ds-dev` and `3ds-release`
  through `nnh` with `/opt/devkitpro` (+ `dist`), `nnh doctor` with `DEVKITPRO` unset (exit 1, the `FAIL` and fix) and
  set (exit 0), and the guide's Linux steps — files: `scripts/vm_check.sh` — covers FR-15, FR-19, FR-23, FR-24 (code
  only; run at stage 9)
- [ ] **T8.6** Final Windows guards: full `ctest -LE consumer-full` in `pc-debug` and `pc-release`, `ctest -L
  consumer-full -R nnh_new_smoke`, 0 warnings in the four presets, `hello.3dsx` SHA-256 unchanged, longest 3DS command,
  plan.md "Implementation results".

## Stage 9: review and Linux pass (together, once)

- [ ] **T9.1** One independent review (`nnh-reviewer`) and one `scripts/vm_check.sh final` on VM 120 (full `ctest` with
  `consumer-full` including `nnh_new_smoke`, GCC 14, 32-bit, ASan/UBSan, T8.5's steps); the reports and `doctor`
  outputs pasted into plan.md (AC-2/AC-3/AC-7/AC-10 Linux).
- [ ] **T9.2** Fixes, test first; re-checked with `scripts/vm_check.sh fast`.

## Stage 10: console run, merge, status

- [ ] **T10.1** Console (Old 3DS XL; the maintainer presses Y once per step): from a generated project outside the
  repository, `nnh run demo --script tests/smoke.nnhs` → `nnh: PASS`; `nnh install demo` → FBI installs the `.cia`;
  "Does it start from the HOME Menu with the default icon and banner? (yes/no)" — covers FR-15, FR-16 (AC-4)
- [ ] **T10.2** `docs/ROADMAP.md` checkbox and budget table; `specs/README.md` row; spec, plan and tasks status →
  implemented; D56 → implemented (change-log line).
- [ ] **T10.3** Merge to `main`; post-merge `ctest -LE consumer-full` on `main`; generated test folders deleted.
