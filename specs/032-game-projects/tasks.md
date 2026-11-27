# Tasks 032: Game projects outside the engine repository

Status: approved 2026-11-27 (with plan.md)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task is one or a few TDD loops (test first, then code). `[P]` means the task can be done in parallel with the
previous one. The work happens in a **separate git worktree** (branch `spec-032`) created from `main` after spec 031's
merge; no other spec is built in parallel. Order: the baseline; FR-19 first (it unblocks every 3DS build outside the
tree and must keep `hello` identical); then the consumer fixture written red and the CMake contract made green; then
`nnh`; then the Windows end-to-end runs, the repository regression, the docs; then the one review with the one
`scripts/vm_check.sh final` pass, fixes re-checked with `fast`; then the console. Deviations found while building go
into plan.md as "Stage N refinements". While developing, run the touched tests only (`nnh_tool_tests project preset
configure repo`, `ctest -R consumer_platform`, `ctest -R "^consumer$"`); the full suite at stage ends. Before stages 3
and 5, `df -h /e` must show ≥ 10 GB free.

Guards held at every stage end (the "stage-end guards"): ctest pc-debug and pc-release (`-LE consumer-full`); 3ds-dev and
3ds-release without a warning; 3ds-release `hello` `.3dsx` and stripped `.elf` **SHA-256 identical** to T0.1's (unless
the plan's fallback B was taken at T1.3: then identical to T1.3's recorded reference); pc-release `hello` `.text` size
identical; the target lists, option defaults and `nnh_games.txt` of every preset equal to T0.1's apart from
`nnh_core_objects`; `scripts/check_command_length.sh` ≤ 16,383 on `build/3ds-dev` and `build/3ds-release` (from T1.2).

## Stage 0: baseline

- [x] **T0.0** Start: `main` with spec 031 merged; worktree `spec-032`; `df -h /e` (≥ 10 GB) recorded; the spike's
  findings re-checked against the tree (the nine directory values and the `NNH_PLATFORM` reads in `cmake/`; the four
  `find_repo_root` callers). Files: plan.md ("Stage 0 results"). Covers FR-6, FR-11.
- [x] **T0.1** Repository baseline (Windows; scripts in `build/ref-032/`, not committed): fresh `pc-debug`, `pc-release`,
  `3ds-dev`, `3ds-release`; for each: the sorted target list (`ninja -t targets all`), the `NNH_*` cache entries and
  their values, `nnh_games.txt`; `hello`: 3ds-release `.3dsx` and stripped `.elf` SHA-256 and sizes, `nm -C` lists,
  pc-release `.text` size; the link commands of `hello` and `nnh_tests` (`ninja -t commands`), kept for T1.2's
  comparison; every example golden as in the tree (`--pc` check scripts). Pre-measured on 2026-11-27 from the existing
  folders: 3ds-release `hello.3dsx` 241.7 KB, SHA-256 `db7d888f…` (T0.1 replaces it with the base commit's). Files:
  plan.md. Covers FR-17, AC-8.
- [x] **T0.2** [P] Command lengths and times (3 runs each): `ninja -t commands` longest per 3DS preset (pre-measured:
  27,354 = 83.5 %, the `libnnh_core.a` archive of 447 objects; then 2,533, 1,914, 1,245); edit `examples/hello/main.cpp`
  → simulator and `.3dsx` times; clean engine build (`nnh_core` + platform, pc-dev and 3ds-release); fresh configure
  times of pc-debug and 3ds-dev. Files: plan.md; `docs/ROADMAP.md` baseline table untouched until T6.3. Covers III,
  FR-19, AC-8, AC-11.
- [x] **T0.3** [P] Design measurements in a temporary folder outside the repository (deleted afterwards): which `file://`
  URL forms native CMake and devkitPro's MSYS CMake each download (`file:///E:/…`, `file://E:/…`, `file:///e/…`;
  plan question 12); `cmake --preset X -D…` overriding a preset's `$env{}` mapping in MSYS CMake as in native (measured
  native on 2026-11-27); `arm-none-eabi-gcc-ar qc lib.a @list` with relative paths from MSYS bash. Files: plan.md.
  Covers FR-1, FR-15, FR-19.

## Stage 1: the command-line limit (FR-19)

- [x] **T1.1** The check first: `scripts/check_command_length.sh <build dir> [limit]` (`ninja -C <dir> -t commands`;
  prints the longest length, its share of 32,767 and its first 100 characters; exit 1 above the limit, default
  16,383; exit 2 without a `build.ninja`). Test: on today's `build/3ds-release` it prints 27,354 and exits 1 (red);
  with `32767` it exits 0; on a folder without `build.ninja` exit 2. Covers FR-19, AC-11.
- [x] **T1.2** The archive from a response file: `engine/core/CMakeLists.txt` (`nnh_core_objects` OBJECT library with
  every source, per-file option and include as before), `engine/core/archive/CMakeLists.txt` (static `nnh_core` from it,
  `LINKER_LANGUAGE CXX`, the same PUBLIC include/definitions/features and PRIVATE link libraries, archive output in
  `engine/core/`, `CMAKE_NINJA_FORCE_RESPONSE_FILE` only when `NNH_PLATFORM` is `3ds`), `tests/CMakeLists.txt` (fuzz
  instrumentation on `nnh_core_objects`; shell edit keeping the file's encoding). Test: T1.1's script ≤ 16,383 on
  3ds-dev and 3ds-release (record the new longest); the archive's member list (`ar t`) and order equal to T0.1's; the
  `hello` and `nnh_tests` link commands equal to T0.1's; the stage-end guards (hash identical); the Linux 3DS build is
  checked in stage 8. Covers FR-19, FR-17, AC-11.
- [ ] **T1.3** (only if T1.2 fails) Fallback B: split the core into a few static libraries (module groups, link order
  kept stable, cycles checked); `hello`'s one-time change checked (sizes, `nm -C` diff, disassembly of moved objects)
  and recorded as FR-17's new reference in plan.md and spec.md's verification notes. Covers FR-19, FR-17.
- [x] **T1.4** Stage end: the stage-end guards; plan.md ("Stage 1 results": lengths before/after). Covers AC-11.

## Stage 2: the consumer fixture (red) and the CMake contract (green)

- [x] **T2.1** The fixture skeleton and driver, written to fail: `tests/consumer/` (`CMakeLists.txt` with
  `CONSUMER_WAY`, `CMakePresets.json` with the six presets and the `NNHGE_ENGINE_DIR` mapping, `nnh.toml`, `.gitignore`,
  `game/` with `SOURCES`, `ASSETS` (one PNG, one data file) and `DATA_SCHEMA`, `small/`), `tests/consumer_tools/
  run_consumer.cmake` (way `local`: configure, build, run, the self-check line), `tests/consumer.cmake` and its
  `include` line in `tests/CMakeLists.txt`. Test: `ctest --preset pc-debug -R "^consumer$"` fails with the spike's
  `/tools/data_schema` error (red). Covers FR-16, VI.
- [x] **T2.2** The platform: `cmake/nnh_platform.cmake`, `CMakeLists.txt` (global property), every helper reading it
  (`nnh_game`, `nnh_assets`, `nnh_images`, `nnh_data`, `nnh_host_tools`'s `nnh_assetc_command`); the check first in
  `nnh_add_game`. Test first: `tests/consumer_tools/unknown_platform/` and the ctests `consumer_platform_empty` and
  `consumer_platform_unknown` (expected message with the game, `"wii"`/`empty`, `pc, 3ds`; no `build.ninja` target of
  the game). Covers FR-6, AC-12.
- [x] **T2.3** Engine files from the function: `CMAKE_CURRENT_FUNCTION_LIST_DIR` in `nnh_game`, `nnh_assets`,
  `nnh_images`, `nnh_cia`, `nnh_ctr_tools`, `nnh_package_rules`, `nnh_data` (`_NNH_*` directory variables removed; the
  data root kept as `<cmake>/..`). Test: `grep -n "_NNH_[A-Z_]*_DIR\|_NNH_PACKAGE_ART\|_NNH_DATA_" cmake/` finds no
  file-scope `set`; `consumer` gets past configure and the asset steps; in-repository `build.ninja` diff shows no path
  change of the helper scripts. Covers FR-6.
- [x] **T2.4** Only what a game needs: `cmake/nnh_optional_parts.cmake`, `CMakeLists.txt` (the four options with
  `PROJECT_IS_TOP_LEVEL` defaults, `NNH_BUILD_TOOLS` PC only, `link/` gate, `enable_testing()` only with tests). Test:
  `consumer`'s target list, `ctest -N` (only the consumer's own test) and `nnh_games.txt` checks; the in-repository
  target lists and defaults equal to T0.1's. Covers FR-4, AC-1.
- [x] **T2.5** [P] Game target defaults and the cache switch: `cmake/nnh_game_defaults.cmake`, `nnh_game.cmake`,
  `nnh_data.cmake` (the schema objects and program), `CMakeLists.txt` (`NNH_CCACHE` searched only when undefined or
  `*-NOTFOUND`). Test: `compare_flags.cmake` (the game's compile command = the parent's `hello` apart from paths and
  `NNH_GAME_TITLE`), the launcher on the game's compile when `NNH_CCACHE` is set, none with `-DNNH_CCACHE=`; a scratch
  configure with `CMAKE_CXX_STANDARD 23` keeps `-std=c++23` on the game. Covers FR-7, FR-8, AC-7.
- [x] **T2.6** [P] Version: `cmake/nnh_version.cmake`, `CMakeLists.txt`, the status line `NNHGE <version>: <dir>`. Test:
  the driver compares `NNHGE_VERSION*` in the consumer (a configure-time check in `tests/consumer/CMakeLists.txt` against
  `CONSUMER_EXPECT_VERSION`) and in the parent's `CMakeCache.txt` with the engine's `project()` version. Covers FR-10,
  AC-7.
- [x] **T2.7** Stage end: `consumer` green (PC, the skeleton's assets); the stage-end guards; plan.md ("Stage 2
  results"). Covers FR-17.

## Stage 3: the full fixture and the four ways

- [x] **T3.1** Every keyword and asset kind: `tests/consumer/game/` gains `IMAGES`, `LANGUAGES en pt-BR BASE en`,
  `TITLE`/`DESCRIPTION`/`AUTHOR`/`VERSION`, `ICON`, `CIA UNIQUE_ID 0xF7032 PRODUCT_CODE CTR-N-NNHC BANNER BANNER_AUDIO`
  and assets copied from `tests/data/` (a Tiled map + tileset, an Aseprite export, data files, a `.font` over
  `nnhtest.ttf`, two string files, a glTF model, a WAV, package art). Test: `consumer` builds every asset step on PC; a
  3DS configure of the fixture through `scripts/dkp.sh`-style commands generates the `.3dsx`, `dist` and `_cia` targets.
  Covers FR-6, FR-16, IX.
- [x] **T3.2** The self-check: `game/src/` (`main.cpp`, `check_scene.{hpp,cpp}`, `self_check.{hpp,cpp}`): one scene
  loads each kind and prints `consumer: PASS <crc32>`; `game/tests/check.nnhs`. Test: the golden recorded and identical
  in pc-debug and pc-release; changing one asset value changes it (once by hand). Covers FR-16, AC-1.
- [x] **T3.3** [P] `plain/` and `tests/consumer_tools/plain_only/`: a project library compiled with and without the
  engine; the driver compares the two compile commands. Covers FR-8, AC-7.
- [x] **T3.4** The other ways (label `consumer-full`): `consumer_archive` (tarball of the engine folder without `build/`
  and `.git`, SHA-256, `CMAKE_DISABLE_FIND_PACKAGE_Git=ON`), `consumer_archive_override`, `consumer_submodule` (clone of
  `HEAD`; skipped with a message without `.git`), `consumer_pruned` (without `examples/ tests/ apps/ specs/ docs/`; then
  a second configure with `NNH_BUILD_EXAMPLES=ON` fails naming the option and folder), `consumer_bad_hash`; AC-5
  listings of the copied engine before/after; each way's configure time recorded; each build deleted after it passes.
  Test: every way prints the same golden. Covers FR-1, FR-2, FR-3, FR-5, FR-9, AC-2, AC-5.
- [x] **T3.5** [P] `consumer_options_on` (configure only, all four options ON: targets exist, `ctest -N` lists the
  engine's tests). Covers FR-4, AC-13.
- [x] **T3.6** Cost: `consumer` time on Windows pc-debug cold and warm, with and without `CCACHE_NOHASHDIR=1` (ccache
  hit counts); Linux SDL3 reuse wired (`FETCHCONTENT_SOURCE_DIR_NNH_SDL3`) for stage 8. Files: plan.md. Covers III, Q4.
- [x] **T3.7** Stage end: the stage-end guards; plan.md ("Stage 3 results"). Covers AC-1, AC-2.

## Stage 4: `nnh` in a game project

- [x] **T4.1** Root detection: `tools/nnh/src/project_root.{hpp,cpp}`. Test first: `tools/nnh/tests/test_project_root.cpp`
  (engine repository; game project; a game project inside the engine repository; nested folders below each; a folder
  with `nnh.toml` *and* the engine markers → game; nothing found → empty; the message names `nnh.toml` and the engine
  repository); `test_repo.cpp`'s old root cases moved. Covers FR-11, AC-6.
- [x] **T4.2** [P] `nnh.toml`: `tools/nnh/src/project_file.{hpp,cpp}` over `remote::read_tunes`. Test first:
  `test_project_file.cpp` (empty; `name` and `engine`; an unknown key → one warning line; a table → warning; malformed
  lines → `nnh.toml:<line>: reason`; > 64 KiB refused), a mini-fuzz case in `test_fuzz_tool.cpp`. Covers FR-12, X,
  AC-6.
- [x] **T4.3** [P] Presets: `tools/nnh/src/preset_names.{hpp,cpp}`. Test first: `test_preset_names.cpp` (the
  repository's file; a user file; escaped strings; a preset only in `buildPresets`; missing; malformed JSON → not
  found; > 1 MiB refused). Covers FR-13, AC-6.
- [x] **T4.4** The configure command: `tools/nnh/src/host_compiler.{hpp,cpp}`, `repo.{hpp,cpp}` (`configure_command`,
  `ConfigureExtras`). Test first: `test_configure_command.cpp` (`NNH_HOST_CXX` from the environment, from `PATH`
  (`g++` before `c++` before `clang++`), absent; Windows 3DS: `/c/...` for `C:\...` and `c:/...`, spaces quoted, the
  override from `NNHGE_ENGINE_DIR` as `/e/...`, a `'` refused; Linux 3DS and every PC preset: `cmake --preset <p>`
  unchanged). Covers FR-14, FR-15, AC-6.
- [x] **T4.5** The commands: `commands/build.cpp` (root, `nnh.toml`, preset check before CMake, `configure_command` on
  both configure paths), `run.cpp`, `run_reload.cpp`, `serve.cpp`, `tools/nnh/CMakeLists.txt`. Test: `nnh_tool_tests`
  build-path cases with a fake shell runner where they exist; `nnh_script_pc_smoke` passes; from `tests/consumer/` (a
  game project inside the repository) `NNH_PRESET=pc-debug nnh build nnh_consumer` builds into
  `tests/consumer/build/pc-debug/` (git-ignored by the fixture's `.gitignore`). Covers FR-11, FR-13, AC-6.
- [x] **T4.6** Stage end: the stage-end guards; `nnh_tool_tests` count; plan.md ("Stage 4 results"). Covers AC-6.

## Stage 5: Windows end to end (outside the repository)

- [ ] **T5.1** `tests/consumer_tools/copy_project.sh <dest> [pin|local]`: copies the fixture, writes `engine_pin.cmake`
  (a tarball of the engine folder and its SHA-256, in the `file://` form T0.3 found) or nothing (local). Dest for this
  stage: a path of ≥ 64 characters outside the repository (e.g. `E:/RANDOM/nnhge-032-checks/a-game-project-folder-with-a
  -deliberately-long-name`). Covers FR-16, AC-11.
- [ ] **T5.2** From the copy, fresh build folders, no `scripts/dkp.sh` run, `nnh` from the worktree's pc-debug: `nnh build
  nnh_consumer` (3ds-dev) and `nnh build nnh_consumer --release` → `.3dsx`; `nnh run nnh_consumer --pc --script
  game/tests/check.nnhs` → exit 0, log under `<copy>/build/logs/`; then with `NNHGE_ENGINE_DIR` set (drive form) and the
  pin file removed: `pc-debug` and `3ds-dev` again; `nnh serve` lists only the consumer's games. With no compiler on
  `nnh`'s `PATH` and no `NNH_HOST_CXX`: the 3DS configure fails with the `NNH_HOST_CXX`/`NNH_ASSETC` message. Covers
  FR-1, FR-13, FR-14, FR-15, AC-3.
- [ ] **T5.3** [P] In the worktree: delete `build/3ds-dev`, then `nnh build animation` → configured by `nnh` and built.
  Covers FR-14, AC-3.
- [ ] **T5.4** Outputs and pins: after T5.2, `git status --porcelain` of the engine checkout empty; SDL3 under
  `<copy>/build/pc-*/_deps/`, makerom/bannertool under `<copy>/build/3ds-*/_tools/` after a `dist`; nothing under the
  engine folder. Covers FR-2, FR-9, AC-5.
- [ ] **T5.5** [P] Lengths: `scripts/check_command_length.sh` on the copy's `build/3ds-dev` and `build/3ds-release`
  (≤ 16,383; record), and the object count at which a game's own link line would reach 50 % (plan question 10). Covers
  FR-19, AC-11.
- [ ] **T5.6** Budgets (3 runs each): edit `small/main.cpp` → simulator (pc-debug) and `.3dsx` (3ds-dev); `ninja -v`
  shows the cache launcher on the game's compile; fresh and repeated configure times for the pin and local ways; the
  copy's build folder sizes. Then delete the copy. Covers FR-7, III, AC-9.
- [ ] **T5.7** Stage end: plan.md ("Stage 5 results"). Covers AC-3, AC-9, AC-11.

## Stage 6: the repository unchanged

- [ ] **T6.1** Regression: target lists, defaults and `nnh_games.txt` of every preset against T0.1 (only
  `nnh_core_objects` new); every example's `--pc` check script and golden; 3ds-release `hello` SHA-256 (`.3dsx`, stripped
  `.elf`) and pc-release `.text` size; `scripts/check_release_strings.sh` on `hello`; `ctest --preset pc-debug` and
  `pc-release` (all labels but `consumer-full`). Covers FR-17, AC-8.
- [ ] **T6.2** [P] Build times against T0.2 (3 runs): `hello` one-file edit (simulator, `.3dsx`), clean engine, fresh
  configures. Covers III, AC-8.
- [ ] **T6.3** `docs/ROADMAP.md` baseline table: a line "after spec 032" with T6.2's numbers and T5.6's game-project
  numbers. Covers AC-8, AC-9.
- [ ] **T6.4** Mutation pass (plan.md's planned hand mutants, one at a time, with the consumer and `nnh_tool_tests`
  filters); survivors become tests or are argued equivalent. Files: plan.md ("Mutation testing"). Covers VI.

## Stage 7: docs

- [ ] **T7.1** `docs/guides/game-projects.md` (new): a game project by hand — the four ways' `CMakeLists.txt`, the six
  presets with the local-engine mapping, `nnh.toml`, `.gitignore`, where outputs go, using a local engine checkout (per
  machine and per shell), upgrading the pin, reading `NNHGE_VERSION`, the options, the reserved names, 3DS builds on
  Windows (MSYS forms by hand, `NNH_HOST_CXX`), CA certificates on Windows, the large-game link note; `getting-started.md`
  §5 points to it. Covers FR-18, AC-10.
- [ ] **T7.2** `docs/tech/cmake.md` (the subproject contract, the four options, `link/` gate, game target defaults,
  `NNH_CCACHE`, `NNHGE_VERSION*`, the reserved target names and cache entries taken from the consumer's `--target help`
  and cache, the archive response file and `check_command_length.sh`, `engine/core/archive/`, the consumer tests and
  labels, `vm_check.sh`'s new step), `docs/guides/nnh-tool.md` (game projects, `nnh.toml`, the root rule and message,
  the preset check, `NNH_HOST_CXX`, `NNHGE_ENGINE_DIR`), `docs/architecture/project-structure.md` (`tests/consumer/`,
  `tests/consumer_tools/`, the new `cmake/` files), `CLAUDE.md` (layout and build lines), `README.md` if it says games
  live in the repository. Covers FR-18, AC-10.
- [ ] **T7.3** `docs/architecture/decisions.md`: D55's change-log note (approved/implemented, stage refinements);
  `python -I scripts/docs_tool.py check` → 0 broken links. Covers AC-10.
- [ ] **T7.4** The page verbatim: in an empty folder outside the repository, follow `game-projects.md` word for word
  (local way, then the pin with a locally made archive) → `nnh run <game> --pc` shows the window (screenshot read back);
  every difference becomes a doc fix. Covers FR-18, AC-10.

## Stage 8: review and Linux (once, after stages 0–7)

- [ ] **T8.1** `scripts/vm_check.sh`: `final` gains `ctest --preset pc-debug -L consumer-full` and a 3DS build of a
  consumer copy (`copy_project.sh` under `/tmp`, `DEVKITPRO=/opt/devkitpro`, `nnh build nnh_consumer` and `--release`)
  plus `nnh build animation` in a fresh `build/3ds-dev`. Test: the script's new step on the VM. Covers AC-1, AC-3.
- [ ] **T8.2** The lead: one independent review (`nnh-reviewer`: every helper path that read the caller's scope, the
  repository unchanged, `tests/CMakeLists.txt`'s one-line diff and encoding) together with one `scripts/vm_check.sh
  final` on VM 120 (GCC 13 `ctest` with `consumer` and `consumer-full`, GCC 14, 32-bit, ASan/UBSan, the 3DS consumer
  build); fixes re-checked with `scripts/vm_check.sh fast`. Files: plan.md ("Review round", "Stage 8 results"). Covers
  AC-1, AC-2, AC-3, I.

## Stage 9: console (the only steps on the 3DS)

- [ ] **T9.1** (on the PC) A fresh consumer copy (`local`), `nnh build nnh_consumer` (3ds-dev) and `--release` without a
  warning; 3ds-release `hello` identical to the reference. Covers AC-4.
- [ ] **T9.2** Old 3DS XL, nobody touching it after Y: from the copy, `nnh run nnh_consumer --script
  game/tests/check.nnhs` → PASS, the self-check line equal to the PC's, log under `<copy>/build/logs/`; then `nnh install
  nnh_consumer` → FBI installs the `.cia` and it starts from the HOME Menu with the fixture's icon and banner (yes/no).
  Files: plan.md ("Console run"). Covers AC-4, FR-9, FR-13.

## Stage 10: status

- [ ] **T10.1** `docs/ROADMAP.md`: an entry for game projects outside the repository (spec 032) in the 1.0 release
  section (created here if 033/034 have not), with the implemented summary. Covers FR-18.
- [ ] **T10.2** spec, plan, tasks and `specs/README.md` status → implemented; D55 → implemented in `decisions.md`.
- [ ] **T10.3** (the lead) Merge `spec-032` into `main`; then `ctest --preset pc-debug` (with `consumer`), every example's
  `--pc` script, one 3ds-release build with `hello`'s hash; the worktree's build folders and copies deleted (disk).
