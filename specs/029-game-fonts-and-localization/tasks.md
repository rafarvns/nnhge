# Tasks 029: Game fonts and localization

Status: approved 2026-11-26 (with plan.md)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task is one or a few TDD loops (test first, then code). `[P]` means the task can be done in parallel with the
previous one. Every stage ends with something verifiable on PC (`ctest --preset pc-debug`, a screenshot, a `--pc`
script). The work happens in a **separate git worktree** (branch `spec-029`) created from `main`; shared files only get
insertions at the anchors of plan.md (risk 11). The order: the two contract additions and the remote message first
(small, every platform), then the pure text code and the font format, fonts at run time with the UI seam, the font
converter, the string converter, localization at run time, then the golden session, the example, the bench and the ARM
pre-check before the only console stage (10). Stage 9 is run on the Linux build VM by the lead. Deviations found while
building go into plan.md as "Stage N refinements", as in plans 012–028. While developing, run the module's tests
(`nnh_tests text_`, `font_`, `loc_`, `ui_`; `nnh_assetc_tests font`, `strings`); the full suite at stage ends.

Guards held at every stage end (the "stage-end guards"): ctest pc-debug and pc-release; 3ds-release without a warning;
3ds-release and pc-release `hello` sizes and `nm` **identical** to T0.1's (3DS: byte-identical `.elf`/`.3dsx`);
Debug/Development `hello` growth only from the remote message (stage 1 onwards, recorded); every earlier golden
(`ui: PASS 30400DA6` and its checkpoints, `camera: PASS 5201E6CF`, `queries: PASS 4ACAA177`, `entities: PASS C8FB9A41`,
`prefabs: PASS 7BB7901E`, `hierarchy: PASS 23DB5C36`, `tweens: PASS B44CA3F6`, `pathfinding: PASS 1C0B5992`,
`particles: PASS C5EF1A84`, `transitions: PASS BFE1D810`, the models self-check `A3BF4EDF`, the stereo golden
`B3550D40`, `data check: PASS checksum 0xD5BD3020`, the saves goldens, the platformer replay ctest with the tree's lines —
values re-read from the tree at T0.3).

## Stage 0: baseline

- [x] **T0.1** Baseline numbers (Windows, 3 runs each; scripts in `build/ref-029/`, not committed): `hello.exe`
  (pc-release, pc-debug) and `hello.elf`/`.3dsx` (3ds-release, 3ds-dev) sizes (`size`: text/data/bss, file bytes), `nm -C`
  lists and (3DS) SHA-256; `.text` of the 3ds-release `text`, `sprites`, `ui`, `platformer` ELFs; edit
  `examples/ui/title_scene.cpp` → simulator and `.3dsx` times (the reference for the new example's edit); clean
  `nnh_core` and clean `nnh-assetc`; preprocessed lines and `-fsyntax-only` time of one-line TUs including `<nnh/gfx.hpp>`
  + `<nnh/result.hpp>`; the UI micro-benchmark (plan 028's `build/ref-028/micro`, copied as `build/ref-029/micro_ui`):
  draw hash `7429F9EB` and relayout time. Files: plan.md ("Stage 0 results"). Covers III, FR-36, AC-7.
- [x] **T0.2** [P] Reference screenshots for AC-7 in `build/ref-029/ref/`: `hello`, `text`, `sprites` (world and
  `SPRITES_DEMO=bench` top), `ui` (`UI_SHOT=title|options|list|grid|dialog|modal|theme2`), platformer
  `PLATFORMER_DEMO=bench` (top), level 1 at 90, `PLATFORMER_DEMO=replay` at 64 and 312; `build/ref-029/compare.sh`
  retakes them into `now/` and compares bytes. Files: plan.md. Covers AC-7, FR-14.
- [x] **T0.3** [P] Goldens and counts today: the stage-end guards' values as in the tree; `nnh_tests` count against
  `kMaxTests` (4096; this spec adds ≈ 550 tests: raise it to 8192 in `tests/framework/test_main.cpp`, one commit, if
  fewer than 600 are left). Files: plan.md, `tests/framework/test_main.cpp`. Covers VI.

## Stage 1: the contract, the platforms, the system language, the remote message

- [x] **T1.1** `texture_create_alpha` in the contract and the fake: `video.hpp` (declaration and comment of plan.md),
  `engine/platforms/fake/src/texture_alpha_fake.cpp` (coverage copy per handle, `alpha_textures_created`, `bytes = w × h`),
  `platform_fake.hpp`. Test: `tests/platform_conformance/test_contract.cpp` (`texture_create_alpha_accepts_coverage`:
  sizes 8..1024, handle non-zero, bytes = w × h, destroy; a frame drawing a sprite of it), fake-only test of the stored
  bytes. Files: those, `engine/platforms/fake/CMakeLists.txt`. Covers FR-2, I.
- [x] **T1.2** [P] The PC: `engine/platforms/pc/src/texture_alpha_pc.cpp` (white RGBA32 + coverage, nearest, blend, bytes =
  w × h). Test: conformance on pc; `tests/platform_conformance/test_texture_alpha_pc.cpp` (a coverage texture tinted
  red over blue, read back through `remote_capture`, equals an RGBA white-alpha texture drawn the same way, byte for
  byte, for every 16th coverage value). Covers FR-2, FR-13.
- [x] **T1.3** [P] The 3DS: `texture_3ds.hpp` (the `Texture3ds` struct moved out of `texture_3ds.cpp`, nothing else
  changed), `texture_alpha_3ds.cpp` (`GPU_LA4`, `0xF0 | (a / 17)`, Morton by 8 × 8 tiles with a 64-entry offset table,
  `C3D_TexFlush`, nearest, clamp). Test: 3ds-release and 3ds-dev build without a warning; `texture_3ds.o` and `hello.elf`
  byte-identical to T0.1's; the LA4 nibble order and 4 → 8-bit expansion checked against 3dbrew's format table and
  written in a comment (verified on the console at T10.2, AC-5). Covers FR-2, I, XI.
- [x] **T1.4** The system language: `system.hpp` contract `system_language()`, `language_fake.cpp`
  (`fake::set_system_language`), `language_pc.cpp` (`NNH_SIM_LANGUAGE`, `language.ini` beside `window.ini`, `en`; bounded
  parse), `language_3ds.cpp` (CFGU language + region table of plan.md, cached, `en` on failure); core
  `system/system_language.cpp` (`system::language()`: override, else platform) and `system/language_override.cpp`; tag
  well-formedness helper shared with `loc_match` (pure, in `system/language_tag.{hpp,cpp}`). Test:
  `tests/system/test_system_language.cpp` (fake value, override and release, never null), conformance
  (`system_language_is_a_well_formed_tag`, stable across calls) on fake and pc, `test_language_tag.cpp` (every form of
  plan.md, refusals), a PC test of `language.ini` parsing (bounded, invalid lines ignored). Covers FR-27.
- [x] **T1.5** The remote message: `remote_wire.{hpp,cpp}` (`MessageType::Language = 8`; bytes 8–15 the code, NUL-padded;
  all zero = off; validation of plan.md; decoded into `press`/`release` with `pack_language`/`unpack_language`;
  `remote_schedule.hpp`'s 32-byte assert holds), `remote_session.cpp` (apply → `set_language_override`; off → the
  console's own (or `en` while a script is attached); an exclusive Hello → `en`; Bye/disconnect → cleared). Test:
  `tests/remote/test_remote_wire.cpp` (round trips, every refusal: bad characters, interior NUL, empty, leading `-`),
  `tests/remote/test_remote_session.cpp` (apply, off, exclusive, bye), the remote fuzz target's corpus gains a Language
  message. Covers FR-27, D28.
- [x] **T1.6** [P] The `nnh` tool: `tools/nnh/src/remote/script_parser.cpp` (`{"language", Command::Language, 1, 1,
  "language CODE|off"}`, the same validation), `script_timeline.*` (frame-stamped Language messages). Test:
  `tools/nnh/tests/test_script_parser.cpp` (arity, `off`, refusals with their messages),
  `tools/nnh/tests/test_script_timeline.cpp` (the encoded 16 bytes). Covers FR-27, AC-4.
- [x] **T1.7** Stage end: the stage-end guards; Debug/Development `hello` growth from T1.5 measured (3ds-dev, pc-debug)
  and recorded; plan.md ("Stage 1 results"). Covers XI, AC-7.

## Stage 2: pure text code and the font format

- [x] **T2.1** [P] UTF-8 coverage: `tests/text/test_utf8.cpp` extended (every sequence length, U+10FFFF, U+110000,
  overlongs of each length, surrogates, truncated sequences at the NUL, continuation bytes alone). Code changes only if a
  case fails. Covers FR-12.
- [x] **T2.2** Break classes: `src/text/break_class.{hpp,cpp}` (space, hyphen, ID ranges, Hangul, combining/zero-width,
  no-start and no-end tables of plan.md; pure; compiled into `nnh-assetc` too). Test: `tests/text/test_break_class.cpp`
  (every range edge ±1, every table entry, every entry ≥ U+2000, Latin-1 classes equal spec 028's wrapper's). Covers
  FR-10, FR-12.
- [x] **T2.3** The font format and validator: `src/font/font_format.{hpp,cpp}` (v1 layout, writer helpers building a
  file from glyph tables), `src/font/font_validate.cpp`, `src/font/page_codec.{hpp,cpp}` (nibble pack/unpack, PackBits).
  Test: `tests/font/test_font_format.cpp` (a hand-built font written and read back), `test_font_validate.cpp` (each
  refusal reason on a corrupted copy), `test_page_codec.cpp` (round trips, runs of 1..128, malformed runs, exact
  output size); mini-fuzz `tests/fuzz/fuzz_font_validate.cpp`. Covers FR-2, FR-20 (fonts), X.
- [x] **T2.4** The font block and lookups: `src/font/font_block.hpp` (layout, inline Latin-1 and bucket lookups, kerning
  run search), `src/font/font_index.cpp` (derive the tables, beyond-BMP binary search). Test:
  `tests/font/test_font_lookup.cpp` (Latin-1 table, buckets with 0..32 glyphs, the first and last code point of each
  bucket, U+10000+, kerning runs of 0, 1, 255 pairs; a counter: ≤ 3 compares per lookup on the CJK fixture table). Covers
  FR-6, FR-34.
- [x] **T2.5** Faces and runs: `src/text/text_face.hpp` (built-in or font + fallbacks + scale), `src/text/text_runs.{hpp,cpp}`
  (pen walk, kerning within one font, ink extent, alignment `width / 2` rounded down, zero-advance characters). Test:
  `tests/text/test_text_runs.cpp` (hand-computed pens for kerning pairs, negative offsets, scale 1..8, centre/right on
  odd widths, combining marks and ZWJ). Covers FR-6, FR-7, FR-12.
- [x] **T2.6** The shared wrapper: `src/ui/ui_wrap.*` moved to `src/text/text_wrap.{hpp,cpp}` (pixels through a face; ID
  breaks, U+2010, U+3000, Hangul at spaces, kinsoku with the back-off rule, the ellipsis `…`/`...`). Test:
  `tests/text/test_text_wrap.cpp` (spec 028's `test_wrap.cpp` cases moved with their expected lines unchanged and widths
  as cells × 8; the new rules case by case), `tests/ui/reference/ui_wrap_cells.{hpp,cpp}` (a frozen copy of the pre-029
  wrapper) + `tests/ui/test_ui_seam_equivalence.cpp` part 1 (100 000 seeded Latin-1 strings × widths 1..60 cells × max
  lines 0..4: identical lines, bytes, widths and ellipsis flags). Files: `engine/core/CMakeLists.txt` (the UI block loses
  `ui_wrap.cpp`, the text block gains the new files). Covers FR-10, FR-14.
- [x] **T2.7** Stage end: the stage-end guards (every `ui_` test still passes); plan.md ("Stage 2 results"). Covers AC-7.

## Stage 3: fonts at run time, drawing, measuring, the UI seam

- [ ] **T3.1** The font table, `Font` and `FontRef`: `<nnh/font.hpp>`, `gfx.hpp` additions (`FontRef`, `FontMetrics`,
  `TextCaret`, the four `TextOptions` fields, declarations), `src/font/font_table.{hpp,cpp}`, `src/font/font.cpp`. Test:
  `tests/font/test_font_table.cpp` (32 entries, refcount by path hash, generations, stale refs assert and resolve to the
  built-in, `g_font_resolve` null until the first load), `test_font_metrics.cpp`. Covers FR-5, FR-6, FR-35.
- [ ] **T3.2** Loading: `src/font/font_load.cpp`, `src/gfx/texture_alpha.cpp` (`detail::texture_from_coverage`, tag,
  D47 release). Test: `tests/font/test_font_load.cpp` (fonts written in memory into the fake's assets: `NotFound`,
  `InvalidData` with the validator's reason logged, `OutOfMemory` via a failing tag, `Unsupported` at 33, sharing by
  path, fallbacks one level deep, colour pages through RGBA, staging freed, failure cleanup leaves no texture, D47
  counters on destroy), `test_font_cost.cpp`. Covers FR-2, FR-5, FR-11, FR-34.
- [ ] **T3.3** Drawing: `src/text/text_draw.cpp`, `src/text/text_missing.{hpp,cpp}`, `src/gfx/draw_text.cpp` (the
  dispatch only). Test: `tests/text/test_text_draw_fonts.cpp` (plan.md's FR-7/9/11/12 cases against hand-computed fake
  items), `test_text_missing.cpp`, `tests/text/test_text_builtin_unchanged.cpp` (the built-in path's items with every
  new option at its default identical to a recording made before the change; a huge `wrap_width` gives the same items).
  Covers FR-7, FR-9, FR-11, FR-12, FR-13.
- [ ] **T3.4** Measuring: `src/text/text_measure.cpp` (`measure_text(utf8, options)`, `measure_chars`, `font_metrics`).
  Test: `tests/text/test_text_measure_box.cpp` (no drawn item outside the measured box over 10 000 seeded strings ×
  fonts × scales × wraps, shadow and outline extents as documented), `tests/text/test_measure_chars.cpp` (bytes, pens,
  lines, alignment, capacity). Covers FR-8, FR-10.
- [ ] **T3.5** The UI seam in pixels: `ui_text_seam.*`, `ui_layout_measure.cpp`, `ui_draw_widgets.cpp`,
  `ui_draw_list.cpp`, `ui_reveal.cpp`, `ui_list.cpp`, `ui_parts.cpp`, `ui_draw_out.*` (built-in arithmetic and hash
  unchanged), `ui_block.hpp` (font slots), `ui_create.cpp`. Test: every `ui_` test with no expected value edited except
  `test_create.cpp`'s block formula (+32 B; the reviewed diff recorded in plan.md); `test_ui_seam_equivalence.cpp` part 2
  (labels, buttons, choices, sliders, lists: rectangles and wrap lines equal to the reference cell code);
  `ui_self_check_pc` keeps `30400DA6` and its checkpoints; `build/ref-029/micro_ui` draw hash `7429F9EB`, relayout within
  ±5 % of T0.1's. Covers FR-14, AC-7.
- [ ] **T3.6** UI font slots: `UiFont::Slot0..Slot7`, `kMaxUiFonts`, `Ui::set_font` in `src/ui/ui_fonts.cpp`. Test:
  `tests/ui/test_ui_fonts.cpp` (proportional label sizes, wrapping, `…`, reveal by characters, choice/slider/list/button
  sizes with a font; `set_font` relayouts; a stale font draws with the built-in; the draw hash folds a slot only when it
  isn't built-in). Covers FR-14, FR-35.
- [ ] **T3.7** [P] Debugging and misuse: `src/font/font_debug.cpp` (`dump`, `debug_draw_pages`; Release empty). Test:
  `tests/font/test_font_debug.cpp` (the dump's exact text, page view items, pc-release nothing),
  `tests/font/test_font_misuse.cpp` (plan.md's font cases assert and are harmless on the Release path). Covers FR-33,
  FR-35.
- [ ] **T3.8** Stage end: the stage-end guards; `compare.sh` (every reference identical, `ui` included); include checks
  (`<nnh/font.hpp>` and the grown `<nnh/gfx.hpp>`: within +400 lines and +0.05 s of T0.1's); plan.md ("Stage 3 results":
  bytes per glyph and per font, header cost). Covers III, AC-7, FR-36.

## Stage 4: the font converter

- [ ] **T4.1** Vendoring: `third_party/stb_truetype/` and (question 2) `third_party/stb_image/` — the upstream files at one
  pinned `nothings/stb` commit, unmodified, SHA-256 and licence in each `README.md` (as `stb_vorbis`), one `.c` per library
  defining the implementation (`STBI_ONLY_PNG`, `STBI_NO_STDIO`), host-only object libraries with warnings off and
  `-ffp-contract=off` (`/fp:precise` under MSVC), linked only into `nnh-assetc`. Test: `nnh-assetc` builds on pc presets and
  in the 3DS presets' host build; `nm` of a 3ds-release game shows no `stbtt_`/`stbi_` symbol. Files: those,
  `tools/assetc/CMakeLists.txt`. Covers IX, III.
- [ ] **T4.2** [P] Test fonts: `scripts/make_test_fonts.py` (stdlib only, `python -I`) writes `tests/data/fonts/`:
  `nnhtest.ttf` (≈ 60 glyphs of simple outlines: ASCII letters and digits, `…`, U+FFFD, a few kana and CJK code points as
  boxes, a `kern` table, a `GPOS` pair table), `nnhtest_cff.otf` (the same in CFF), BMFont `mono.fnt` + `mono_0.png`
  (text form), `mono_xml.fnt` (XML form), `colour.fnt` + `colour_0.png`, and their `.font` descriptions; outputs committed
  (building and testing need no Python); rerunning the script gives the same bytes (checked once by hand, noted).
  Covers IX, AC-1.
- [ ] **T4.3** BMFont reading: `tools/assetc/src/bmfont_text.{hpp,cpp}` (text and XML through one attribute tokenizer,
  bounded lines; binary refused). Test: `tools/assetc/tests/test_bmfont_text.cpp`, `test_bmfont_xml.cpp` (every field,
  quoting, ids twice, values out of range, `file:line: reason` texts exactly). Covers FR-1, X.
- [ ] **T4.4** BMFont conversion: `png_pages.{hpp,cpp}` (stb_image wrapper: RGBA8 out, size and texture rules),
  `bmfont_convert.{hpp,cpp}` (pages named `<font>_<n>.png`, mono vs colour, metrics, re-packing). Test:
  `test_bmfont_convert.cpp` (fixtures: glyph rects, offsets, advances, kerning, mono → A4, colour → RGBA8, a misnamed or
  missing page, a rect outside its page). Covers FR-1, FR-2.
- [ ] **T4.5** [P] Descriptions and character sets: `font_desc.{hpp,cpp}`, `charsets.{hpp,cpp}` (every named set of
  plan.md; `kyoiku` and `jis-level1` as compact tables in `charsets_cjk.cpp`). Test: `test_font_desc.cpp` (every key,
  defaults, refusals), `test_charsets.cpp` (each set's size and first/last code point; ranges; `.chars` files; the
  `strings` and `language-names` sources through a stub). Covers FR-1, FR-3, FR-4.
- [ ] **T4.6** The TrueType pre-validator: `ttf_check.{hpp,cpp}` (every offset stb_truetype follows; CFF2 and
  variable-only refused). Test: `test_ttf_check.cpp` (each table's corruptions on copies of the fixtures: truncated
  directory, `loca` past `glyf`, `cmap` ranges past the end, CFF INDEX overflow, `GPOS` subtable outside). Covers X.
- [ ] **T4.7** Rasterising: `ttf_raster.{hpp,cpp}` (stb_truetype on the padded copy; integer rounding of metrics;
  4-bit coverage; anti-aliasing off with `threshold`), `outline_bake.{hpp,cpp}` (dilation 1..3). Test:
  `test_ttf_raster.cpp` (hand-computed advances/kerning/ascent for the fixtures at 8, 12, 16, 32 px; a box glyph's exact
  coverage; AA off; the same glyph bytes for the glyf and CFF fixtures where the outlines are equal), `test_outline_bake.cpp`.
  Covers FR-1, FR-4, FR-6, FR-9.
- [ ] **T4.8** Packing and writing: `glyph_pack.{hpp,cpp}`, `font_writer.{hpp,cpp}` (the shared `font_format` +
  page codec; replacement and ellipsis glyphs; fallbacks and licence paths), `convert_font.{hpp,cpp}`, `cli_font.cpp`
  (`nnh-assetc font <in> <out> --assets <dir> --depfile <d> [--strings-sources <list>] [--languages <list>]`, the report
  line). Test: `test_glyph_pack.cpp` (determinism, sides, forced page, padding, the 8-page refusal message),
  `test_cli_font.cpp` (both sources end to end; the depfile; licence required/`own`; rewrites only on change; core's
  validator accepts every output). Covers FR-1, FR-2, FR-3, FR-9, FR-11.
- [ ] **T4.9** The build: `cmake/nnh_assets.cmake` (routing of plan.md before the data rule; pipeline-owned pages and
  sources; `nnh-assetc` needed for `.font`/`.fnt`), the data routing's reserved keys. Test: ctests `font_routing`
  (a fixture game: outputs, unpacked sources, the packed licence, the asset index) and `font_fixtures_golden` (converted
  fixture checksums, recorded in pc-debug; equal in pc-release). Files: `tests/CMakeLists.txt` (029 block: converting
  `tests/data/fonts` at build for the core tests). Covers FR-1, FR-20, AC-2, III.
- [ ] **T4.10** [P] Fuzzing: `tools/assetc/tests/libfuzzer_assetc_bmfont.cpp`, `libfuzzer_assetc_ttf.cpp` (through the
  pre-validator and the rasteriser on a size and a few code points), `libfuzzer_assetc_png_pages.cpp`,
  `libfuzzer_assetc_font_desc.cpp`, mini-fuzz versions in `nnh_assetc_tests`; seed corpora from the fixtures. Test: each
  runs 60 s clean under ASan on the PC's host build (the VM's clang run at T9.1). Covers X, AC-1.
- [ ] **T4.11** Stage end: the stage-end guards; `nnh-assetc` clean build time; plan.md ("Stage 4 results": fixture
  goldens, the stb commit). Covers III, AC-2.

## Stage 5: the string converter

- [ ] **T5.1** The TOML strings mode: `tools/assetc/src/toml*.{hpp,cpp}` (an options value: dotted keys with TOML's
  table semantics, multi-line basic strings with line-ending backslashes, 1 MiB files, 4 KiB lines; data mode unchanged).
  Test: `tools/assetc/tests/test_toml_strings_mode.cpp` (each feature, conflicts such as `a = "x"` then `a.b`, limits),
  every existing TOML test unchanged; `libfuzzer_assetc_toml.cpp` gains the mode. Covers FR-16, X.
- [ ] **T5.2** Reading string files: `strings_model.hpp`, `strings_read.{hpp,cpp}` (keys from sections and dotted names,
  the `[language]` tables, the folder form merged with duplicates across files refused, code from the path),
  `placeholders.{hpp,cpp}`. Test: `test_strings_read.cpp`, `test_placeholders.cpp` (plan.md's syntax, every malformed
  case with its column). Covers FR-15, FR-16, FR-17, FR-18.
- [ ] **T5.3** Plural rules: `plural_rules_cldr.{hpp,cpp}` (the 36 shipped languages' CLDR 46 integer rules as text,
  with the source cited), `plural_compile.{hpp,cpp}` (parser, bytecode, required categories), core `src/loc/plural.{hpp,cpp}`
  (the interpreter, shared). Test: `test_plural_compile.cpp` (syntax, refusals, required categories incl. the
  1 000 000 `many` of fr/es/it/pt/ca), `tests/loc/test_plural_rules.cpp` (every shipped language's compiled rule against
  `tests/data/plurals/cldr46_integer_samples.txt`, transcribed from CLDR 46 with its notice). Covers FR-17.
- [ ] **T5.4** Checks: `strings_check.{hpp,cpp}` (every FR-19 error and warning of plan.md; did-you-mean through
  `suggest`; `STRICT_STRINGS`). Test: `tools/assetc/tests/test_strings_check.cpp` (one fixture per message under
  `tests/data/strings/errors/`, the exact `file:line: reason` text). Covers FR-19.
- [ ] **T5.5** Tables: core `src/loc/loc_file.{hpp,cpp}` (v1 layouts and writer helpers, shared), `strings_writer.{hpp,cpp}`,
  `cli_strings.cpp` (`nnh-assetc strings <code> --files … --base-files … --out … [--release] --show …`). Test:
  `test_strings_writer.cpp` (sorted hashes, buckets, records, `{{` → `{`, the zero guard; Release: no name byte),
  `test_cli_strings.cpp` (end to end, rewrite only on change). Covers FR-20.
- [ ] **T5.6** The index: `coverage.{hpp,cpp}`, `pseudo.{hpp,cpp}`, `manifest_writer.{hpp,cpp}`, `cli_strings_index.cpp`
  (hash collisions across languages, roles naming fonts, coverage through roles and fallbacks, chains, the manifest,
  `qps` in Debug/Development). Test: `test_coverage.cpp` (the `買` message exactly; fallbacks counted; chain roles),
  `test_pseudo.cpp`, `test_cli_strings_index.cpp` (manifest contents; no `qps` with `--release`). Covers FR-18, FR-19,
  FR-21, FR-29, FR-31.
- [ ] **T5.7** The build: `cmake/nnh_strings.cmake` (per-language steps, the index step, the language list file),
  `cmake/nnh_game.cmake` (`LANGUAGES`, `BASE`, `STRICT_STRINGS`), font steps depending on the string files of their
  `strings` languages. Test: ctests `strings_declaration_*` (missing/unlisted files, bad codes, both forms of one
  language), `strings_incremental` (editing `de.strings` reruns only the `de` step, its fonts and the index — Ninja's
  `-n -d explain` list), Release outputs without names. Covers FR-15, FR-20, AC-7, III.
- [ ] **T5.8** [P] Fuzzing: `libfuzzer_assetc_strings.cpp` (a string file + a base through the checks and the writer) and
  its mini-fuzz version; core mini-fuzz `tests/fuzz/fuzz_loc_table.cpp`, `fuzz_loc_manifest.cpp` over the validators
  (stage 6's `loc_validate.cpp`, written here test-first). Covers X, AC-1.
- [ ] **T5.9** Stage end: the stage-end guards; the fixture goldens gain the string fixtures; plan.md ("Stage 5
  results"). Covers AC-2.

## Stage 6: localization at run time

- [ ] **T6.1** Validators: `src/loc/loc_validate.cpp` (tables and manifest, every check of plan.md). Test:
  `tests/loc/test_loc_validate.cpp` (each reason on corrupted copies of written fixtures). Covers FR-20, X.
- [ ] **T6.2** State, start, matching: `<nnh/localization.hpp>`, `src/loc/loc_state.{hpp,cpp}`, `loc_match.{hpp,cpp}`,
  `loc_queries.cpp`. Test: `tests/loc/test_loc_start.cpp` (manifest, block size, tag `strings`, queries, `find`,
  `NotFound`, player > system > base, the log line, `LanguageChanged` after start), `tests/loc/test_language_match.cpp`.
  Covers FR-21, FR-22.
- [ ] **T6.3** Lookup: `src/loc/loc_lookup.cpp` (`tr`, `resolve`, `key()`, chains, markers). Test:
  `tests/loc/test_loc_lookup.cpp` (current, chain, base; the hash equals `UiText::name`'s; unknown keys: name once in
  Debug/Development, `??` in pc-release; before start `""`). Covers FR-16, FR-23.
- [ ] **T6.4** Formatting: `src/loc/loc_number.{hpp,cpp}`, `src/loc/loc_format.cpp`. Test: `tests/loc/test_loc_number.cpp`
  (grouping styles, `grouping_min`, `INT64_MIN`/`MAX`, zero, negatives), `tests/loc/test_loc_format.cpp` (plan.md's
  FR-24 cases). Covers FR-17, FR-18, FR-24.
- [ ] **T6.5** Switching: `src/loc/loc_switch.cpp` (pending, the D32 hook, block reload with failure restore, the guard,
  serial, `LanguageChanged`, following the system language). Test: `tests/loc/test_loc_switch.cpp` (plan.md's FR-25/28
  cases with `nnh::frame()` on the fake; an assets failure injected through the fake). Covers FR-25, FR-27, FR-28.
- [ ] **T6.6** Role fonts and the UI hook: `src/loc/loc_fonts.cpp`, `src/loc/loc_ui.cpp`. Test:
  `tests/loc/test_loc_fonts.cpp` (roles, chain roles, sharing, release, unknown role, nothing loaded without opting in),
  `tests/loc/test_loc_ui.cpp` (`apply`; a scene's subscription re-applies on `LanguageChanged`; the UI shows the new
  texts and fonts the same frame). Covers FR-29, FR-30.
- [ ] **T6.7** [P] Debugging, misuse, cost, random operations: `src/loc/loc_debug.cpp`. Test: `tests/loc/test_loc_debug.cpp`
  (dump text exactly, show keys, `NNH_LOC_SHOW_KEYS`), `tests/loc/test_loc_misuse.cpp`, `tests/loc/test_loc_cost.cpp`
  (no allocation over 100 000 `tr`/`format`), `tests/loc/test_loc_random_ops.cpp` (100 000 seeded operations with
  invariants), `tests/loc/test_docs_snippets.cpp` (the API page's examples compile and run). Covers FR-26, FR-33, FR-34,
  FR-35.
- [ ] **T6.8** Stage end: the stage-end guards; include check (`<nnh/localization.hpp>` within +400 lines and +0.05 s);
  plan.md ("Stage 6 results": bytes per key, the block of the fixtures). Covers III, FR-36.

## Stage 7: the golden session, `examples/localization`, the bench, the ARM pre-check

- [ ] **T7.1** Example assets (needs the answers to questions 2 and 3): `examples/localization/art/make_hud_font.py`
  (original pixel BMFont → `assets/fonts/hud.fnt`, `hud_0.png`, committed), `fonts/make_subset.py` + `fonts/src/README.md`
  (outside the assets) and the committed subset `assets/fonts/src/NNHExampleSansJP.otf` with
  `assets/licences/OFL-NNHExampleSansJP.txt` (the upstream file approved by the maintainer; nothing downloaded without
  it), `assets/fonts/src/kyoiku_1006.chars`, the `.font` descriptions, `tools/make_bench_strings.py` →
  `assets/strings/en/bench.strings`, the five languages' strings. Test: the example's assets convert without a warning
  (Release: `STRICT_STRINGS` clean); the build's report lines recorded; the subset ≤ 300 KB. Covers AC-3, AC-6, IX.
- [ ] **T7.2** The self-check (AC-2): `examples/localization/self_check.{hpp,cpp}` (plan.md's session, checkpoints per
  language, `kLocCheckGolden`); goldens from pc-debug, equal in pc-release. Test:
  `tests/loc_example/test_self_check.cpp` (on the fake with the example's converted assets; the fake's items equal the
  pure layout's; a one-byte change of a string or a glyph fails it — checked once by hand). Files: `tests/CMakeLists.txt`.
  Covers AC-2, FR-13, FR-32.
- [ ] **T7.3** The showcase: `main.cpp`, `demos.hpp`, `roles.hpp`, `title_scene`, `options_scene`, `dialog_scene`,
  `shop_scene`, `gallery_scene`, `hud.{hpp,cpp}`, `settings.{hpp,cpp}` (the player's language in a spec 027 settings
  type), `CMakeLists.txt` (`LANGUAGES en pt-BR de ru ja BASE en`), `examples/CMakeLists.txt` (after `ui`). Test: PC
  screenshots `LOC_SHOT=title|options|dialog|shop|gallery|ja|ru|qps|pages` read back; the log lines of plan.md;
  `NNH_SIM_LANGUAGE=ja` starts in Japanese. Covers AC-3, FR-22, FR-26, FR-29, FR-30, FR-31.
- [ ] **T7.4** The check scene: `check_scene.{hpp,cpp}` (Y or `LOC_DEMO=check`: `loc: check started`, `loc: <code> sum
  <S>`, `loc: PASS <G>` / `loc: FAIL …`); ctest `loc_self_check_pc`. Covers AC-4.
- [ ] **T7.5** [P] The bench: `bench.{hpp,cpp}`, `bench_scene.{hpp,cpp}` (X or `LOC_DEMO=bench`: plan.md's phases, each
  timing only its workload; lines and verdict; Debug `not judged (Debug)`), `fonts/bench_1024.font`. Test: a pc-dev run
  logs every line; the memory line matches plan.md's estimates within 20 % (else plan.md is corrected). Covers AC-6.
- [ ] **T7.6** The script `examples/localization/tests/check.nnhs` (plan.md's outline; every expect followed by `wait 10`
  before the next input, one `expect` per frame) and ctest `loc_check_script_pc` (`RESOURCE_LOCK nnh_remote_port`). Test:
  `nnh run localization --pc --no-build --script …` → PASS twice in a row. Files: plan.md ("Stage 10: console run
  (prepared)" with every expected line). Covers AC-4.
- [ ] **T7.7** ARM11 pre-check (before any console run): `arm-none-eabi-objdump -drC` of the 3ds-dev objects
  `src/text/*`, `src/font/*`, `src/loc/*` and the example's `bench.o`; per AC-6 path record instructions, calls,
  `__aeabi_*` (none in `tr`, the plural interpreter, digits or wrapping), VFP (only the final positions), `pld`;
  `-fstack-usage` along the deepest draw, format and switch paths (< 1 KiB); compare with plan.md's table and fix what
  differs without changing an item, string or golden. Files: plan.md ("Stage 7 ARM pre-check": the table and the
  expected console numbers). Covers AC-6, II.

## Stage 8: mutation testing, release strings, opt-in checks, build times

- [ ] **T8.1** Mutation pass as in plans 019–028: plan.md's planned hand mutants (`build/ref-029/mutants.py`, applied one at
  a time, `nnh_tests text_ font_ loc_ ui_` and `nnh_assetc_tests` each) and Release-path mutants in pc-release; survivors
  become tests or are argued equivalent. Files: plan.md ("Mutation testing"). Covers VI.
- [ ] **T8.2** [P] Release hygiene: `scripts/check_release_strings.sh` gains the dump, missing-key, missing-glyph and
  misuse texts and the example's key names (one `forbidden=` line, read from the Debug manifest); run on the 3ds-release
  and pc-release `localization` executables and packages (no `.nnh_loc` names, no `strings/qps.nnhstr`). Covers AC-8,
  FR-20, FR-31.
- [ ] **T8.3** [P] Opt-in by linking: probes (`build/ref-029/probe_*`, not committed) linked against the 3ds-release
  libraries: a font-only game (no `loc::` symbol), a strings-only game with the built-in font (no `load_font`, `font_`,
  `stb` symbol), a UI game without `set_font` (no `ui_fonts.o`), `loc::tr` only (no `loc_format.o`, no plural code);
  `hello` sizes, `nm` and 3DS bytes identical to T0.1's (Release); Debug/Development growth recorded. Files: plan.md
  ("Stage 8 results"). Covers FR-36, AC-7, XI.
- [ ] **T8.4** Build times and regressions: every example's console script passes with `--pc`; `compare.sh` (text,
  sprites, ui, platformer pixel-identical); edit `examples/localization/title_scene.cpp` → simulator and `.3dsx` times
  against T0.1's reference; edit `strings/de.strings` → reconversion time (< 2 s) and steps run; clean `nnh_core` and
  `nnh-assetc` times. Files: plan.md ("Stage 8 results"). Covers AC-7, III.

## Stage 9: Linux checks (VM 120, run by the lead)

- [ ] **T9.1** On `nnhge-build` (start it per CLAUDE.md, never with the Bazzite VM; shut it down after): GCC 13, GCC 14
  and clang 18 (ASan/UBSan): `nnh_tests`, `nnh_assetc_tests` and ctest pass; `font_fixtures_golden` and AC-2's
  checkpoints identical on all three; the 32-bit host build (`-m32 -msse2 -mfpmath=sse`): the fixture goldens and AC-2
  identical; libFuzzer targets of stage 4/5 (`bmfont`, `ttf`, `png_pages`, `font_desc`, `strings`, `toml`) 10 minutes
  each under ASan/UBSan, plus the mini-fuzzers; any crash in stb_truetype answered by the pre-validator (risk 3).
  Files: tasks.md (results), plan.md ("Stage 9 results"). Covers I, X, FR-13, FR-32, AC-1, AC-2.

## Stage 10: console check (the only steps on the 3DS)

- [ ] **T10.1** (on the PC) Builds and checks: `scripts/dkp.sh cmake --build --preset 3ds-release` and `scripts/dkp.sh
  cmake --build --preset 3ds-dev --target localization_3dsx ui_3dsx platformer_3dsx` without a warning; T7.7's table still
  true; 3ds-release `hello.elf` byte-identical to T0.1's. Covers AC-7, II.
- [ ] **T10.2** Old 3DS, nobody touching it after Y. Runs, each: Homebrew Launcher, press **Y**, then on the PC:
  1. `build/pc-debug/tools/nnh/nnh run localization --script examples/localization/tests/check.nnhs` → every checkpoint
     equal to the PC's and `loc: PASS <G>`, every showcase line, the bench lines and the verdict (numbers recorded);
     screenshots compared with the `--pc` run's (AC-5: positions, empty and full texels identical; partial texels' largest
     difference recorded);
  2. `build/pc-debug/tools/nnh/nnh run ui --script examples/ui/tests/check.nnhs` → `ui: PASS 30400DA6` and the UI bench
     within its budgets (relayout < 0.93 ms, draw ratio ≤ 1.15): the seam change costs nothing on the console;
  3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs` → `bench: PASS 4 layers +
     300 sprites, …`;
  4. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` → the tree's replay lines
     and `60 Hz PASS`;
  5. (yes/no) the maintainer sets the console's language to Português (or Deutsch) in System Settings, then HBL → Y and
     `build/pc-debug/tools/nnh/nnh run localization` (no script): the log shows `loc: language pt-BR (system)` (or `de`)
     and the title is in that language — yes/no; then the setting goes back.
  Files: plan.md ("Console run 1": verdicts, bench numbers, log paths, the screenshot comparison). Covers AC-4, AC-5,
  AC-6, FR-27. Every expected line of run 1 is in plan.md, "Stage 10: console run (prepared)" (recorded from the `--pc`
  run).
- [ ] **T10.3** (only if a bench target is missed) Optimisation round: find the cost (3ds-dev disassembly, the bench's
  breakdown), apply risk 1's list without changing a glyph item, string, golden or draw hash (tests first for any
  rewrite); then T10.2 run 1 again; if still missed, the budget is set from the measurement with ≈ 25 % margin and
  written in the spec's AC-6, `bench_scene.cpp` and `docs/api/fonts.md` / `localization.md`. Files: plan.md
  ("Optimisation round", "Console run 2 and budgets set"). Covers AC-6.
- [ ] **T10.4** (contingency, only if the glyph ratio or the frame target is still missed after T10.3, and with the
  maintainer's OK) A glyph batch through D39's mechanism (the spec's named fallback, spec 028 risk 2): the design written
  into plan.md and D39's row first (an amendment), then tests (items equal to the glyph sprites `draw_text` would make;
  budgets), then code; every reference screenshot unchanged. Covers AC-6, II.

## Stage 11: docs, roadmap, status

- [ ] **T11.1** `docs/api/fonts.md` (font assets, descriptions, BMFont, character sets, metrics, drawing, outline,
  fallback, missing characters, measuring, `measure_chars`, costs measured at T10.2, memory, misuse) and
  `docs/api/localization.md` (languages, string files, placeholders and plurals with the shipped rule list, build checks
  with their messages, `tr`/`format`, switching, `LanguageChanged`, the system language, following it, font roles,
  widgets, pseudo-localization, saving the choice with spec 027, costs, misuse); header comments (allocates? per-frame
  safe? misuse?). Covers VIII, AC-9.
- [ ] **T11.2** `docs/guides/localizing-a-game.md`: from one language to three; handing files to translators; plurals and
  placeholders; adding a CJK language within Old 3DS memory (strings-driven character sets, one page); font licences;
  the language option and the saved choice; localized images by `loc::code()`; testing with `language` and `qps`.
  Covers AC-9.
- [ ] **T11.3** Other pages: `docs/api/text.md` (game fonts, wrapping, the shared wrapper), `gfx.md` (the new
  `TextOptions` fields, `measure_text` overload), `ui.md` (font slots, `loc::apply`, the seam in pixels), `assets.md`
  (routing of `.font`, `.fnt`, pages, sources, `strings/`, `.nnh_loc`), `system.md` (`language()`), `simulator.md`
  (`NNH_SIM_LANGUAGE`, `language.ini`), `saves.md` (storing the language), `docs/guides/testing-on-the-console.md`
  (`language`), `docs/guides/menus.md` (a language option), `docs/tech/cmake.md` (`LANGUAGES`, the vendored host
  libraries), new `docs/tech/third-party.md` or a section of `docs/tech/README.md` (stb_truetype, stb_image: versions,
  update procedure, fuzzing), `docs/architecture/rendering.md` and `platforms.md` (the design notes made final),
  `patterns.md`, `project-structure.md` (`src/font/`, `src/loc/`, `third_party/` users), `docs/REFERENCES.md` (BMFont,
  stb, Godot, Unity, CLDR with its notice, UAX #14, JIS X 4051, libctru `cfgu.h`, 3dbrew), `CLAUDE.md` (layout paragraph
  and the `localization` commands); `python -I scripts/docs_tool.py check` → 0 broken links. Covers AC-9.
- [ ] **T11.4** `docs/architecture/decisions.md`: D49 and D50 change-log notes "approved"/"implemented" with the stage
  refinements and the maintainer's answers; D28's `Language` message and D32's new user noted. Covers AC-9.
- [ ] **T11.5** Final measurements (header costs, build times, `hello` sizes, the modules' `.text`, the example's memory)
  in plan.md and `docs/ROADMAP.md`'s baseline table if it changed. Covers III.
- [ ] **T11.6** `docs/ROADMAP.md`: the "Game fonts" and "Localization" checkboxes, with the implemented summary.
- [ ] **T11.7** spec, plan, tasks and `specs/README.md` status → implemented (or "implemented, console T10.2 pending").
- [ ] **T11.8** (the lead) Merge `spec-029` into `main` (rebased; insertions at plan.md's anchors); then the full ctest
  (pc-debug, pc-release), every example's `--pc` script, AC-2's golden and every earlier golden, one 3ds-release build;
  T10.2 runs 1 and 2 repeated if the merge happened after them.
