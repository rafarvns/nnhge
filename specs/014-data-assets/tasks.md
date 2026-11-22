# Tasks 014: Data assets

Status: approved (2026-11-22, maintainer pre-approved); in progress — stages 0–2 done
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops. `[P]` = can be done in parallel with the previous task. Every stage ends with
something verifiable on PC (`ctest --preset pc-debug`, a converter run, a screenshot); the only console step is
T8.4, a scripted check where the maintainer only presses Y. Deviations found while building go into plan.md as
"Stage N refinements", as in plan 012.

## Stage 0 — baseline and toolchain probe

- [x] **T0.1** baseline numbers (Windows; 3 runs each): edit `examples/hello/main.cpp` → simulator and `.3dsx`
  build times; clean engine build; `hello.exe` and `hello.3dsx` sizes and their Ninja step lists
  (`ninja -t commands hello`); `platformer` builds clean — files: this file (result) — covers III, AC-6

  **Result (2026-11-22, commit `b9e52bf`, maintainer's PC: WinLibs GCC 16.2 + ccache with every edit a miss /
  devkitARM GCC 16.1 without ccache):** edit `examples/hello/main.cpp` → simulator (`pc-debug`, target `hello`)
  1.71 / 1.66 / 1.67 s; → `.3dsx` (`3ds-release`, target `hello`) 2.67 / 2.64 / 2.62 s. Clean 3DS engine
  (`nnh_core` + `nnh_platform_3ds`) 10.97 / 10.92 s; the rest of a clean `3ds-release` (examples, NNH Link, host
  tool) 15.6 s, no warnings; 3DS no-op build 2.9 s. Sizes: Release `hello.3dsx` **261,284** bytes, `hello.elf`
  2,487,084; `pc-release` `hello.exe` **276,599** bytes. Ninja step lists of target `hello` (`ninja -t commands`):
  122 commands in `pc-debug` and `pc-release`, 116 in `3ds-release` (118 for `hello_3dsx`); kept in
  `build/tmp/s014/hello_cmds_*_before.txt` for T7.8. `platformer` builds clean in both presets.
- [x] **T0.2** probe (scratch, not committed) — files: a 40-line `probe.cpp` — test: compiles and runs with WinLibs
  GCC, devkitARM (`scripts/dkp.sh`), GCC 13 and clang 18 on VM 120: `using enum` in a constant lambda,
  `offsetof` on a standard-layout aggregate holding `String<N>` and `List<T, N>`, `&type_tag<T>` as a constant
  across two translation units, a `constinit` list head with static registrars. **Also the CMake spike**: an
  `ExternalProject` whose build step depends on `$<TARGET_OBJECTS:x>` of a console OBJECT library (via a stamp
  custom command if needed) in `3ds-release`; measure a no-op build with it vs `BUILD_ALWAYS`; write the choice back
  into plan.md (D29 fallback) — covers FR-1, FR-3, III (risks)

  **Result (2026-11-22): the language features work on both Windows compilers; the object dependency works, so D29's
  main path is kept (no `BUILD_ALWAYS`).** Probe (two translation units, `-std=c++20 -Wall -Wextra -Werror -O2`):
  WinLibs GCC 16.2 compiles and runs it (`nodes=2 same_tag=1 same_off=1 enum=6`); devkitARM GCC 16.1 compiles and
  links it for the 3DS (`-specs=3dsx.specs`, libctru). Checked: `using enum` inside a `constexpr` lambda, `offsetof`
  of a `List<float, 4>` member in a constant `Node`, `&type_tag<T>` in a constant expression and equal across the two
  units, a `constinit` list head filled by static registrars in both units. GCC 13 and clang 18 on VM 120: **left to
  the maintainer** (VMs are not started by agents for this task; the same probe is the first thing T9.1 runs).
  CMake spike (`3ds-release` toolchain through `dkp.sh`, a console OBJECT library + an `ExternalProject` building the
  same source for the host): a stamp custom command `DEPENDS $<TARGET_OBJECTS:schema_objs>`, added to the build
  step with `ExternalProject_Add_StepDependencies(... build <stamp>)`. No-op build **0.31 s** (nothing runs) against
  **1.27 s** with `BUILD_ALWAYS` (the build step, its install and the description step run every time). Editing a
  header included by the schema source rebuilds the console object, the stamp, the host program and the
  description; editing a game source unrelated to the schema runs no host step. Caveat for T7.3: the host program's
  own sources (`tools/data_schema/schema_main.cpp`, core's pure registry files) are not in the console objects, so
  they must be file dependencies of the build step as well.

## Stage 1 — declaring data types in C++ (pure, PC tests)

- [x] **T1.1** value types `String<N>`, `List<T, N>`, `Ref<T>`, `TextureRef`/`SoundRef`/`MusicRef` — files:
  `engine/core/include/nnh/data.hpp` — test: `tests/data_assets/test_string_list.cpp`, `test_refs.cpp` (trivially
  copyable, standard layout, defaults, `assign` truncation at a code point, `==`, `push_back` past N, empty) —
  covers FR-2, FR-8
- [x] **T1.2** kind deduction and `FieldInfo` (`make_field`, every kind/width, arrays, lists, nested, options,
  static_asserts with readable messages) — files: `engine/core/include/nnh/detail/data_schema.hpp` — test:
  `test_schema_declare.cpp`; ctest `data_schema_rejects_*`/`explains_*` on `tests/data_assets/compile_fail/*.cpp`
  (EXCLUDE_FROM_ALL objects built by the test) — covers FR-1, FR-2
- [x] **T1.3** macros `NNH_DATA_TYPE`/`STRUCT`/`ENUM`/`FIELD`, `Registrar`, registry lookups by tag and key, enum
  name splitting — files: `data_schema.hpp`, `engine/core/src/data/schema_registry.{hpp,cpp}`,
  `enum_names.{hpp,cpp}` — test: `test_schema_registry.cpp`, `test_enum_names.cpp`, tests' schema
  `tests/data_assets/test_types.{hpp,cpp}` — covers FR-1, FR-2
- [x] **T1.4** registry check (duplicate key/type, unresolved nested/enum type, `Ref` to a keyless type, depth > 4,
  reserved and malformed keys, type/enum counts), on local registries — files: `data/schema_check.cpp` — test:
  `test_schema_registry.cpp` — covers FR-1, FR-14
- [x] **T1.5** [P] fingerprint (canonical encoding, clamped ranges) — files: `data/schema_fingerprint.cpp`,
  `data/data_format.hpp` — test: `test_fingerprint.cpp` (stable value pinned; changes with each covered property;
  unchanged by member order, default values and offsets) — covers FR-10
- [x] **T1.6** [P] description writer — files: `data/schema_write.{hpp,cpp}` — test: `test_schema_write.cpp`
  (exact text of the tests' schema) — covers FR-3

  Stage end: `ctest --preset pc-debug` green; PC Debug and Release build; `3ds-release` builds (the header compiles
  for the console).

  **Result (2026-11-22):** unit tests in `tests/data_assets/` — `test_string_list.cpp` 11, `test_refs.cpp` 2,
  `test_schema_declare.cpp` 15, `test_enum_names.cpp` 5, `test_schema_registry.cpp` 7, `test_schema_check.cpp` 30,
  `test_fingerprint.cpp` 18 (Enemy pinned `0xFCEF3C32`), `test_schema_write.cpp` 5, plus `crc32_update` (1) in
  `tests/core/test_crc32.cpp`; the tests' schema `test_types.{hpp,cpp}`. 25 compile-fail ctests:
  `data_schema_accepts_good` and `data_schema_rejects_*`/`data_schema_explains_*` for unknown member, pointer,
  double, int64, `std::string`, `.min` on a string, min > max, a field listed twice, not an aggregate, over 16 KiB,
  65 fields, unknown enum name. ctest 176/176; `pc-debug`, `pc-release`, `3ds-release` without warnings; the tests'
  schema compiles for the 3DS with devkitARM `-Werror`. Refinements: plan.md "Stage 1 refinements".

## Stage 2 — the converted format: validator and apply (pure)

- [x] **T2.1** test-only writer of `NNHD` buffers by field index (`seal()` fixes size and CRC) — files:
  `tests/data_assets/data_file.{hpp,cpp}` — test: used by T2.2–T2.4 — covers FR-10
- [x] **T2.2** bounded validator — files: `data/data_validate.{hpp,cpp}`, `data_format.hpp` — test:
  `test_data_validate.cpp` (magic, version → `Unsupported`, size, CRC, fingerprint message, field order, every
  kind's bounds incl. `min`/`max`, UTF-8, list counts, trailing bytes, each limit + 1) — covers FR-10, FR-11, FR-14
- [x] **T2.3** apply through offsets (ints by width, strings zero-filled, enum values from the table, list counts,
  nested, arrays of structs) — files: `data/data_apply.{hpp,cpp}` — test: `test_data_apply.cpp`,
  `test_data_layout.cpp` (two structs, different member order/padding, one schema → same bytes, same values) —
  covers FR-2, FR-10, FR-11
- [x] **T2.4** [P] fuzz target `data_target` (validate as is, then with size/CRC fixed; apply accepted buffers into
  a struct of a schema with every kind) — files: `tests/fuzz/fuzz_data.cpp`, `libfuzzer_data.cpp`,
  `fuzz_targets.*`, `test_fuzz_smoke.cpp`, `tests/CMakeLists.txt` — test: mini-fuzz in `nnh_tests` (> 100 accepted
  mutants) — covers AC-5, X

  **Result (2026-11-22):** `test_data_validate.cpp` 20, `test_data_apply.cpp` 10, `test_data_layout.cpp` 2,
  `utf8_valid` 2 in `tests/text/test_utf8.cpp`, mini-fuzz `data_target_survives_mutated_inputs` (6 000 mutants of a
  file with every kind, > 100 accepted, plus random short inputs); libFuzzer `fuzz_data` wired (clang only, not run
  yet: T9.1). `nnh_tests` 1117/1117 (Debug), 1084/1084 (Release); ctest 176/176; `pc-debug`, `pc-release`,
  `3ds-dev`, `3ds-release` without warnings. Opt-in: Release `hello.exe` 276,599 bytes and `hello.3dsx` 261,284,
  both equal to T0.1, no data symbol in either (`nm`); `ninja -t commands hello` lists 9 more commands (131 / 125),
  exactly the 9 new `nnh_core` sources compiled once for the library — hello's own steps are unchanged (T7.8 checks
  again). Hand mutation testing of the validator, apply, fingerprint and description writer: 90 mutants, **84
  killed**; the first run left 11 survivors, and tests were added for the 5 real ones (an enum missing from the
  registry, an impossible integer width, an enum index equal to the name count, a buffer shorter than a header,
  an unsigned full-range field in the description); the 6 left are equivalent (a read check repeated later,
  `== 1` vs `!= 0` on a validated bool, the fingerprint memo, a marker for unknown types, depth guards and an
  ignored element failure that change nothing observable). Refinements: plan.md "Stage 2 refinements".

## Stage 3 — the index and the loading API (fake platform)

- [ ] **T3.1** `core/path_hash` (FNV-1a; vectors) and the `NNHX` validator + lookups (by group and hash, by type
  and position) — files: `engine/core/src/core/path_hash.{hpp,cpp}`, `data/index_validate.{hpp,cpp}`,
  `data_file.*` (index writer) — test: `tests/core/test_path_hash.cpp`, `test_index_validate.cpp` — covers FR-12,
  FR-14
- [ ] **T3.2** [P] fuzz target `data_index_target` — files: `tests/fuzz/fuzz_data.cpp`,
  `libfuzzer_data_index.cpp`, `fuzz_targets.*` — test: mini-fuzz — covers AC-5, X
- [ ] **T3.3** shutdown hooks in core; index load/release with the scratch (tag `data`), missing-type warning;
  `kMaxAssetIndexBytes` → 256 KiB — files: `core/shutdown_hooks.{hpp,cpp}`, `app/app.cpp`,
  `data/data_index.{hpp,cpp}`, `assets/assets_internal.hpp` — test: `test_data_index.cpp` (`tests/data_assets/`),
  `tests/app/` hook test — covers FR-13
- [ ] **T3.4** `load<T>` by path and by reference: errors, `out` unchanged, in-place `T{}`, stale fingerprint
  message — files: `data/data_load.cpp`, `data.hpp` — test: `test_data_load.cpp` — covers FR-11, FR-8
- [ ] **T3.5** `count`/`name`/`ref`/`find`/`path`/`preload`/`release`/`memory_used` — files: `data/data_list.cpp`
  — test: `test_data_list.cpp`, `test_data_alloc.cpp` (formula = tag stats; no allocation over 100 calls after
  `preload`; shutdown frees all) — covers FR-12, FR-13, AC-7

  Stage end: ctest green; `hello` links no `nnh::data` symbol (`nm`) — the opt-in check starts early.

## Stage 4 — the TOML-subset reader (`nnh-assetc`)

- [ ] **T4.1** document model and lexer/parser: lines, comments, keys, headers `[a.b]`/`[[a.b]]`, key or table set
  twice, BOM, CRLF/lone CR, UTF-8, file and line limits, errors with lines — files:
  `tools/assetc/src/toml.{hpp,cpp}`, `toml_parse.cpp` — test: `tools/assetc/tests/test_toml.cpp`,
  `test_toml_tables.cpp` — covers FR-5, FR-6, FR-14
- [ ] **T4.2** values: bools, integers (`_`, sign, leading zeros), floats via `from_chars` (hard decimal cases),
  basic strings and escapes, inline arrays (multi-line, trailing comma, depth 2); each refused TOML feature with
  its message — files: `toml_value.{hpp,cpp}` — test: `test_toml_values.cpp` — covers FR-5, FR-16
- [ ] **T4.3** [P] fuzzing: mini-fuzz of the reader + `fuzz_assetc_toml` — files: `tools/assetc/tests/fuzz_assetc.*`,
  `libfuzzer_assetc_toml.cpp`, `tools/assetc/CMakeLists.txt` — test: `test_fuzz_assetc.cpp` — covers AC-5, X

## Stage 5 — `nnh-assetc data`: one file against the schema

- [ ] **T5.1** description reader → registry nodes (bounded; version) — files: `tools/assetc/src/schema_read.{hpp,cpp}`
  (the library also compiles core's `schema_registry`, `schema_check`, `enum_names`, `schema_fingerprint`,
  `schema_write`, `data_validate`, `path_hash`) — test: `test_schema_read.cpp` (round trip with `schema_write`: same
  fingerprint as the compiled tests' schema) — covers FR-3
- [ ] **T5.2** document × schema: header paths, fields, kinds, ranges, strings, enums, arrays, lists, nested,
  required, references collected with lines; suggestions; diagnostics (line order, 20 cap, shown path) — files:
  `data_convert.{hpp,cpp}`, `data_tables.cpp`, `data_values.cpp`, `suggest.*`, `diagnostics.*` — test:
  `test_data_convert.cpp`, `test_data_convert_tables.cpp`, `test_suggest.cpp`, `test_diagnostics.cpp` — covers
  FR-6, FR-7, FR-9, FR-14
- [ ] **T5.3** writer (then `data_validate` on its own output) and references sidecar (lines, kinds, targets,
  converted size) — files: `data_writer.*`, `data_refs.*` — test: `test_data_writer.cpp` — covers FR-10
- [ ] **T5.4** CLI `data <in> <out> --schema --refs --show`: converts declared keys, copies other files and writes an
  empty sidecar; exit codes; version 0.2.0 — files: `cli.cpp`, `main.cpp` — test: `test_cli.cpp`, ctest
  `assetc_version` updated — covers FR-4, FR-9
- [ ] **T5.5** [P] fuzzing: whole conversion from memory against a fixed schema + `fuzz_assetc_data` — files:
  `fuzz_assetc.*`, `libfuzzer_assetc_data.cpp` — covers AC-5, X

## Stage 6 — `nnh-assetc data-index`: references, names, index

- [ ] **T6.1** index build: data names unique across types, references to existing data of the right type (with
  suggestion), media references to `.png` / `.wav`/`.ogg` assets of the right kind, hash collisions and hash 0
  (forced in tests through an injectable hash), counts and string caps, largest asset; errors grouped by file
  (first file, ≤ 20) — files: `data_index_build.{hpp,cpp}` — test: `test_data_index.cpp` — covers FR-4, FR-8, FR-9,
  FR-12, FR-14
- [ ] **T6.2** `NNHX` writer (then `index_validate`) and CLI `data-index` — files: `index_writer.cpp`, `cli.cpp` —
  test: `test_data_index.cpp`, `test_cli.cpp` — covers FR-12, FR-13
- [ ] **T6.3** fixtures and ctest `data_convert_accepts_good`, `data_convert_rejects_*`/`explains_*` — one bad file
  per rule of AC-3 (unknown field + suggestion, wrong kind, out of range, string too long, list too long, wrong array
  count, unknown enum + suggestion, missing required, key set twice, each unsupported TOML feature, missing reference,
  reference of the wrong type, texture to a non-texture, duplicate name, each limit + 1), each checking the exact
  message and line — files: `tests/data/data_assets/`, `tests/CMakeLists.txt` (a fixtures' schema description
  committed next to them, regenerated and compared by a ctest so it can't drift) — covers AC-3, FR-5–FR-9, FR-14

## Stage 7 — build integration

- [ ] **T7.1** host-compiler detection shared (`_nnh_host_cxx()`), no behaviour change for `nnh-assetc` — files:
  `cmake/nnh_host_tools.cmake` — test: 3DS configure messages unchanged; `platformer` 3DS build — covers III
- [ ] **T7.2** the host program: `tools/data_schema/` (standalone project + `schema_main.cpp`: check, write only if
  changed, exit codes) and `nnh_data_schema_support` for PC presets — files: `tools/data_schema/*`,
  `engine/core/CMakeLists.txt`, `CMakeLists.txt` — test: ctest running the tests' schema program (exact
  description) and a two-same-key program (message names both sources) — covers FR-1, FR-3
- [ ] **T7.3** `nnh_add_data_schema()` and `nnh_add_game(... DATA_SCHEMA ...)`: OBJECT library (PC: shared with the
  host program; 3DS: ExternalProject per the T0.2 decision), description step — files: `cmake/nnh_data.cmake`,
  `nnh_game.cmake` — test: a test game in `tests/data/data_assets/game/` builds on PC and `3ds-release` — covers FR-3,
  III
- [ ] **T7.4** data and index steps in `nnh_add_assets` (only with a schema), `.nnh_data` in the names and reserved
  in `nnh_asset_index.cmake` — files: `cmake/nnh_assets.cmake`, `nnh_asset_index.cmake` — test: ctest
  `asset_index_*` updated; `asset_index_rejects_reserved_data` — covers FR-4, FR-9, FR-15
- [ ] **T7.5** code vs file: the tests' fixtures converted at build time by the real pipeline (target
  `nnh_test_data`), loaded in `nnh_tests` and compared with the expected structs — files:
  `tests/data_assets/test_round_trip.cpp`, `tests/CMakeLists.txt` — covers FR-3, FR-10, FR-11, AC-4
- [ ] **T7.6** ctest `data_build_fails_with_file_line`: building a test game's `_assets` target with a broken data
  file fails with `assets/…:<line>: <reason>` — files: `tests/CMakeLists.txt`, `tests/data/data_assets/broken_game/`
  — covers AC-3, FR-9
- [ ] **T7.7** incrementality (`ninja -d explain`, PC and 3DS): a data edit reruns only its step, the index, the asset
  index (+ repack on 3DS); a schema comment edit reconverts nothing (restat); a schema change reconverts the data;
  a header included by the schema source rebuilds the host program on 3DS — test: written in this file — covers III,
  AC-2
- [ ] **T7.8** opt-in and budgets: `hello` same steps and sizes as T0.1 (PC and 3DS); `platformer` unchanged; 3DS
  with `-DNNH_ASSETC=<PC build's nnh-assetc>`; edit `hello/main.cpp` → simulator < 2 s, `.3dsx` < 5 s; clean engine
  < 30 s — test: numbers written in this file — covers AC-6, FR-15, III

## Stage 8 — the example and the console check

- [ ] **T8.1** art, sounds and data files — files: `examples/data/art/make_art.py`, `examples/data/assets/**` —
  test: the build converts them — covers AC-1
- [ ] **T8.2** the bestiary/shop: types, scenes, listing, following references, hurt sound, settings — files:
  `examples/data/{CMakeLists.txt,main.cpp,data_types.hpp,data_types.cpp,bestiary_scene.*}`, `examples/CMakeLists.txt`
  — test: PC screenshot read back (`NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/data.png`) — covers AC-1, FR-8,
  FR-11, FR-12
- [ ] **T8.3** self-check mode and AC-2: `check.cpp` (PASS/FAIL line, banner, field-by-field checksum, allocation
  check, error cases); edit `enemies/goblin.enemy` hp → build log has only data/index/asset steps (PC and 3DS),
  simulator < 2 s, screenshot shows the new value; numbers into `docs/ROADMAP.md`'s build table; leak report clean
  — files: `examples/data/check.cpp`, `examples/data/tests/check.nnhs` — test: `nnh run data --pc --script
  examples/data/tests/check.nnhs` → `nnh: PASS` — covers AC-1, AC-2, AC-7, FR-13
- [ ] **T8.4** **3DS** (maintainer presses Y only): Homebrew Launcher open, then on the PC `nnh run data --script
  examples/data/tests/check.nnhs` (builds `3ds-dev`) → `nnh: PASS`, the same checksum as the simulator, screenshots
  saved in `build/logs/data-<stamp>/` — covers AC-1, FR-16

## Stage 9 — verification and docs

- [ ] **T9.1** Linux on VM 120: gcc build + ctest; clang ASan/UBSan for `nnh_tests` and `nnh_assetc_tests`;
  `fuzz_data`, `fuzz_data_index`, `fuzz_assetc_toml`, `fuzz_assetc_data` 60 s each without findings; `3ds-release`
  with the host-built programs — covers AC-5, FR-16
- [ ] **T9.2** `docs/api/data.md` (declaring a type, every kind and file form, references, loading, listing, errors,
  limits, memory formula with the example's measured total, the schema-source rule, editor TOML highlighting for the
  extensions) + header comments; format section in `docs/api/assets.md` — covers AC-8, VIII
- [ ] **T9.3** [P] `docs/tech/cmake.md` (`DATA_SCHEMA`, the host program in cross presets, data/index steps),
  `docs/architecture/{project-structure,security,README}.md` (module, `tools/data_schema`, new parsers), D29 marked
  implemented in `decisions.md` — covers AC-8
- [ ] **T9.4** [P] `CLAUDE.md` (module, layout, build), `README.md` status, `docs/REFERENCES.md` (Unity
  ScriptableObjects, Godot Resources, TOML 1.0) — covers AC-8
- [ ] **T9.5** `docs/ROADMAP.md` "Data assets" checkbox and build numbers; spec, plan, tasks status → implemented;
  `specs/README.md` — covers AC-8
