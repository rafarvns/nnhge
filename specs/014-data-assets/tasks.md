# Tasks 014: Data assets

Status: implemented (T8.4 passed on the Old 3DS XL and T9.1 on VM 120, 2026-11-22) — approved 2026-11-22 (maintainer pre-approved)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops. `[P]` = can be done in parallel with the previous task. Every stage ends with
something verifiable on PC (`ctest --preset pc-debug`, a converter run, a screenshot); the only console step is
T8.4, a scripted check where the maintainer only presses Y. Deviations found while building go into plan.md as
"Stage N refinements", as in plan 012.

## Stage 0 — baseline and toolchain probe

- [x] **T0.1** baseline numbers (Windows; 3 runs each): edit `examples/hello/main.cpp` → simulator and `.3dsx`
  build times; clean engine build; `hello.exe` and `hello.3dsx` sizes and their Ninja step lists
  (`ninja -t commands hello`); `platformer` builds clean — files: this file (result) — covers III, AC-6

  **Result (2026-11-22, commit `7aebf9a`, maintainer's PC: WinLibs GCC 16.2 + ccache with every edit a miss /
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

- [x] **T3.1** `core/path_hash` (FNV-1a; vectors) and the `NNHX` validator + lookups (by group and hash, by type
  and position) — files: `engine/core/src/core/path_hash.{hpp,cpp}`, `data/index_validate.{hpp,cpp}`,
  `data_file.*` (index writer) — test: `tests/core/test_path_hash.cpp`, `test_index_validate.cpp` — covers FR-12,
  FR-14
- [x] **T3.2** [P] fuzz target `data_index_target` — files: `tests/fuzz/fuzz_data.cpp`,
  `libfuzzer_data_index.cpp`, `fuzz_targets.*` — test: mini-fuzz — covers AC-5, X
- [x] **T3.3** shutdown hooks in core; index load/release with the scratch (tag `data`), missing-type warning;
  `kMaxAssetIndexBytes` → 256 KiB — files: `core/shutdown_hooks.{hpp,cpp}`, `app/app.cpp`,
  `data/data_index.{hpp,cpp}`, `assets/assets_internal.hpp` — test: `test_data_index.cpp` (`tests/data_assets/`),
  `tests/app/` hook test — covers FR-13
- [x] **T3.4** `load<T>` by path and by reference: errors, `out` unchanged, in-place `T{}`, stale fingerprint
  message — files: `data/data_load.cpp`, `data.hpp` — test: `test_data_load.cpp` — covers FR-11, FR-8
- [x] **T3.5** `count`/`name`/`ref`/`find`/`path`/`preload`/`release`/`memory_used` — files: `data/data_list.cpp`
  — test: `test_data_list.cpp`, `test_data_alloc.cpp` (formula = tag stats; no allocation over 100 calls after
  `preload`; shutdown frees all) — covers FR-12, FR-13, AC-7

  **Result (2026-11-22):** `tests/core/test_name_hash.cpp` 4 (plan 016's T0.3 file), `tests/app/test_shutdown_hooks.cpp`
  5 (Debug/Development), `tests/data_assets/test_index_validate.cpp` 15, `test_index_lookup.cpp` 7,
  `test_data_index.cpp` 11, `test_data_load.cpp` 13, `test_data_list.cpp` 8, `test_data_alloc.cpp` 3, mini-fuzz
  `data_index_target_survives_mutated_inputs` (6 000 mutants of a two-type index with every group; libFuzzer
  `fuzz_data_index` wired, not run: T9.1); `tests/framework/alloc_counter.*` created (shared with plans 015 and 016).
  `nnh_tests` 1184/1184 (Debug), 1146/1146 (Release). `hello` links no data code: Release `hello.exe` 276,599 bytes
  and `hello.3dsx` 261,284, both equal to T0.1, and `nm` finds no data or shutdown-hook symbol in either — which
  needed the shutdown hooks to run in Debug/Development only (a plain call cost 1,027 / 124 bytes). Refinements:
  plan.md "Stage 3 refinements".

  Stage end: ctest green; `hello` links no `nnh::data` symbol (`nm`) — the opt-in check starts early.

## Stage 4 — the TOML-subset reader (`nnh-assetc`)

- [x] **T4.1** document model and lexer/parser: lines, comments, keys, headers `[a.b]`/`[[a.b]]`, key or table set
  twice, BOM, CRLF/lone CR, UTF-8, file and line limits, errors with lines — files:
  `tools/assetc/src/toml.{hpp,cpp}`, `toml_parse.cpp` — test: `tools/assetc/tests/test_toml.cpp`,
  `test_toml_tables.cpp` — covers FR-5, FR-6, FR-14
- [x] **T4.2** values: bools, integers (`_`, sign, leading zeros), floats via `from_chars` (hard decimal cases),
  basic strings and escapes, inline arrays (multi-line, trailing comma, depth 2); each refused TOML feature with
  its message — files: `toml_value.{hpp,cpp}` — test: `test_toml_values.cpp` — covers FR-5, FR-16
- [x] **T4.3** [P] fuzzing: mini-fuzz of the reader + `fuzz_assetc_toml` — files: `tools/assetc/tests/fuzz_assetc.*`,
  `libfuzzer_assetc_toml.cpp`, `tools/assetc/CMakeLists.txt` — test: `test_fuzz_assetc.cpp` — covers AC-5, X

  **Result (2026-11-22):** `tools/assetc/tests/test_toml.cpp` 11, `test_toml_tables.cpp` 7, `test_toml_values.cpp` 16
  (hard decimal cases pinned bit for bit), mini-fuzz `assetc_toml_target_*` 2 (8 000 mutated documents, > 200
  accepted and walked); libFuzzer `fuzz_assetc_toml` wired, not run (T9.1). Refinements: plan.md "Stage 4
  refinements".

## Stage 5 — `nnh-assetc data`: one file against the schema

- [x] **T5.1** description reader → registry nodes (bounded; version) — files: `tools/assetc/src/schema_read.{hpp,cpp}`
  (the library also compiles core's `schema_registry`, `schema_check`, `enum_names`, `schema_fingerprint`,
  `schema_write`, `data_validate`, `path_hash`) — test: `test_schema_read.cpp` (round trip with `schema_write`: same
  fingerprint as the compiled tests' schema) — covers FR-3
- [x] **T5.2** document × schema: header paths, fields, kinds, ranges, strings, enums, arrays, lists, nested,
  required, references collected with lines; suggestions; diagnostics (line order, 20 cap, shown path) — files:
  `data_convert.{hpp,cpp}`, `data_tables.cpp`, `data_values.cpp`, `suggest.*`, `diagnostics.*` — test:
  `test_data_convert.cpp`, `test_data_convert_tables.cpp`, `test_suggest.cpp`, `test_diagnostics.cpp` — covers
  FR-6, FR-7, FR-9, FR-14
- [x] **T5.3** writer (then `data_validate` on its own output) and references sidecar (lines, kinds, targets,
  converted size) — files: `data_writer.*`, `data_refs.*` — test: `test_data_writer.cpp` — covers FR-10
- [x] **T5.4** CLI `data <in> <out> --schema --refs --show`: converts declared keys, copies other files and writes an
  empty sidecar; exit codes; version 0.2.0 — files: `cli.cpp`, `main.cpp` — test: `test_cli.cpp`, ctest
  `assetc_version` updated — covers FR-4, FR-9
- [x] **T5.5** [P] fuzzing: whole conversion from memory against a fixed schema + `fuzz_assetc_data` — files:
  `fuzz_assetc.*`, `libfuzzer_assetc_data.cpp` — covers AC-5, X

  **Result (2026-11-22):** `test_schema_read.cpp` 10 (the engine tests' schema round trip: same fingerprints and the
  same text), `test_suggest.cpp` 4, `test_diagnostics.cpp` 4, `test_data_convert.cpp` 15 and
  `test_data_convert_tables.cpp` 8 (every converted file passes the engine's validator and loads into the compiled
  struct), `test_data_writer.cpp` 5, `test_cli_data.cpp` 7, mini-fuzz `assetc_data_target_*` 2 (6 000 mutants, > 300
  accepted and loaded; mutated schema descriptions too); libFuzzer `fuzz_assetc_data` wired, not run (T9.1).
  `nnh_assetc_tests` 192/192; ctest `assetc_version` expects 0.2.0. ctest 176/176; `pc-debug` (clean), `pc-release`
  (clean), `3ds-dev` and `3ds-release` (clean) without warnings. Hand mutation testing of the index validator and lookups, the index, `load` and listing, shutdown hooks, the hash, the TOML reader, the schema reader and the converter (values and tables, writer, suggestions, diagnostics, CLI): 196 mutants, **190 killed** (2 by a compile error: the hash's `static_assert` test); the first run left 29 survivors, and tests were added for the 23 real ones (exactly 4096 data assets, sections a few bytes short, an empty key, types that stop short, a path offset just past the strings, exact reasons for order, lookup range, flags and string limits, an index shorter than its header, two names with one hash (load and find), a lone CR at the end, UTF-8 length boundaries of escapes, a bad fingerprint token, a long field name, a colour without `#`, a max-only bound, a suggestion just out of reach, the old sidecar removed on failure, the line of a section implied before it is defined); the 6 left are equivalent (the media-entry guard of `type_of`, the index's own init check behind `load`'s, the top byte of a scratch size the validator refuses anyway, `id == 0` before a lookup that never finds 0, the converter's depth guard behind the schema check, a dot in a folder name that can't form a key). Refinements: plan.md "Stage 5
  refinements".

## Stage 6 — `nnh-assetc data-index`: references, names, index

- [x] **T6.1** index build: data names unique across types, references to existing data of the right type (with
  suggestion), media references to `.png` / `.wav`/`.ogg` assets of the right kind, hash collisions and hash 0
  (forced in tests through an injectable hash), counts and string caps, largest asset; errors grouped by file
  (first file, ≤ 20) — files: `data_index_build.{hpp,cpp}` — test: `test_data_index.cpp` — covers FR-4, FR-8, FR-9,
  FR-12, FR-14
- [x] **T6.2** `NNHX` writer (then `index_validate`) and CLI `data-index` — files: `index_writer.cpp`, `cli.cpp` —
  test: `test_data_index.cpp`, `test_cli.cpp` — covers FR-12, FR-13
- [x] **T6.3** fixtures and ctest `data_convert_accepts_good`, `data_convert_rejects_*`/`explains_*` — one bad file
  per rule of AC-3 (unknown field + suggestion, wrong kind, out of range, string too long, list too long, wrong array
  count, unknown enum + suggestion, missing required, key set twice, each unsupported TOML feature, missing reference,
  reference of the wrong type, texture to a non-texture, duplicate name, each limit + 1), each checking the exact
  message and line — files: `tests/data/data_assets/`, `tests/CMakeLists.txt` (a fixtures' schema description
  committed next to them, regenerated and compared by a ctest so it can't drift) — covers AC-3, FR-5–FR-9, FR-14

  **Result (2026-11-22):** `tools/assetc/tests/test_data_index.cpp` 26 (names, every reference rule with its exact
  message, suggestions, media kinds incl. `.ogg`, first file only and the 20 cap, forced hash collisions and hash 0
  in both groups, 4096/4097 data, 4097 media, 256 KiB of paths, the written file through the engine's validator and
  lookups, 60 entries sorted), `test_data_refs.cpp` 3 (the sidecar reader: round trip, a copied file's sidecar, every
  malformed record), `test_cli_data_index.cpp` 9 (the command, relative sidecar paths, CRLF and blank lines in the
  lists, rewrite only on change — the `data` command too —, problems remove the index, bad sidecar/list/schema, usage
  errors), mini-fuzz `assetc_refs_target_survives_mutated_sidecars` (6 000 mutants, > 300 accepted; it found a real
  bug: a NUL inside a path passed the path check — fixed) and libFuzzer `fuzz_assetc_refs`. Fixtures in
  `tests/data/data_assets/`: 30 committed bad files + 2 generated (file > 64 KiB, line > 1024), each with its exact
  expected output in `# expect:` lines (ctests `data_convert_rejects_*`), 6 cross-file cases (`data_index_rejects_*`:
  duplicate name, missing reference with suggestion, reference of the wrong type, texture to a sound, sound to an
  image, missing texture with suggestion), `data_convert_accepts_good` (the good game through `data` + `data-index`).
  `nnh_assetc_tests` 232/232. Refinements: plan.md "Stages 6–9 refinements".

## Stage 7 — build integration

- [x] **T7.1** host-compiler detection shared (`_nnh_host_cxx()`), no behaviour change for `nnh-assetc` — files:
  `cmake/nnh_host_tools.cmake` — test: 3DS configure messages unchanged; `platformer` 3DS build — covers III
- [x] **T7.2** the host program: `tools/data_schema/` (standalone project + `schema_main.cpp`: check, write only if
  changed, exit codes) and `nnh_data_schema_support` for PC presets — files: `tools/data_schema/*`,
  `engine/core/CMakeLists.txt`, `CMakeLists.txt` — test: ctest running the tests' schema program (exact
  description) and a two-same-key program (message names both sources) — covers FR-1, FR-3
- [x] **T7.3** `nnh_add_data_schema()` and `nnh_add_game(... DATA_SCHEMA ...)`: OBJECT library (PC: shared with the
  host program; 3DS: ExternalProject per the T0.2 decision), description step — files: `cmake/nnh_data.cmake`,
  `nnh_game.cmake` — test: a test game in `tests/data/data_assets/game/` builds on PC and `3ds-release` — covers FR-3,
  III
- [x] **T7.4** data and index steps in `nnh_add_assets` (only with a schema), `.nnh_data` in the names and reserved
  in `nnh_asset_index.cmake` — files: `cmake/nnh_assets.cmake`, `nnh_asset_index.cmake` — test: ctest
  `asset_index_*` updated; `asset_index_rejects_reserved_data` — covers FR-4, FR-9, FR-15
- [x] **T7.5** code vs file: the tests' fixtures converted at build time by the real pipeline (target
  `nnh_test_data`), loaded in `nnh_tests` and compared with the expected structs — files:
  `tests/data_assets/test_round_trip.cpp`, `tests/CMakeLists.txt` — covers FR-3, FR-10, FR-11, AC-4
- [x] **T7.6** ctest `data_build_fails_with_file_line`: building a test game's `_assets` target with a broken data
  file fails with `assets/…:<line>: <reason>` — files: `tests/CMakeLists.txt`, `tests/data/data_assets/broken_game/`
  — covers AC-3, FR-9
- [x] **T7.7** incrementality (`ninja -d explain`, PC and 3DS): a data edit reruns only its step, the index, the asset
  index (+ repack on 3DS); a schema comment edit reconverts nothing (restat); a schema change reconverts the data;
  a header included by the schema source rebuilds the host program on 3DS — test: written in this file — covers III,
  AC-2
- [x] **T7.8** opt-in and budgets: `hello` same steps and sizes as T0.1 (PC and 3DS); `platformer` unchanged; 3DS
  with `-DNNH_ASSETC=<PC build's nnh-assetc>`; edit `hello/main.cpp` → simulator < 2 s, `.3dsx` < 5 s; clean engine
  < 30 s — test: numbers written in this file — covers AC-6, FR-15, III

  **Result (2026-11-22):** T7.1–T7.6 as planned with the refinements in plan.md (the tests' schema program
  `nnh_tests_data_schema`; ctests `data_schema_program_writes_the_fixtures_schema` — exact committed description and
  no rewrite of an unchanged one —, `data_schema_program_{refuses,names_both_sources_of}_a_key_used_twice`,
  `asset_index_accepts_generated_data`, `asset_index_{rejects,explains}_reserved_data`,
  `data_build_{fails,explains}_with_file_line` (`assets/enemies/goblin.enemy:3: unknown field 'lvel' (did you mean
  'level'?)`); `test_round_trip.cpp` 4 tests: every kind, defaults, a CRLF file, `\u` escapes, `_` separators,
  references, listing and a copied non-data file, all through the real pipeline). The 3DS side of T7.3 is
  `examples/data` (3ds-release and 3ds-dev build clean; with a prebuilt `-DNNH_ASSETC` too).

  **T7.7** (`ninja` step lists, `examples/data`, PC `pc-debug` / 3DS `3ds-release`):

  | Edit | PC steps (time) | 3DS steps (time) |
  |------|-----------------|------------------|
  | none | nothing (0.53 s) | `nnh-assetc`'s up-to-date check only (3.0 s); the schema program doesn't run |
  | `goblin.enemy` hp 30 → 31 | its data step, the asset index (0.82 s) | + the `.3dsx` repack (3.68 s) |
  | `goblin.enemy` comment only | its data step only (0.65 s): output unchanged, restat | the same (3.21 s) |
  | `data_types.cpp` comment | schema object, program link, description (unchanged), game link (2.72 s) | console object, stamp, host program rebuild, description, link, repack (6.57 s) |
  | `data_types.cpp` new `.min` | as above + all 8 data steps, index, asset index (3.42 s) | the same + repack (7.78 s) |
  | `data_types.hpp` comment | schema object + the game's 3 sources that include it (2.88 s) | + the host program (8.48 s) |

  The value edit doesn't rerun the index step (its sidecar is unchanged); no C++ compile or link for any data edit
  (AC-2); the screenshot after the hp edit shows `HP 31`.

  **T7.8:** hello's own Ninja steps equal T0.1's in `pc-release` and `3ds-release` (only `nnh_core`'s source list grew:
  90 → 121, stages 2–3 and spec 015; stages 6–9 add none). Release `hello.exe` 277,503 and `hello.3dsx` 261,464 bytes:
  larger than T0.1 because of spec 015's merge — a build of the previous commit has the same `hello.elf` data and bss
  and text 64 bytes larger only from its longer source path —, no data symbol (`nm`). `platformer` builds unchanged.
  Edit `hello/main.cpp` → simulator 1.77 / 1.65 / 1.63 s; → `.3dsx` 2.86 / 3.09 / 3.12 s (runs of 5–8 s were load
  from another agent building on the machine; the steps are hello's three). Clean 3DS engine 13.9 s (145 steps), no
  warnings. 3DS with `-DNNH_ASSETC=build/pc-debug/tools/assetc/nnh-assetc.exe`: `data.3dsx` builds (the schema program
  is still host-built: it holds the game's declarations).

## Stage 8 — the example and the console check

- [x] **T8.1** art, sounds and data files — files: `examples/data/art/make_art.py`, `examples/data/assets/**` —
  test: the build converts them — covers AC-1
- [x] **T8.2** the bestiary/shop: types, scenes, listing, following references, hurt sound, settings — files:
  `examples/data/{CMakeLists.txt,main.cpp,data_types.hpp,data_types.cpp,bestiary_scene.*}`, `examples/CMakeLists.txt`
  — test: PC screenshot read back (`NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/data.png`) — covers AC-1, FR-8,
  FR-11, FR-12
- [x] **T8.3** self-check mode and AC-2: `check.cpp` (PASS/FAIL line, banner, field-by-field checksum, allocation
  check, error cases); edit `enemies/goblin.enemy` hp → build log has only data/index/asset steps (PC and 3DS),
  simulator < 2 s, screenshot shows the new value; numbers into `docs/ROADMAP.md`'s build table; leak report clean
  — files: `examples/data/check.cpp`, `examples/data/tests/check.nnhs` — test: `nnh run data --pc --script
  examples/data/tests/check.nnhs` → `nnh: PASS` — covers AC-1, AC-2, AC-7, FR-13
- [x] **T8.4** **3DS** (maintainer presses Y only): Homebrew Launcher open, then on the PC `nnh run data --script
  examples/data/tests/check.nnhs` (builds `3ds-dev`) → `nnh: PASS`, the same checksum as the simulator, screenshots
  saved in `build/logs/data-<stamp>/` — covers AC-1, FR-16

  **Result (2026-11-22, Old 3DS XL, Development):** `data check: PASS checksum 0xD5BD3020 (4 enemies, 3 items, 1
  settings, index 595 bytes)`, the simulator's checksum; the two provoked warnings; the goblin/yeti/check screenshots
  as on PC. The first run stopped at "the game didn't answer": the example loads its media before frame 1 (2.2 s) and
  `nnh` waited only 3 s for the `Hello` answer — now 10 s, restarted by each log line (commit a5111e0).

  **Result (2026-11-22, T8.1–T8.3):** `examples/data` (4 enemies, 3 items, `game.settings`; art and sounds by
  `art/make_art.py`). Screenshots read back: the bestiary (goblin, tinted from its data file, stats, drops with
  icons), the yeti and its drops (the ice crystal "summons enemies/yeti": the reference cycle), the PASS banners.
  Self-check line: `data check: PASS checksum 0xD5BD3020 (4 enemies, 3 items, 1 settings, index 595 bytes)`; ctest
  `data_self_check_pc`; `NNH_PRESET=pc-debug nnh run data --pc --no-build --script examples/data/tests/check.nnhs` →
  `nnh: PASS (26 lines, frames 1-61)`. Leak report clean after quitting (only the two warnings the check provokes on
  purpose). `check_release_strings.sh` on the Release `data.elf`: ok. **T8.4 is the maintainer's** (command and
  expected lines in the report).

## Stage 9 — verification and docs

- [x] **T9.1** Linux on VM 120: gcc build + ctest; clang ASan/UBSan for `nnh_tests` and `nnh_assetc_tests`;
  `fuzz_data`, `fuzz_data_index`, `fuzz_assetc_toml`, `fuzz_assetc_data` 60 s each without findings; `3ds-release`
  with the host-built programs — covers AC-5, FR-16

  **Result (2026-11-22, VM 120, commit b8a228c):** GCC 13 builds and passes ctest 229/229 (one `-Wformat-truncation`
  warning in `examples/data/check_scene.cpp`, fixed by a 128-byte buffer). clang 18 ASan/UBSan: `nnh_tests`
  1371/1371 and `nnh_assetc_tests` 233/233 with leak checks on; ctest green except `data_schema_explains_unknown_
  {member,enum_name}`, whose regexes knew only GCC's wording — they now also accept clang's ("no member named",
  "undeclared identifier"). libFuzzer 120 s each, no crash: `fuzz_data` cov 242, `fuzz_data_index` 145,
  `fuzz_assetc_toml` 926, `fuzz_assetc_data` 1948, `fuzz_assetc_refs` 2212, `fuzz_assetc_json` 433. The earlier
  stage 3–5 run gave the same picture (`fuzz_data` 257, `fuzz_data_index` 142, `fuzz_assetc_toml` 926,
  `fuzz_assetc_data` 2014). Full `ninja` in a `NNH_BUILD_FUZZERS` build fails to link the `nnh-assetc` program
  (instrumented library, uninstrumented link): build the fuzz targets by name.
- [x] **T9.2** `docs/api/data.md` (declaring a type, every kind and file form, references, loading, listing, errors,
  limits, memory formula with the example's measured total, the schema-source rule, editor TOML highlighting for the
  extensions) + header comments; format section in `docs/api/assets.md` — covers AC-8, VIII
- [x] **T9.3** [P] `docs/tech/cmake.md` (`DATA_SCHEMA`, the host program in cross presets, data/index steps),
  `docs/architecture/{project-structure,security,README}.md` (module, `tools/data_schema`, new parsers), D29 marked
  implemented in `decisions.md` — covers AC-8
- [x] **T9.4** [P] `CLAUDE.md` (module, layout, build), `README.md` status, `docs/REFERENCES.md` (Unity
  ScriptableObjects, Godot Resources, TOML 1.0) — covers AC-8
- [x] **T9.5** `docs/ROADMAP.md` "Data assets" checkbox and build numbers; spec, plan, tasks status → implemented;
  `specs/README.md` — covers AC-8

  **Result (2026-11-22):** T9.2–T9.5 done (`docs/api/data.md` rewritten as the full reference, data section in
  `assets.md`, "Data assets (DATA_SCHEMA)" in `cmake.md`, D29 implemented in `decisions.md`, security,
  project-structure, REFERENCES, CLAUDE.md, README, ROADMAP item ticked "console check pending" with a build table;
  `docs_tool.py check` 0 broken links). Hand mutation testing of stages 6–9 (the index build and writer, the sidecar
  reader, both CLI commands, write-if-changed, the schema program, the reserved keys): 74 mutants, 72 valid (2
  snippets didn't match), **68 killed**. The first pass killed 58; of the 14 survivors, 10 were real gaps and got
  tests (a 5-character `x.png`, a reference to a type the schema lacks, non-data sidecars passed in, a lookup that
  was sorted only by chance in a small index, a leading zero and an empty token in a sidecar, CRLF and blank lines in
  the lists, a longer old file that starts like the new one, the schema program's usage line); 4 are equivalent (an
  empty diagnostics entry that `add` never creates, which of two duplicate names wins once the duplicate is already
  an error, the writer's self-check that a correct writer can't fail, the CLI filtering non-data sidecars that
  `build_index` filters again). Also: the compile-fail ctests' regexes `damgae[^ ]* is not a member` and
  `Fier[^ ]* was not declared` match GCC's quotes in any locale (Linux UTF-8 uses ‘ ’). T9.1 ran afterwards (above).
