# Tasks 027: Save data with versions

Status: approved 2026-11-24 (overnight pre-approval)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task is one or a few TDD loops (test first, then code). `[P]` means the task can be done in parallel with the
previous one. Every stage ends with something verifiable on PC (`ctest --preset pc-debug`, a screenshot, a `--pc`
script). The work happens in a **separate git worktree** (branch `spec-027`) created from `main`, merged **after specs
022, 023, 024 and 025**; follow plan.md's "Merge notes" for every shared file (insertions at the stated anchors only;
spec 014's data module is **never** edited; spec 002's `save.cpp` only gets an appended function). The wire primitives
and the schema-free validator come first, then the declarations and their compile-time checks, the encoder, the
decoder with evolution and migrations, the file layer with backups and power cuts, the golden files and fuzzing, the
example with the ARM pre-check, mutation testing, Linux (stage 9, by the lead), the console (stage 10: three scripted
runs the maintainer only starts) and the docs. Deviations found while building go into plan.md as "Stage N
refinements", as in plans 012–026.

Guards held at every stage end (the "stage-end guards"): ctest pc-debug and pc-release; 3ds-release without a warning;
`hello` sizes and `nm` equal T0.1's; spec 002's save tests and spec 017's bindings tests unchanged and passing;
`test_raw_unchanged.cpp` (from T1.3: the pinned raw `NNHS` and `NNHB` bytes); `data check: PASS checksum 0xD5BD3020`,
`actions: PASS 7/7` (`--pc`), `camera: PASS 5201E6CF`, `queries: PASS 4ACAA177`, `entities: PASS C8FB9A41`, `prefabs:
PASS 7BB7901E`, the hierarchy golden, `tweens: PASS B44CA3F6` (their ctests); `git diff --exit-code engine/core/include/nnh/data.hpp
engine/core/include/nnh/detail/data_* engine/core/src/data tools/assetc`.

## Stage 0: baseline

- [x] **T0.1** Baseline numbers (Windows, 3 runs each; scripts in `build/ref-027/`, not committed): `hello.exe`
  (pc-release, pc-debug) and `hello.elf`/`.3dsx` (3ds-release, 3ds-dev) sizes (`size`: text/data/bss, file bytes) and
  `nm -C` lists (does `hello` link `save.o`? does the 3DS link use `--gc-sections`? — decides whether the appended
  `save_read_one` can reach `hello`); `.text` of the 3ds-release `actions`, `data`, `foundation` ELFs; edit
  `examples/data/main.cpp` → simulator and `.3dsx` times (the reference for the saves example's edit); clean `nnh_core`;
  preprocessed lines and `-fsyntax-only` time of one-line TUs including `<nnh/data.hpp>` and `<nnh/fs.hpp>`; 3ds-dev
  disassembly of `save.o` and `save_format.o` saved in `build/ref-027/dis0/`. Files: plan.md ("Stage 0 results").
  Covers III, FR-20, AC-7.
- [x] **T0.2** [P] Pins: the bytes `nnh::save::write` produces for a fixed 100-byte blob with version 7 on the fake (CRC-32
  of the whole file and its size), the `NNHB` blob of the actions example's fixed set (`save_to`), the fake's fs call
  sequence of one `save::write` over an existing file and of one `save::read` (counts per function), `link.sav`'s format
  constant. Files: plan.md ("Stage 0 results"). Covers AC-7, FR-6.
- [x] **T0.3** [P] Goldens and counts today: `0xD5BD3020`, `actions: PASS 7/7` and its `saved 40 bytes sum` line,
  `5201E6CF`, `4ACAA177`, `C8FB9A41`, `7BB7901E`, the hierarchy golden, `B44CA3F6`; `nnh_tests` count against
  `kMaxTests` (4096; this spec adds ≈ 250 tests: raise only if fewer than 400 are left, and note it for the merge).
  Files: plan.md. Covers VI.

## Stage 1: wire primitives, the validator, spec 002's accessor, fake power cuts (pure)

- [x] **T1.1** Wire primitives: `src/saves/save_wire.hpp` (wire kinds and shapes, the wire byte, canonical varint read
  and write ≤ 3 bytes, widths per kind, `static_assert` little-endian), `save_header.{hpp,cpp}` (build and parse the
  structured header: magic `NNSV`, format 1, key length and characters, flags 0, version ≥ 1, FNV-1a key hash). Test:
  `tests/saves/test_wire.cpp` (varints 0, 127, 128, 16 383, 16 384, 2²¹ − 1 against hand bytes; overlong and 4-byte
  forms refused; every wire byte valid or not), `tests/saves/test_header.cpp` (hand-written headers accepted; each field
  wrong refused with its message; format 2 → `Unsupported`; a raw payload recognised). Files: those,
  `engine/core/CMakeLists.txt` (the `_nnh_saves_sources` block after the prefabs block's `target_sources` line),
  `tests/CMakeLists.txt` (027's block after the `queries_example` line). Covers FR-5, FR-6, FR-14.
- [x] **T1.2** The schema-free validator: `save_validate.{hpp,cpp}` (the bounded walk of plan.md: ids ascending, depth
  ≤ 8, lengths/counts before use, struct and `bytes` exact fills, templates 1..128 ascending, strings through
  `detail::utf8_valid` and no NUL; reasons with the field path; a step counter for the linear-time test). Test:
  `tests/saves/test_validate.cpp` (every bound both ways with hand-written payloads; messages
  `field 7: list count 900 exceeds the file`, `field 9[12].4: string is not UTF-8`; a 256 KiB adversarial file of
  nested sequences validated within a fixed step count; unknown fields of every kind skipped by length). Covers FR-5,
  FR-14, X.
- [x] **T1.3** Spec 002's accessor: `detail::save_read_one` appended after `} // namespace nnh::save` in
  `src/fs/save.cpp`, declared after `validate_save` in `save_format.hpp`. Test: `tests/saves/test_raw_unchanged.cpp`
  (T0.2's pins: raw file bytes, `NNHB` bytes, fs call sequences; `save_read_one` equals `read_one`'s behaviour: missing,
  short, bad CRC, too large); `tests/fs/test_save.cpp` unchanged and passing; 3ds-dev `save.o`'s existing functions
  byte-identical to T0.1's. Covers FR-7, FR-20, AC-7.
- [x] **T1.4** [P] Fake power cuts: `platform_fake.hpp` (`cut_power_after(ops, torn)`, `restore_power()`,
  `truncate_file(path, size)` after `corrupt_file`), `fs_fake.cpp` (the guard line in the four mutating functions, the
  state, the reset). Test: `tests/platform_fake/test_power_cut_fake.cpp` (cut after 0..4 operations: the cut write keeps
  half, a cut rename/remove does nothing, later mutating calls fail without effect, reads work, `restore_power()` and
  `reset()` end it; `fail_next_writes` unchanged). Covers AC-3.
- [x] **T1.5** Stage end: the stage-end guards. Files: plan.md ("Stage 1 results"). Covers AC-7.

## Stage 2: declarations and compile-time checks

- [x] **T2.1** The header and the schema records: `<nnh/saves.hpp>` (limits, `WireKind`, `WireShape`, `FieldPath`,
  `path()`, `Report`, `FileInfo`, `OldFields` and the template declarations), `nnh/detail/save_schema.hpp`
  (`SaveFieldInfo`, `SaveTypeInfo`, `SaveEntry`, `SaveStep`, `SaveTag`, `save_error_*`), `nnh/detail/save_kinds.hpp`
  (the wrapper over spec 014's traits: `double`, 64-bit integers, arrays ≤ 65 535, element checks with messages). Test:
  `tests/saves/test_declare.cpp` (part 1: kinds, widths, signs, shapes, capacities, enum and ref targets for every member
  type; `FieldPath` building and `format`). Covers FR-1, FR-3.
- [x] **T2.2** Tables and macros: `nnh/detail/save_tables.hpp` (`make_save_field` with the nested record found by ADL,
  `make_save_table` sorting fields by id, retired ids, steps, the complete-chain check, `make_save_type`, depth, root
  size, `max_size`), `nnh/detail/save_macros.hpp` (`NNH_SAVE_TYPE/STRUCT/SUMMARY/FIELD/RETIRED/VERSION/STEP`, names `""`
  under `NNH_FLAVOR_RELEASE`). Test: `tests/saves/test_declare.cpp` (part 2: the tables of a type, a struct, a summary,
  in the global namespace and in a game namespace, in a header and in one source; a struct that is also an
  `NNH_DATA_TYPE`; `max_size` of hand-computed cases; names empty in pc-release). Covers FR-1, FR-2, FR-8, FR-12.
- [x] **T2.3** Negative-compile ctests: `tests/saves/compile_fail/` (`good.cpp` with `-Werror`; one file per case of
  plan.md's AC-1 list: unknown member, pointer, `std::string`-like, union, `char`, `const`, id 0, id 65 536, duplicate
  id, retired used, retired twice, nested without declaration, 129 fields, 9 levels, root > 64 KiB, array of 65 536,
  max size > 256 KiB, bad keys, version 0, step at the current version, two steps for one version, a gap in the chain,
  `NNH_SAVE_VERSION` in a struct, a summary stored, an undeclared type stored). ctests `saves_declaration_rejects_<case>`
  (`WILL_FAIL`) and `saves_declaration_explains_<case>` (GCC's and clang's wording) in 027's block, under the
  data-schema block's condition. Covers FR-1, FR-2, FR-12, FR-21, AC-1.
- [x] **T2.4** Stage end: the stage-end guards; include check (`<nnh/saves.hpp>` pulls only `<nnh/data.hpp>`,
  `<nnh/result.hpp>` and the standard headers named; no gfx, audio, tilemap, entities header); preprocessed lines and
  `-fsyntax-only` within 0.05 s of `<nnh/data.hpp>` alone (else plan risk 6). Files: plan.md ("Stage 2 results").
  Covers III, FR-20.

## Stage 3: the encoder (pure)

- [ ] **T3.1** `save_encode.cpp` for one-value kinds and nested structs: every kind's bytes, ascending ids whatever the
  declaration order, bools as 0/1, strings at their length, non-UTF-8 → `InvalidArgument` (assert in Debug/Development),
  short buffer → `LimitReached` with nothing written past `capacity`; `encode<T>`. Test: `tests/saves/test_encode.cpp`
  (hand-written expected bytes per kind with a comment per field; 64-bit and `double` values; nested structs; overhead
  per field as plan.md states). Covers FR-3, FR-4, FR-5, FR-8, FR-21.
- [ ] **T3.2** Sequences: packed number arrays and lists (one copy when kinds match), string sequences with `bytes`, struct
  sequences with their template and rows, nested arrays of structs. Test: `tests/saves/test_encode_sequences.cpp` (hand
  bytes; an empty list still writes its template; `max_size<T>()` equals the encoded size of a value with every string
  and list full, for 10 shapes). Covers FR-3, FR-4, FR-8.
- [ ] **T3.3** Stage end: the stage-end guards. Files: plan.md ("Stage 3 results": the overhead table measured). Covers
  FR-5.

## Stage 4: the decoder, evolution, migrations (pure)

- [ ] **T4.1** Apply and decode: `save_apply.{hpp,cpp}` (reset `T{}`, merge-walk by id, sequences, rows in lockstep
  with the template, report counting and Debug/Development paths), `save_convert.{hpp,cpp}` (same-kind copies),
  `save_decode.cpp` (validate → header checks → reset → apply), `decode<T>`. Test: `tests/saves/test_round_trip.cpp`
  (every kind and shape round-trips; a canary-filled struct is unchanged after each error; no allocation). Covers FR-8,
  FR-9.
- [ ] **T4.2** Automatic evolution: `save_convert.cpp` (integer width and sign changes that fit, integer → float/double
  exact by bit length, float → double on bits, array ↔ list, capacities grown, enum checks through `find_enum` once per
  sequence field). Test: `tests/saves/test_evolution.cpp` (added, removed → unknown, retired → silent, renamed,
  reordered, widened, signedness, exact int → float, a subnormal float → double unchanged, array/list grown,
  string grown, nested struct evolution). Covers FR-2, FR-10, FR-18.
- [ ] **T4.3** Values that don't fit: Test: `tests/saves/test_fit.cpp` (out of range; inexact int → float at 2²⁴ + 1;
  inexact double → float; int → struct, struct → int, one ↔ array; NaN and ∞ in float, double, `Vec2`; enum outside
  `NNH_DATA_ENUM` names; bool bytes 2 and 0xFF; strings cut at 2- and 3-byte boundaries; list and array shrunk; array
  with fewer items not counted; drops per value; the counts saturate; the first three paths). Covers FR-11, FR-15.
- [ ] **T4.4** Versions and type checks: key mismatch → `InvalidArgument` naming both keys, newer version →
  `Unsupported` ("saved by version 5; this game reads up to 3"), raw `nnh::save` payload → its message, format 2 →
  `Unsupported`; the struct unchanged. Test: `tests/saves/test_versions.cpp`. Covers FR-6, FR-9, FR-14.
- [ ] **T4.5** Migrations and the old-fields view: `save_migrate.cpp` (steps v … c − 1 in order, the runner's assert),
  `old_fields.cpp` (`has`, `kind`, `shape`, `count`, `get` for every C++ kind and for save structs). Test:
  `tests/saves/test_migrations.cpp` (chains 1→3 and 2→3, call order, steps see the loaded struct, a current file runs
  none, a hand-built record with a null step asserts), `tests/saves/test_old_fields.cpp` (retired, unknown, dropped
  values; `4.2`, `9[3].2`; a region into a save struct; kinds that can't hold the value → false; the old-struct pattern).
  Covers FR-12, FR-13, FR-21.
- [ ] **T4.6** [P] Summaries: `NNH_SAVE_SUMMARY` loads (same key, a subset of ids, no steps, newer refused). Test:
  `tests/saves/test_summary.cpp`. Covers FR-16.
- [ ] **T4.7** Stage end: the stage-end guards. Files: plan.md ("Stage 4 results"). Covers AC-1.

## Stage 5: files, backups, power cuts, the dump

- [ ] **T5.1** Store and load: `save_file.cpp` (`store`/`load` and their `Buffer` overloads, the backup rules of D46,
  `Corrupted` with both reasons, `NotFound`), `save_report.cpp` (the warning line, the `saves` channel). Test:
  `tests/saves/test_file.cpp` (`NNHS` version 1 + structured header on disk; the fs call sequence equals T0.2's
  `save::write` sequence; one `fs_write` per store; the report; one warning line exactly when backup/dropped/adjusted;
  no line otherwise), `tests/saves/test_backup.cpp` (CRC-valid broken structure, bit flips, truncation, missing main →
  backup used; key mismatch and newer version never try the backup; both bad → `Corrupted`, struct unchanged). Covers
  FR-6, FR-7, FR-9, FR-15.
- [ ] **T5.2** Slots: `info`, `exists`, `remove`. Test: `tests/saves/test_slots.cpp` (file, backup only, none; remove of
  file + backup + temp; `info` of main, backup, raw file; three slots and a settings file side by side). Covers FR-16.
- [ ] **T5.3** Power cuts and damage: Test: `tests/saves/test_power_cut.cpp` (first store and store over a save; cut after
  0..4 operations, torn and not; another store after each cut; the next load gives the new or the previous values, never
  `Corrupted`, never `NotFound` once a save existed). Covers AC-3, X.
- [ ] **T5.4** The dump: `save_dump.cpp` (values, buffers, files; names when known, ids otherwise; Release stubs),
  `save_dump_env.cpp` (`NNH_SAVE_DUMP` once, the test override, `<path>.txt` in 4 KB pieces). Test:
  `tests/saves/test_dump.cpp` (exact text of a small value; nested indentation; list counts; escapes; unknown and
  retired fields; `<path>.txt` written with the override; `Unsupported` in pc-release). Covers FR-17.
- [ ] **T5.5** [P] Misuse, cost and blobs: Test: `tests/saves/test_save_misuse.cpp` (every FR-21 case), 
  `tests/saves/test_save_cost.cpp` (no allocation over every call; visited-field counters linear for 128-field structs and
  256-item lists), `tests/saves/test_blob.cpp` (an `ActionSet`'s `NNHB` in a `List<std::uint8_t, 256>` stored, loaded,
  applied with `load_from`). Covers FR-3, FR-8, FR-19, FR-21.
- [ ] **T5.6** Stage end: the stage-end guards. Files: plan.md ("Stage 5 results"). Covers AC-1, AC-3.

## Stage 6: the reference save, golden files, fuzz

- [ ] **T6.1** The reference types: `examples/saves/progress.hpp` (v3 types and declarations), `progress_history.hpp`
  (`ProgressV1`, `ProgressV2`), `progress_migrate.cpp` (steps 1 and 2), `progress_sum.{hpp,cpp}` (FNV-1a over values in id
  order), `reference.{hpp,cpp}` (seeded values for each version, a subnormal `wear`, a two-byte character in the name),
  `difficulty_names.cpp` (`NNH_DATA_ENUM`), compiled into `nnh_tests` from 027's block. Test:
  `tests/saves/test_reference.cpp` (sizes ≈ plan.md's; v1 → v3 by hand equals the migrated load). Covers AC-2.
- [ ] **T6.2** Golden files: `tests/saves/make_saves_golden.cpp` (`EXCLUDE_FROM_ALL`) writes `tests/data/saves/v{1,2,3}.sav`
  and `v{1,2,3}.txt`; `tests/data/saves/README.md` (how, and that a regeneration needs a reason in the commit); copies in
  `examples/saves/assets/golden/`; ctest `saves_golden_copies_match`. Test: `tests/saves/test_golden.cpp` (each file loads
  to its recorded sum and report; the reference encodes to `v3.sav`'s payload byte for byte; each dumps to its `.txt`).
  Files: plan.md ("Stage 6 results": the sums, CRCs, sizes). Covers AC-2, FR-4, FR-18.
- [ ] **T6.3** [P] Fuzz: `tests/fuzz/fuzz_saves.{hpp,cpp}` (a reference type with every kind and shape; inputs decoded as
  given and as an `NNHS` file with the CRC and size fixed, through `load` on the fake too; every accepted input
  re-encoded and decoded to the same values; `OldFields` on paths taken from the input; `dump_bytes` in Debug),
  `libfuzzer_saves.cpp`, `tests/fuzz/test_fuzz_saves.cpp` (deterministic mini-fuzz seeded with the three goldens and a
  seed of every kind). Files: `tests/CMakeLists.txt` (027's block: `target_sources(nnh_fuzz_targets …)`, its own
  `if(NNH_BUILD_FUZZERS)`). Covers AC-4, X.
- [ ] **T6.4** Stage end: the stage-end guards. Covers AC-2, AC-4.

## Stage 7: `examples/saves` (PC), the bench and the ARM pre-check

- [ ] **T7.1** The showcase: `tiny_game.{hpp,cpp}`, `slots_panel.{hpp,cpp}` (layout of plan.md, touch and buttons,
  summaries), `settings.{hpp,cpp}` (declarations and store in one source), `summary.hpp`, `hud.{hpp,cpp}` (the last
  report), `showcase_scene.{hpp,cpp}`, `main.cpp`, `demos.hpp`, `CMakeLists.txt` (`ASSETS assets`),
  `examples/CMakeLists.txt` (after `queries`). Test: PC screenshots `SAVES_SHOT=empty|saved|golden|backup` read back;
  `NNH_SAVE_DUMP=1` writes `slot1.sav.txt` (read back). Covers AC-5, FR-16, FR-17.
- [ ] **T7.2** Check scene: `check_scene.{hpp,cpp}`, `check_golden.hpp` (values from pc-debug), the lines of plan.md;
  ctest `saves_self_check_pc` (after the `queries_self_check_pc` block). Test: the ctest passes in pc-debug and
  pc-dev; one value changed by hand fails it (checked once). Covers AC-6, FR-18.
- [ ] **T7.3** [P] Bench: `bench.{hpp,cpp}` (the eight workloads of plan.md on the reference save), `bench_scene.{hpp,cpp}`
  (one SD operation per frame, the lines and the verdict; Debug `not judged (Debug)`). Test: a pc-dev run logs every
  line. Covers AC-8.
- [ ] **T7.4** The script `examples/saves/tests/check.nnhs` (plan.md "Stage 10": every expected line, a `wait 3` after
  every `expect` an input follows, the touch coordinates of the layout) and ctest `saves_check_script_pc`
  (`RESOURCE_LOCK nnh_remote_port`). Test: `nnh run saves --pc --no-build --script examples/saves/tests/check.nnhs` →
  PASS, twice in a row (repeatable with files left from the first run). Files: plan.md ("Stage 7 results": every
  expected line). Covers AC-5, AC-6.
- [ ] **T7.5** ARM11 pre-check (before any console run): `arm-none-eabi-objdump -drC` of the 3ds-dev objects
  `src/saves/*` and the example's `bench.o`; per AC-8 path record instructions, calls, `pld`, any VFP instruction
  (none expected in the codec), `__aeabi_*` (none), `memcpy` (packed copies only); 3ds-dev `-fstack-usage` of
  `src/saves/*` summed along the deepest validate/apply path at 8 levels (< 1 KiB besides the struct and buffer);
  compare with plan.md's table and fix what differs without changing a byte (goldens hold). Files: plan.md ("Stage 7 ARM
  pre-check": the table and the expected console numbers). Covers AC-8, II, FR-8.

## Stage 8: mutation testing, opt-in checks

- [ ] **T8.1** Mutation pass as in plans 019–026: hand mutants (`build/ref-027/mutants.py`, applied one at a time by
  `mutate.py`, the whole `nnh_tests` each) of `src/saves/*` (varint canonical check, id order, depth bound, length checks,
  `bytes` fill, UTF-8 call, each conversion's range test, exactness tests, enum name check, bool test, string cut, list
  shrink, report counts, backup rules, key/version checks, step order, the view's walk), the compile-time checks (one
  mutant per `save_error_*` caller, caught by the negative-compile ctests) and the fake's power cut; Release-path mutants
  in pc-release; survivors become tests or are argued equivalent. Files: plan.md ("Mutation testing"). Covers VI.
- [ ] **T8.2** [P] Release strings: `scripts/check_release_strings.sh` gains the dump's and the report's Debug-only texts
  (one `forbidden=` line after the input-actions line); run on the 3ds-release and pc-release `saves` executables (no
  member names, no dump text). Covers FR-17, FR-15.
- [ ] **T8.3** [P] Opt-in by linking: probes (`build/ref-027/probe_*`, not committed) linked against the 3ds-release
  libraries: a game that stores and loads a save without enums links no `src/data/` object; with an enum field, only
  `schema_registry.o`; no `data_load`, `data_index`, `index_validate`, `data_validate` symbol in either (map file);
  `hello` sizes and `nm` equal T0.1's; the saves module's `.text` recorded against plan.md's estimate. Files: plan.md
  ("Stage 8 results"). Covers FR-20, AC-7.
- [ ] **T8.4** Build times and regressions: every example's console script passes with `--pc`; `.text` of `actions`,
  `data`, `foundation` equal T0.1's; edit `examples/saves/tiny_game.cpp` and `progress.hpp` (the declarations' header) →
  simulator and `.3dsx` times against T0.1's and constitution III. Files: plan.md ("Stage 8 results"). Covers AC-7, III.

## Stage 9: Linux checks (VM 120, run by the lead)

- [ ] **T9.1** On `nnhge-build`: GCC 13, GCC 14 and clang 18 (ASan/UBSan): `nnh_tests` and ctest pass; AC-2's sums,
  CRCs and the v3 bytes identical on all three; a 32-bit host build (`-m32`) of `nnh_tests`' saves tests if the multilib
  is installed (else recorded); ASan/UBSan clean over the validator, apply, view, dump and power cuts; libFuzzer
  `fuzz_saves` 10 minutes (coverage growing, no crash) and `fuzz_save`, `fuzz_bindings`, `fuzz_data` 5 minutes each,
  clean. Files: tasks.md (results), plan.md ("Stage 9 results"). Covers I, X, FR-18, AC-1, AC-4.

## Stage 10: console check (the only steps on the 3DS)

- [ ] **T10.1** (on the PC) Builds and checks: `scripts/dkp.sh cmake --build --preset 3ds-release` and `scripts/dkp.sh
  cmake --build --preset 3ds-dev --target saves_3dsx actions_3dsx data_3dsx` without a warning; T7.5's table still
  true; `hello.elf` equal to T0.1's; plan.md's "Stage 10: console run (prepared)" completed with the recorded sums, CRC,
  byte counts and showcase lines. Covers AC-7, II.
- [ ] **T10.2** Old 3DS, nobody touching it after Y. Three runs, each: Homebrew Launcher, press **Y**, then on the PC:
  1. `build/pc-debug/tools/nnh/nnh run saves --script examples/saves/tests/check.nnhs` → every check line equal to the
     PC's (`saves: v1 sum … migrated 2`, v2, v3, `saves: encode crc …`, slot, backup, summaries, `saves: PASS`), every
     showcase line, the bench lines and the verdict (numbers recorded); screenshots read back;
  2. `build/pc-debug/tools/nnh/nnh run actions --script examples/actions/tests/check.nnhs` → the tree's `saved 40 bytes
     sum` line, `actions: PASS 7/7`;
  3. `build/pc-debug/tools/nnh/nnh run data --script examples/data/tests/check.nnhs` → `data check: PASS checksum
     0xD5BD3020 (4 enemies, 3 items, 1 settings, index 595 bytes)`.
  Files: plan.md ("Console run 1": verdicts, bench numbers, the SD card's make and size, log paths). Covers AC-6, AC-8.
- [ ] **T10.3** (only if a bench target is missed) Console bench refinement: find the cost (3ds-dev disassembly for the
  codec; the bench's per-operation numbers for the SD), apply plan.md's optimisation order without changing a byte
  (goldens hold; tests first for any rewrite); then T10.2 run 1 again; if still missed, the budget is set from the
  measurement with ≈ 25 % margin and written in the spec's AC-8, `bench_scene.cpp` and `docs/api/saves.md` with the
  numbers and the card. Files: plan.md ("Optimisation round", "Console run 2 and budgets set"). Covers AC-8.
- [ ] **T10.4** (optional, the maintainer's choice; decision 21) Copy `slot1.sav` from the SD card into the simulator's
  data folder and load it (the same summary); a battery pull during stores, recorded in the spec if done. Covers AC-6.

## Stage 11: docs, roadmap, status

- [ ] **T11.1** (the Old 3DS costs added after T10.2) `docs/api/saves.md`: declaring a save type (the namespace and
  unqualified-name rule, a header or one source), ids and retired ids (never change an id's meaning), kinds and limits,
  the file and its guarantees (atomic, backup, CRC, cross-platform bytes), loading rules and the report, versions and
  migrations (steps, `OldFields`, paths) with both patterns (steps; keep the old struct), slots, summaries and settings,
  buffers and `max_size`, headroom, embedding with `encode`/`decode`, the dump and `NNH_SAVE_DUMP`, costs (measured, the
  card named), where to save (a save point, a "Saving…" indicator), migrating from raw `nnh::save`, newer files (ask the
  player), misuse, pitfalls; header comments (allocates? per-frame safe? misuse?). Covers VIII, AC-9.
- [ ] **T11.2** Other pages: `docs/api/fs.md` (a link from "Saves"; raw saves stay documented), `docs/api/data.md` (one
  sentence: its value types serve saves too), `docs/architecture/security.md` (the parser and `fuzz_saves` in its table),
  `project-structure.md` (the `saves` module if listed), `patterns.md` (the "Versioned schema" row: implemented),
  `docs/REFERENCES.md` (protobuf, FlatBuffers, Serde, Godot, Unity, Butano, Pokémon Gen III, GBATEK), `CLAUDE.md` (the
  layout paragraph after "Input actions", the commands after the `hello.exe` screenshot line); `python -I
  scripts/docs_tool.py check` → 0 broken links. Covers AC-9.
- [ ] **T11.3** `docs/architecture/decisions.md`: D45 and D46 change-log note "implemented" with the stage refinements.
  Covers AC-9.
- [ ] **T11.4** Final measurements (header cost, build times, `hello` sizes, the module's `.text`) in plan.md and
  `docs/ROADMAP.md`'s baseline table if it changed. Covers III.
- [ ] **T11.5** `docs/ROADMAP.md`: the "Save data with versions" checkbox, with the implemented summary.
- [ ] **T11.6** spec, plan, tasks and `specs/README.md` status → implemented (or "implemented, console T10.2 pending").
- [ ] **T11.7** (the lead, after 022–025 are merged) Merge `spec-027` into `main` per plan.md's "Merge notes"; then the
  full ctest (pc-debug, pc-release), every example's `--pc` script, AC-2's goldens, 022–025's goldens and the platformer
  replay ctest with the tree's lines, `0xD5BD3020`, one 3ds-release build; T10.2 run 1 is repeated if a merge happened
  after it.
