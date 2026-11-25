# Tasks 028: UI widgets

Status: approved 2026-11-25 (with plan.md)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task is one or a few TDD loops (test first, then code). `[P]` means the task can be done in parallel with the
previous one. Every stage ends with something verifiable on PC (`ctest --preset pc-debug`, a screenshot, a `--pc`
script). The work happens in a **separate git worktree** (branch `spec-028`) created from `main`; shared files only get
insertions at the anchors of plan.md (risk 11). Pure helpers come first (box arithmetic, anchors, wrapping, numbers,
9-slice geometry, repeat, the geometric rule), then the tree and layout, focus, input and results, lists and touch,
styles and drawing; the golden session, the example and the ARM pre-check come before the only console stage (10).
Stage 9 is run on the Linux build VM by the lead. Deviations found while building go into plan.md as "Stage N
refinements", as in plans 012–027. While developing, run the module's tests (`nnh_tests ui`); the full suite at stage
ends.

Guards held at every stage end (the "stage-end guards"): ctest pc-debug and pc-release; 3ds-release without a warning;
`hello` sizes and `nm` **byte-identical** to T0.1's (no core file outside `src/ui/` changes); every earlier golden
(`camera: PASS 5201E6CF`, `queries: PASS 4ACAA177`, `entities: PASS C8FB9A41`, `prefabs: PASS 7BB7901E`, `hierarchy:
PASS 23DB5C36`, `tweens: PASS B44CA3F6`, `pathfinding: PASS 1C0B5992`, `particles: PASS C5EF1A84`, `transitions: PASS
BFE1D810`, the models self-check `A3BF4EDF`, `data check: PASS checksum 0xD5BD3020`, the saves goldens, the platformer
replay ctest with the tree's lines — values re-read from the tree at T0.3).

## Stage 0: baseline

- [ ] **T0.1** Baseline numbers (Windows, 3 runs each; scripts in `build/ref-028/`, not committed): `hello.exe`
  (pc-release, pc-debug) and `hello.elf`/`.3dsx` (3ds-release, 3ds-dev) sizes (`size`: text/data/bss, file bytes) and
  `nm -C` lists; `.text` of the 3ds-release `sprites`, `text`, `actions`, `transitions`, `platformer` ELFs; edit
  `examples/transitions/hud.cpp` → simulator and `.3dsx` times (the reference for the UI example's edit); clean
  `nnh_core`; preprocessed lines and `-fsyntax-only` time of one-line TUs including `<nnh/gfx.hpp>` + `<nnh/result.hpp>`
  (plan: 7 844 lines, 0.52–0.60 s on the planning day). Files: plan.md ("Stage 0 results"). Covers III, FR-33, AC-7.
- [ ] **T0.2** [P] Reference screenshots for AC-7 in `build/ref-028/ref/`: `hello`, `sprites` (world and
  `SPRITES_DEMO=bench`), `text`, `actions`, `transitions` (`TRANSITIONS_SHOT=iris` and `loading`), platformer
  `PLATFORMER_DEMO=bench` (top), level 1 at 90, `PLATFORMER_DEMO=replay` at 64 and 312; `build/ref-028/compare.sh`
  retakes them into `now/` and compares bytes. Files: plan.md. Covers AC-7.
- [ ] **T0.3** [P] Goldens and counts today: the stage-end guards' values as in the tree; `nnh_tests` count against
  `kMaxTests` (this spec adds ≈ 400 tests: raise only if fewer than 500 are left). Files: plan.md. Covers VI.

## Stage 1: the skeleton and the pure helpers

- [ ] **T1.1** The header and the block: `<nnh/ui.hpp>` (limits, enums, `UiKey`, `UiRect`, `UiText`, `UiLayout`,
  `UiPlace`, every desc, `UiInput`, `UiInputConfig`, `UiResult`, `UiDesc`, `UiStats`, `Ui` declarations),
  `<nnh/detail/ui_style.hpp>`, `ui_block.hpp` (header, hot 32 B and cold 56/≤ 64 B records `static_assert`ed, tables),
  `ui_desc_check.{hpp,cpp}`, `ui_create.cpp` (create, move, destroy, roots, `valid`, `stats`; tag `ui`). Test:
  `tests/ui/test_create.cpp` (every desc bound both ways with the field named; `OutOfMemory`; tag bytes = the formula;
  a game tag; one or two roots, their rectangles; two UIs independent; move; empty UIs answer zero), `ui_fixture.hpp`.
  Files: those, `engine/core/CMakeLists.txt` (a `_nnh_ui_sources` block, `-ffp-contract=off`, before the `nnh::core`
  alias), `tests/CMakeLists.txt` (028's block). Covers FR-1, FR-3, FR-33.
- [ ] **T1.2** [P] Box arithmetic: `ui_box_math.{hpp,cpp}` (distribution by stretch ratios, the minimum-exceeds-share
  iteration, remainder pixels first child first, negative extra; alignment start/centre/end/fill). Test:
  `tests/ui/test_box_math.cpp` (hand-computed cases: ratios 1:1, 1:2:1, 255:1; remainders 1..n−1; mins over share;
  zero and negative extra; centre rounding down). Covers FR-9.
- [ ] **T1.3** [P] Anchors: `ui_anchor.{hpp,cpp}` (nine points + offsets, fill with margins, per axis). Test:
  `tests/ui/test_anchor.cpp` (each point on 400×240 and 320×240 parents, odd sizes, negative offsets, fills). Covers
  FR-10.
- [ ] **T1.4** [P] Wrapping: `ui_wrap.{hpp,cpp}` (code points via `detail::next_codepoint`; breaks at spaces (dropped)
  and after hyphens; `\n`; long words by characters; `max_lines` with `...`; line iteration for drawing; the reveal
  limit in code points). Test: `tests/ui/test_wrap.cpp` (each rule; Latin-1 two-byte characters counted as one cell;
  invalid UTF-8 as the replacement cell; an exact fit with no ellipsis; widths of 1..50 cells). Covers FR-5, FR-23.
- [ ] **T1.5** [P] Numbers: `ui_number.{hpp,cpp}` (signed decimal into a buffer, prefix/suffix, truncation). Test:
  `tests/ui/test_number.cpp` (0, ±1, `INT_MIN`, `INT_MAX`, capacity 0..12, prefixes). Covers FR-25.
- [ ] **T1.6** [P] 9-slice geometry: `ui_nine_slice.{hpp,cpp}` (nine destination and source rectangles and scales
  `dst / src`; parts of size 0 skipped; insets clamped proportionally below their sum). Test:
  `tests/ui/test_nine_slice.cpp` (every size from the insets' sum to 400 × a set of regions: parts tile the rectangle
  exactly, every edge `pos + src × scale` within 2⁻¹⁰ of an integer; clamping below). Covers FR-26.
- [ ] **T1.7** The tree: `ui_tree.{hpp,cpp}` (lowest-free slots with `next_generation`, handles, link/unlink as last
  child, subtree destroy, parent rules, keys: `find`, duplicates), the generic part of creation. Test:
  `tests/ui/test_tree.cpp` (appended as last child; stale handles assert; subtree destroy frees every slot; reuse with a
  new generation; full → null handle + assert, Release: one log per UI; children under a non-container assert),
  `tests/ui/test_keys.cpp`. Covers FR-2, FR-4, FR-32.
- [ ] **T1.8** Stage end: the stage-end guards; include check (`<nnh/ui.hpp>` pulls only its includes; preprocessed lines
  and `-fsyntax-only` within +1 500 lines and +0.05 s of T0.1's `gfx.hpp + result.hpp`). Files: plan.md ("Stage 1
  results": record sizes, header cost). Covers FR-33, III.

## Stage 2: widgets, text and layout (pure, fake platform)

- [ ] **T2.1** Text sources and the seam: `ui_text_source.{hpp,cpp}` (copy with compaction on destroy, borrow +
  `mark_text_changed`, key + resolver cache, names by FNV-1a, `text_changed`), `ui_text_seam.{hpp,cpp}` (measure, cell
  width, line height, `draw_run`; `UiFont::Builtin` only). Test: `tests/ui/test_text_sources.cpp` (capacity error;
  compaction keeps other texts; borrowed edits seen after `mark_text_changed`; the resolver called once per key until
  `text_changed`; a null resolution measures 0 and draws nothing). Covers FR-24.
- [ ] **T2.2** Widget kinds and common values: `ui_widgets_display.cpp`, `ui_widgets_input.cpp`, `ui_widgets_boxes.cpp`
  (every kind's record from its desc; row slices and option texts reserved), `ui_setters.cpp` (text, visible, enabled,
  value, style, layout, render offset and opacity (rounded; non-finite asserts), key, neighbours, default, back, item
  count, reveal), `ui_queries.cpp`. Test: `tests/ui/test_min_size.cpp` (every kind's minimum from content, padding,
  parts; fixed sizes), `tests/ui/test_tree.cpp` (part 2: per-kind creation errors of plan.md). Covers FR-5, FR-6, FR-7.
- [ ] **T2.3** Layout: `ui_layout_measure.cpp` (minimum widths, wrap at final widths, minimum heights),
  `ui_layout_arrange.cpp` (widths, heights, positions, anchors on roots and stacks, pre-order indexes, overflow warnings
  once per widget in Debug/Development, `layout_visits`). Test: `tests/ui/test_layout_boxes.cpp` (VBox, HBox, grid box,
  stack, panel; padding, spacing, alignment, expand, stretch, remainder order; hidden children take no space; overflow),
  `tests/ui/test_layout_anchors.cpp` (nine points and fills on both screens; a wrapping label's height from its final
  width). Covers FR-9, FR-10, FR-13.
- [ ] **T2.4** Dirty rules: every dirtying change marks its root; layout at the end of `update()`, on `rect()`, and at
  `draw()` for changes made after `update()` (decision 6, pending the maintainer's answer to question 1). Test:
  `tests/ui/test_layout_dirty.cpp` (no relayout without a change: `layout_visits` unchanged; each change relayouts;
  render offset and opacity never do). Covers FR-13, FR-7.
- [ ] **T2.5** [P] Reveal: `ui_reveal.cpp` (16.16 accumulator, the revealing list ≤ 16, `set_reveal`, `show_all`,
  `fully_shown`). Test: `tests/ui/test_reveal.cpp` (rates 1, 0.5, 0.25, 3 over 600 frames exactly; line breaks of the
  whole text never change while revealing). Covers FR-23.
- [ ] **T2.6** Stage end: the stage-end guards. Files: plan.md ("Stage 2 results"). Covers AC-7.

## Stage 3: focus and navigation

- [ ] **T3.1** Focus: `ui_focus.cpp` (`focus(w|key)`, `focused`, states, targets, repair after hide/disable/destroy:
  next, previous, root default, none). Test: `tests/ui/test_focus.cpp` (targets: hidden, hidden ancestors, disabled
  skipped vs inert, lists with 0 items; repair order; never dangling). Covers FR-14, FR-15.
- [ ] **T3.2** The geometric rule: `ui_geometric.{hpp,cpp}` (pure: candidates beyond the edge, overlap group first, gap,
  cross-axis centre distance, pre-order ties). Test: `tests/ui/test_geometric.cpp` (each criterion deciding alone;
  ties; empty set; all four directions on hand-drawn layouts). Covers FR-16.
- [ ] **T3.3** Navigation: `ui_navigate.cpp` (the widget's own use, neighbours, ancestor boxes along their axis, grid
  boxes by columns, wrap on a fresh press only, wrap on repeat option, the geometric rule, blocked; cross-root only by
  neighbours). Test: `tests/ui/test_navigate.cpp` (every container kind; neighbours win; sliders/choices/toggles
  consume or pass; wrap press vs repeat; blocked keeps focus; roots). Covers FR-16.
- [ ] **T3.4** Default and remembered focus: entering containers (last focused if a target, else default, else first),
  the first focus (`first_focus` root), lists remembering item and scroll, focus by key after a rebuild. Test:
  `tests/ui/test_remembered_focus.cpp`. Covers FR-17.
- [ ] **T3.5** Modals: `ui_modal.cpp` (open/close, depth ≤ 8, scope of navigation/confirm/cancel/touch, focus restored,
  activation on open). Test: `tests/ui/test_modal.cpp` (scope; nesting to 8; 9 asserts; restore; outside touch ignored
  or cancelling — with T5.3). Covers FR-18.
- [ ] **T3.6** Stage end: the stage-end guards. Files: plan.md ("Stage 3 results"). Covers AC-7.

## Stage 4: input, values, results, scene binding

- [ ] **T4.1** Repeat and suppression: `ui_repeat.{hpp,cpp}` (pure: per-control repeat at press, `delay`, `delay +
  k × interval`; newest direction wins, ties Up, Down, Left, Right; delay 0 = the input's own repeats; suppression until
  release). Test: `tests/ui/test_repeat.cpp` (frame-exact fire lists for 24/6, 1/1, 255/255; suppression). Covers FR-22.
- [ ] **T4.2** The default source and activation: `ui_input_raw.cpp` (public input queries: D-Pad, Circle Pad with
  0.5/0.4 hysteresis, confirm/cancel/page buttons from the config, touch; activation on first update, `reset_input`, a
  `frame_count()` gap > 1). Test: `tests/ui/test_input_raw.cpp` (the fake's scripted samples; A/B swapped; the Circle
  Pad around the thresholds; `input::set_blocked` → idle; a transition's block → idle), `tests/ui/test_no_leak.cpp`
  (held confirm at the first update, after `reset_input`, after a frame gap, after a modal opens). Covers FR-22.
- [ ] **T4.3** Values: `ui_values.cpp` (slider ± step with repeat and clamping; toggle flips and left/right sets;
  choice ±1 with wrap per widget, confirm = next; `set_value` reports nothing). Test: `tests/ui/test_values.cpp`.
  Covers FR-6.
- [ ] **T4.4** Results and callbacks: `ui_results.cpp` (the buffer, stable sort into the documented order, drops
  counted and asserted, per-key queries, function slots, callbacks after the sort; a callback's changes laid out before
  `update()` returns; destroying/moving the UI from a callback asserts). Test: `tests/ui/test_results.cpp`,
  `tests/ui/test_callbacks.cpp` (a callback that creates, destroys and hides widgets: the same frame's `draw()` shows
  it). Covers FR-20, FR-32.
- [ ] **T4.5** The update order and cancel: `ui_update.cpp` (plan.md's 13 steps; confirm; cancel → back widget, modal
  close, `Cancelled`). Test: `tests/ui/test_cancel.cpp`, `tests/ui/test_input_custom.cpp` (`update(input)`, a source
  function, `set_input_source(null)` back to raw). Covers FR-21, FR-22.
- [ ] **T4.6** Scene binding: `ui_scene.cpp` (D30: `BeforeUpdate` → `update()`, `AfterExit` → `reset_input()`). Test:
  `tests/ui/test_scene_binding.cpp` (updated before the scene's `update()`; frozen under a pushed pause scene; held A
  after the pop ignored; reset after exit with focus memory kept; `update()` on a bound UI asserts; moved and destroyed
  while bound). Covers FR-17, FR-21, FR-22.
- [ ] **T4.7** [P] The actions adapter: `ui_actions.cpp` (`Ui::use_actions`: held/pressed from the set's actions, a
  vector action as four directions past 0.5/0.4, touch raw). Test: `tests/ui/test_actions_adapter.cpp` (remapped
  confirm; vector; unbound actions read idle). Covers FR-22.
- [ ] **T4.8** [P] Bus events: `ui_events.cpp` (`set_events`; the four structs ≤ 16 B; published at the end of the
  update). Test: `tests/ui/test_events.cpp` (off by default: `events::stats()` unchanged; on: delivered at the next
  dispatch with keys and values). Covers FR-20.
- [ ] **T4.9** Stage end: the stage-end guards. Files: plan.md ("Stage 4 results"). Covers AC-7.

## Stage 5: lists, grids, scroll boxes, touch

- [ ] **T5.1** Lists and grids: `ui_list.cpp` (focused item, whole-row scroll with margins, pages by visible rows,
  wrap, count changes clamping, `set_scroll`/`set_item`/`list_info`, the row ring and fill calls only for rows coming
  into view or marked changed, disabled rows skipped or refused). Test: `tests/ui/test_list.cpp` (65 535 items; fill
  counters; 20 vs 10 000 equal work), `tests/ui/test_grid.cpp` (four directions by columns, last partial row). Covers
  FR-11, FR-15.
- [ ] **T5.2** Scroll boxes: `ui_scroll_box.cpp` (the window of whole children, reveal on focus with margins,
  translation without relayout, `set_scroll`). Test: `tests/ui/test_scroll_box.cpp` (`layout_visits` unchanged by
  scrolling; children outside not drawn and not touchable). Covers FR-12.
- [ ] **T5.3** Touch: `ui_touch.cpp` (hit test, press and pressed look, release inside/outside, slider drag, choice
  arrows, list taps and `tap_selects_first`, the 8 px threshold and row scrolling, inert → refused, modal outside
  touches, cursor hiding). Test: `tests/ui/test_touch.cpp` (each gesture; 8 vs 9 px; top-root touches ignored; the
  D-Pad continuing from the touched widget). Covers FR-19, FR-18.
- [ ] **T5.4** Stage end: the stage-end guards. Files: plan.md ("Stage 5 results"). Covers AC-7.

## Stage 6: styles, drawing, cursor, prompt, debugging

- [ ] **T6.1** Styles and themes: `ui_style.cpp` (`ui::default_theme()`: solid colours, no texture; lookup by kind and
  `UiStyleId`; `add_style`/`set_style_value`; metric changes relayout, colour changes don't; texture slots). Test:
  `tests/ui/test_style.cpp` (state precedence; overrides; metrics; the default theme's text contrast ≥ 4.5 on its
  backgrounds), `tests/ui/test_style_data.cpp` (`NNH_DATA_STRUCT`/`NNH_DATA_TYPE` over every style struct compiles under
  a test key; a value converted by hand applies). Covers FR-26.
- [ ] **T6.2** The drawing walk: `ui_draw_out.{hpp,cpp}` (the only gfx caller; the draw hash and count), `ui_draw.cpp`
  (roots, pre-order, offsets and integer opacity, scroll windows, modals and the cursor last, hidden roots, layer,
  `ignore_camera`). Test: `tests/ui/test_draw.cpp` (items read from the fake: layers, call order, unaffected by the
  camera's position and zoom 0.5/2, hidden roots, outside a frame nothing recorded but the hash computed, draw counts
  equal gfx's, missing glyphs as the hollow box), `tests/ui/test_draw_equivalence.cpp` (an options-like screen's items
  equal the items of the same gfx calls written in the test). Covers FR-28, FR-7.
- [ ] **T6.3** Per-kind drawing: `ui_draw_widgets.cpp` (text runs with alignment, icons, toggle images, slider track,
  fill and knob, choice arrows or text arrows, progress fill both directions and its text, list rows with icons and
  number columns, the scroll bar from `list_info`, 9-slice backgrounds). Test: `tests/ui/test_draw_widgets.cpp`
  (hand-computed items for each kind). Covers FR-5, FR-6, FR-25, FR-26.
- [ ] **T6.4** [P] The cursor: `ui_cursor.cpp` (eased move over N frames rounded once, pulse, outline, pressed look for
  6 frames, hidden after touch). Test: `tests/ui/test_cursor.cpp` (positions per frame pinned for CubicOut over 6
  frames; pulse cycle; the pressed countdown). Covers FR-27, FR-29.
- [ ] **T6.5** [P] The prompt: `ui_prompt.cpp` (`prompt`, `close_prompt`: a panel, a wrapping label, an HBox of 1..3
  buttons, centred in a stack on the screen's root; opened modally; default and cancel answers; close on answer). Test:
  `tests/ui/test_prompt.cpp`. Covers FR-8.
- [ ] **T6.6** [P] Debugging: `ui_debug.cpp` (`debug_draw` of rectangles and the focus path, `dump` of the tree to the
  log; Release: empty), stats. Test: `tests/ui/test_debug.cpp` (the dump's exact text for a small tree; debug items;
  stats; pc-release: nothing logged). Covers FR-30.
- [ ] **T6.7** Misuse, cost, random operations: Test: `tests/ui/test_ui_misuse.cpp` (every FR-32 case of plan.md asserts
  and is harmless on the Release path), `tests/ui/test_ui_cost.cpp` (allocation counter unchanged over 100 000
  operations; an idle update visits no node; list cost independent of the count), `tests/ui/test_ui_random_ops.cpp`
  (100 000 seeded operations with the invariants of plan.md). Covers FR-31, FR-32.
- [ ] **T6.8** Stage end: the stage-end guards; `compare.sh` (every reference identical). Files: plan.md ("Stage 6
  results": bytes per widget, row, style, function slot; the default theme's colours). Covers AC-7, FR-1.

## Stage 7: the golden session, `examples/ui`, the bench, the ARM pre-check

- [ ] **T7.1** The self-check (AC-2): `examples/ui/self_check.{hpp,cpp}` (plan.md's UI, the 3600-frame `kUiScript`,
  the running hash and the checkpoints); goldens from pc-debug in `kUiCheckGolden`/`kUiCheckpoints`, equal in
  pc-release. Test: `tests/ui_example/test_self_check.cpp` (on the fake platform; a one-value change in the script fails
  it — checked once by hand). Files: those, `tests/CMakeLists.txt` (028's block). Covers AC-2, FR-29.
- [ ] **T7.2** [P] Art and themes: `examples/ui/art/make_art.py` → `assets/art/ui.png` (128 × 128, original),
  `themes.{hpp,cpp}` (the 9-slice theme). Test: the PNG read back; `test_style.cpp` loads the theme's regions inside the
  texture. Covers AC-3, IX.
- [ ] **T7.3** The showcase: `main.cpp`, `demos.hpp`, `names.{hpp,cpp}`, `texts.{hpp,cpp}` (two languages through the
  resolver), `items.{hpp,cpp}`, `background.{hpp,cpp}`, `hud.{hpp,cpp}`, `title_scene`, `options_scene` (tween slide-in
  by setter), `inventory_scene`, `gallery_scene`, `dialog_scene`, `CMakeLists.txt` (`ASSETS assets`),
  `examples/CMakeLists.txt` (after `transitions`). Test: PC screenshots `UI_SHOT=title|options|list|grid|dialog|modal|
  theme2` read back; the log lines of plan.md. Covers AC-3, FR-17 (focus kept across pushes with no code).
- [ ] **T7.4** The check scene: `check_scene.{hpp,cpp}` (Y or `UI_DEMO=check`: 10 script frames per display frame, the
  lines `ui: check started`, `ui: <frame> sum <S>`, `ui: PASS <G>` / `ui: FAIL ...`); ctest `ui_self_check_pc`. Test:
  the ctest passes in pc-debug and pc-dev. Covers AC-4, FR-29.
- [ ] **T7.5** [P] The bench: `bench.{hpp,cpp}` (idle128, move128, geo128, relayout128, list10000/list20, options),
  `hand_options.{hpp,cpp}` (the same gfx calls written out from the UI's queries), `bench_scene.{hpp,cpp}` (X or
  `UI_DEMO=bench`: the lines and verdict of plan.md; Debug `not judged (Debug)`). Test: a pc-dev run logs every line;
  `test_draw_equivalence.cpp` gains the example's options screen against `hand_options`. Covers AC-6.
- [ ] **T7.6** The script `examples/ui/tests/check.nnhs` (plan.md's outline: every expect followed by `wait 10` before
  the next input, one `expect` per frame, the touch coordinates of the layout) and ctest `ui_check_script_pc`
  (`RESOURCE_LOCK nnh_remote_port`). Test: `nnh run ui --pc --no-build --script examples/ui/tests/check.nnhs` → PASS,
  twice in a row. Files: plan.md ("Stage 10: console run (prepared)" completed with every expected line). Covers AC-4.
- [ ] **T7.7** ARM11 pre-check (before any console run): `arm-none-eabi-objdump -drC` of the 3ds-dev objects
  `src/ui/*` and the example's `bench.o`; per AC-6 path record instructions, calls, `pld`, VFP use (none in the idle
  update and layout but the documented ones), `__aeabi_*` (only the stretch shares and the slider drag); `-fstack-usage`
  of `src/ui/*` along the deepest update and draw paths (< 1 KiB); compare with plan.md's table and fix what differs
  without changing a rectangle or the golden. Files: plan.md ("Stage 7 ARM pre-check": the table and the expected
  console numbers). Covers AC-6, II.

## Stage 8: mutation testing, opt-in checks, build times

- [ ] **T8.1** Mutation pass as in plans 019–027: plan.md's planned hand mutants (`build/ref-028/mutants.py`, applied one
  at a time by `mutate.py`, the whole `nnh_tests` each) and Release-path mutants in pc-release; survivors become tests
  or are argued equivalent. Files: plan.md ("Mutation testing"). Covers VI.
- [ ] **T8.2** [P] Release strings: `scripts/check_release_strings.sh` gains the dump's and the misuse texts (one
  `forbidden=` line); run on the 3ds-release and pc-release `ui` executables. Covers FR-30, FR-32.
- [ ] **T8.3** [P] Opt-in by linking: probes (`build/ref-028/probe_*`, not committed) linked against the 3ds-release
  libraries: a UI game that never calls `use_actions` has no `ActionSet` symbol (map file); without `prompt` no
  `ui_prompt.o`; without `set_events` no event code; `hello` sizes and `nm` byte-identical to T0.1's. Files: plan.md
  ("Stage 8 results"). Covers FR-33, AC-7.
- [ ] **T8.4** Build times and regressions: every example's console script passes with `--pc`; `compare.sh` (sprites,
  text, actions, transitions, platformer pixel-identical); edit `examples/ui/title_scene.cpp` → simulator and `.3dsx`
  times against T0.1's reference and constitution III; clean `nnh_core` time. Files: plan.md ("Stage 8 results").
  Covers AC-7, III.

## Stage 9: Linux checks (VM 120, run by the lead)

- [ ] **T9.1** On `nnhge-build` (start it per CLAUDE.md, never with the Bazzite VM; shut it down after): GCC 13, GCC 14
  and clang 18 (ASan/UBSan): `nnh_tests` and ctest pass; AC-2's checkpoints identical on all three; the 32-bit host
  build (`-m32 -msse2 -mfpmath=sse`) of `nnh_tests`: the UI tests and AC-2 identical; ASan/UBSan clean over the random
  operations, touch, lists and layout. Files: tasks.md (results), plan.md ("Stage 9 results"). Covers I, X, FR-29, AC-1,
  AC-2.

## Stage 10: console check (the only steps on the 3DS)

- [ ] **T10.1** (on the PC) Builds and checks: `scripts/dkp.sh cmake --build --preset 3ds-release` and `scripts/dkp.sh
  cmake --build --preset 3ds-dev --target ui_3dsx platformer_3dsx` without a warning; T7.7's table still true;
  `hello.elf` byte-identical to T0.1's. Covers AC-7, II.
- [ ] **T10.2** Old 3DS, nobody touching it after Y. Three runs, each: Homebrew Launcher, press **Y**, then on the PC:
  1. `build/pc-debug/tools/nnh/nnh run ui --script examples/ui/tests/check.nnhs` → every checksum line equal to the
     PC's and `ui: PASS <G>`, every showcase line, the bench lines and the verdict (numbers recorded); screenshots read
     back against the `--pc` run's (AC-5);
  2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs` → `bench: PASS 4 layers +
     300 sprites, …`;
  3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` → the tree's replay
     lines and `60 Hz PASS`.
  Files: plan.md ("Console run 1": verdicts, bench numbers, log paths). Covers AC-4, AC-5, AC-6.
- [ ] **T10.3** (only if a bench target is missed) Optimisation round: find the cost (3ds-dev disassembly, the bench's
  numbers), apply risk 1's list without changing a rectangle, the golden or the draw hash (tests first for any rewrite);
  then T10.2 run 1 again; if still missed, the budget is set from the measurement with ≈ 25 % margin and written in the
  spec's AC-6, `bench_scene.cpp` and `docs/api/ui.md`. Files: plan.md ("Optimisation round", "Console run 2 and budgets
  set"). Covers AC-6.
- [ ] **T10.4** (contingency, only if the frame target or the draw ratio is still missed after T10.3, and with the
  maintainer's OK) A UI glyph batch through D39's mechanism (plan.md risk 2): the design written into plan.md and D39's
  row first (an amendment), then tests (items equal to the glyph sprites `draw_text` would make; budgets), then code;
  every reference screenshot unchanged. Covers AC-6, II.

## Stage 11: docs, roadmap, status

- [ ] **T11.1** `docs/api/ui.md` (the Old 3DS costs added after T10.2): creating a UI and its capacities, widgets,
  layout and anchors, both screens, focus rules, remembered focus, modals, touch, input sources and actions, results and
  callbacks, events, text sources and the fonts/localization seam, styles, themes and 9-slice, the cursor, drawing and
  layers, scene binding, determinism, debugging, costs, misuse; header comments (allocates? per-frame safe? misuse?).
  Covers VIII, AC-8.
- [ ] **T11.2** `docs/guides/menus.md`: title menu, options, inventory, dialog, prompt, HUD, animating with tweens (a
  setter on `set_render_offset`), sounds from results, tabs with L/R pages and hidden panels, a theme in a data file
  (the `test_style_data.cpp` pattern). Covers AC-8.
- [ ] **T11.3** Other pages: `docs/api/gfx.md` (UI on layer 14 below the transition overlay), `text.md` (the UI's text
  seam), `actions.md` (`use_actions`), `tween.md` (animating widgets by setter), `docs/architecture/patterns.md` (the
  "Retained widget tree" row: implemented), `scenes.md` (the UI as a D30 user), `rendering.md` (the layer convention),
  `docs/REFERENCES.md` (Godot, Unity uGUI, Clay, microui, Dear ImGui, console conventions), `CLAUDE.md` (a layout
  paragraph after "3D foundation", the `ui` commands after the models line); `python -I scripts/docs_tool.py check` → 0
  broken links. Covers AC-8.
- [ ] **T11.4** `docs/architecture/decisions.md`: D48 change-log note "implemented" with the stage refinements and the
  maintainer's answers to plan.md's questions. Covers AC-8.
- [ ] **T11.5** Final measurements (header cost, build times, `hello` sizes, the module's `.text`) in plan.md and
  `docs/ROADMAP.md`'s baseline table if it changed. Covers III.
- [ ] **T11.6** `docs/ROADMAP.md`: the "UI widgets" checkbox, with the implemented summary.
- [ ] **T11.7** spec, plan, tasks and `specs/README.md` status → implemented (or "implemented, console T10.2 pending").
- [ ] **T11.8** (the lead) Merge `spec-028` into `main` (rebased; insertions at plan.md's anchors); then the full ctest
  (pc-debug, pc-release), every example's `--pc` script, AC-2's golden and every earlier golden, one 3ds-release build;
  T10.2 run 1 repeated if the merge happened after it.
</content>
</invoke>
