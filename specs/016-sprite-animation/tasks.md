# Tasks 016: Sprite animation

Status: implemented (console check pending: T10.2) — approved (2026-11-22, maintainer pre-approved)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task is one or a few TDD loops. `[P]` means the task can be done in parallel with the previous one. Every
stage ends with something verifiable on PC (`ctest --preset pc-debug`, a screenshot, a `--pc` script). The only
console step is T10.2, a scripted check where the maintainer only starts the game (Homebrew Launcher → Y).
Deviations found while building go into plan.md as "Stage N refinements", as in plan 012.

## Stage 0: baseline, the clock and the name hash

- [x] **T0.1** Baseline numbers (Windows, 3 runs each): `hello.exe` and `hello.elf`/`.3dsx` sizes (`size`); edit
  `examples/hello/main.cpp` → simulator and `.3dsx` build times; clean engine build; the preprocessed line count and
  compile time of a one-line TU including `<nnh/events.hpp>` (the future `animation.hpp` comparison). Files: plan.md
  ("Stage 0 results"). Covers III, AC-9.
- [x] **T0.2** `Frames`, `FrameTime`, `kMaxFrameTime` in `<nnh/time.hpp>`, exactly as plan 015 specifies (skip if
  spec 015 already added them there; see plan 015's amendment). Files: `engine/core/include/nnh/time.hpp`. Test:
  `tests/time/test_frame_time.cpp` (1 s = 245 760 ticks, 0.5 s, 16.67 ms = 4096, 100 ms = 24 576, 1 ms, 0,
  negative and > 1 h invalid, `Frames{n}`, `duration()` round trip). Covers FR-3, FR-14.
- [x] **T0.3** [P] `detail::fnv1a32` (constexpr). Files: `engine/core/include/nnh/detail/name_hash.hpp`. Test:
  `tests/core/test_name_hash.cpp` (published FNV-1a vectors, `""`, usable in a `case` label). Covers FR-5, FR-8.

  Stage end: ctest green; `hello` sizes identical to T0.1.

## Stage 1: the library block and frames (pure)

- [x] **T1.1** Format and layout: `Header` and record structs with `static_assert` sizes (16/52/12/8/40/4 B), limits,
  enums; capacities → section offsets and total. Files: `engine/core/src/animation/sprite_format.hpp`,
  `sprite_layout.{hpp,cpp}`, `engine/core/CMakeLists.txt`. Test: `tests/animation/test_sprite_layout.cpp` (offsets
  4-byte aligned, totals for the plan's examples, ceilings). Covers FR-5.
- [x] **T1.2** `SpriteLibrary` create/destroy/move: one `mem::alloc` (tag `animation`), required capacities and
  ceilings, log channel `animation`, serials and the Debug/Development live table, `memory_used`. Files:
  `nnh/animation.hpp` (library part), `sprite_library_internal.hpp`, `sprite_library.cpp`. Test:
  `tests/animation/test_sprite_library.cpp` (tag stats, freed, move keeps the block, absurd capacities → error).
  Covers FR-5.
- [x] **T1.3** Frames: `add_grid` (margin, spacing, columns or image width, row order), `add_frame` (regions, trim
  offset, source size), default pivots (top-left, centre, bottom-centre at 1/16 px), explicit pivots, every FR-2
  error naming the frame, `frame(i)`. Files: `sprite_frames.cpp`. Test: `tests/animation/test_sprite_frames.cpp`.
  Covers FR-1, FR-2, FR-11.

  Stage end: ctest green on PC and VM 120; `3ds-release` builds (32-bit record sizes).

## Stage 2: clips and sequences (pure)

- [x] **T2.1** The sequence math: cycle length, position → clip index for forward/reverse/ping-pong(-reverse), end
  step and cycle for once/repeat/finite passes, one- and two-frame clips. Files: `sprite_sequence.{hpp,cpp}`. Test:
  `tests/animation/test_sprite_sequence.cpp` (FR-4's tables, written out frame by frame). Covers FR-4.
- [x] **T2.2** `add_clip`: names (1..31 bytes, duplicates, hash collisions), frame lists and ranges, repeated frames,
  default and per-frame durations (`Duration` rounded once, `Frames`), modes, `repeat`, `pingpong_passes`, cycle
  duration; `find_clip`, `clip_name`, `clip_length`, `clip_duration`. Files: `sprite_clips.cpp`, `animation.hpp`.
  Test: `tests/animation/test_sprite_clips.cpp`. Covers FR-3, FR-5.

  Stage end: ctest green.

## Stage 3: the animator: playing, the clock, drawing (pure)

- [x] **T3.1** The animator value: layout (`static_assert` ≤ 64 B on 32-bit, ≤ 96 B otherwise), constructors, the
  default animator, play (same/restart/finished)/play by name/play_from/stop/pause/resume/set_frame/speed (rounding,
  0, negative and > 64 rejected)/flip, lifetime asserts. Files: `animator_internal.hpp`, `animator.cpp`,
  `animation.hpp`. Test: `tests/animation/test_animator_play.cpp`; `test_sprite_lifetime.cpp` (use after destroy
  asserts, via `platform_fake::run_expecting_crash`). Covers FR-5, FR-6.
- [x] **T3.2** Advancing: pending entry, `update`/`advance` with speed and remainder, crossings with carry, end of
  clip and hold, `Looped`/`Finished` counting (no callbacks yet), the queue (once, looping wrap, full, on a finished
  animator), the crossing bound (Debug assert; Release-path jump tested where asserts are off, as
  `tests/events/test_overflow.cpp` does). Files: `animator_advance.cpp`. Test: `test_animator_sequences.cpp` (FR-4
  through the animator), `test_animator_clock.cpp` (AC-2: 240, 6 updates per 100 ms, speed 0.5, 1000 loops of a
  non-whole `Duration`, two identical runs). Covers FR-4, FR-6, FR-7, FR-14.
- [x] **T3.3** [P] Queries and drawing: position, frame, region, pivot (offset and flips), offset, time in frame,
  progress, loops, `draw_options()`; the library's `draw_options(i)`. Files: `animator_queries.cpp`,
  `sprite_frames.cpp`. Test: `tests/animation/test_animator_draw.cpp` (trimmed vs untrimmed vs flipped land on the
  same pixels: world rectangles of every opaque pixel from the fake platform's recorded draw items). Covers FR-6,
  FR-10, FR-11.

  Stage end: ctest green on PC and VM 120; `3ds-release` builds (the 64 B assert).

## Stage 4: markers, notices and re-entrancy (pure)

- [x] **T4.1** `add_marker`: sorted insert, per-frame cap 8, value, names hashed, hash collision between different
  names → error. Files: `sprite_markers.cpp`. Test: `tests/animation/test_sprite_markers.cpp`. Covers FR-8.
- [x] **T4.2** Notices: the scratch, the order rules (end of old position, state change, markers of the entered
  frame), the callback (`SpriteCallback`, 16-byte captures), the returned view, pending entry after play, nothing
  while paused. Files: `animator_notices.cpp`, `animator_advance.cpp`. Test:
  `tests/animation/test_animator_notices.cpp`; `tests/animation/compile_fail_callback_capture.cpp` (ctest expecting a
  compile failure, like the bus's). Covers FR-8.
- [x] **T4.3** Re-entrancy: commands from a callback on the same animator (drop old notices and the rest of the time,
  new entry in the same update), depth 8 with one logged error, other animators' commands, `update()` from a callback
  (assert / Release log). Files: `animator_notices.cpp`, `animator.cpp`. Test:
  `tests/animation/test_animator_reentrancy.cpp`. Covers FR-9.

  Stage end: ctest green on PC and VM 120.

## Stage 5: the state machine (pure)

- [x] **T5.1** States and rules in the library: `add_state`, `set_next_on_finish`, `add_transition`, `find_state`,
  `state_name`, errors; rule resolution (from→to, from→any, any→to, any→any, default Now) as a pure function. Files:
  `sprite_states.{hpp,cpp}`. Test: `tests/animation/test_sprite_states.cpp`. Covers FR-12, FR-13.
- [x] **T5.2** The animator's machine: start state, `request` (current = nothing and clears pending; Now, AtEnd at
  end of clip or loop with carry; Never), keep-time, pending vs next-on-finish, `force`, `StateChanged`, previous
  state, time in state, `play`/`queue` leaving the machine. Files: `animator_state.cpp`, `animator_advance.cpp`.
  Test: `tests/animation/test_animator_states.cpp`. Covers FR-12, FR-13.
- [x] **T5.3** No allocation while playing: the counting `operator new`/`delete` (`tests/framework/alloc_counter.*`,
  shared with spec 015; create it if absent) plus `mem::live_allocations()` around play, queue, update, markers,
  callbacks and state switches on 512 animators; the leak report is clean after destruction. Files:
  `tests/animation/test_sprite_alloc.cpp`. Covers FR-15, AC-3.

  Stage end: ctest green on PC and VM 120 (ASan/UBSan).

## Stage 6: loading `.nnhsprite` (fake platform)

- [x] **T6.1** The bounded validator: header, version (newer → `Unsupported`), sizes and CRC, capacities ≤
  ceilings, counts ≤ capacities, every index and range, enums, durations, names NUL-terminated and matching their
  hashes, no duplicate hashes, markers per frame, ends consistent with modes. Files: `sprite_validate.{hpp,cpp}`.
  Test: `tests/animation/test_sprite_validate.cpp` (one corruption per rule, from a code-built block written by
  `tests/animation/sprite_file.{hpp,cpp}`, a test-only writer). Covers FR-16, X.
- [x] **T6.2** Copy with extras and `load_sprite_library`: asset `<path>.nnhsprite`, validate, copy into a block with
  the options' extra capacities, serial, profile scope, errors (`Corrupted`, `Unsupported`, `LimitReached`,
  `OutOfMemory`). Files: `sprite_copy.{hpp,cpp}`, `sprite_load.cpp`. Test: `tests/animation/test_sprite_load.cpp`
  (fake assets; markers, states and rules added within extras, FR-18), `test_code_vs_file.cpp` (same frames and
  notices). Covers FR-16, FR-18.
- [x] **T6.3** [P] Fuzz target `sprite_library_target` (validate; on success load from memory and play 30 frames of
  every clip and state). Files: `tests/fuzz/fuzz_sprite.cpp`, `libfuzzer_sprite.cpp`, `fuzz_targets.{hpp,cpp}`,
  `test_fuzz_smoke.cpp`, `tests/CMakeLists.txt`. Test: mini-fuzz (2 × 4000 mutants) in `nnh_tests`; `fuzz_sprite`
  for 10 minutes on VM 120 (clang 18, ASan). Covers FR-16, X.

  Stage end: ctest green on PC and VM 120; fuzz run clean.

## Stage 7: the Aseprite converter (`nnh-assetc sprite`)

- [x] **T7.1** JSON line numbers: `Node::line`, `Ref::line()`, `Problem::line`; the CLI prints `file:line: reason`
  when a line is known (Tiled messages unchanged). Files: `tools/assetc/src/json.hpp`, `json_parse.cpp`,
  `convert_context.hpp`, `cli.cpp`. Test: `tools/assetc/tests/test_json.cpp` (lines of values, keys, nested),
  existing Tiled tests unchanged. Covers FR-17.
- [x] **T7.2** Frames and meta: Hash and Array layouts in file order, regions inside `meta.size`, `rotated: true`
  refused, trim (`spriteSourceSize`, `sourceSize`) → offset and source size, durations 1..65 535 ms → ticks (100 ms =
  24 576), `meta.image` = `<name>.png` and `meta.size` = PNG header. Files: `tools/assetc/src/sprite_model.hpp`,
  `aseprite_frames.{hpp,cpp}`. Test: `tools/assetc/tests/test_aseprite_frames.cpp` (in-memory files,
  `aseprite_fixture.{hpp,cpp}`). Covers FR-17.
- [x] **T7.3** Tags and slices: clips from tags (`from`/`to` ranges, every direction, `repeat` as string or number →
  Once/Repeat/passes, absent → loop), names and duplicates, `@name`/`@name=value` markers on every clip showing those
  frames, the `default` clip, the `pivot` slice (key pivot + bounds origin, or bounds centre-bottom), other slices
  ignored, `color`/`data` ignored. Files: `aseprite_tags.{hpp,cpp}`, `aseprite_slices.{hpp,cpp}`. Test:
  `test_aseprite_tags.cpp`, `test_aseprite_slices.cpp`. If the maintainer can export a real sprite from Aseprite, add
  it to `tests/data/aseprite/real/` and check it converts as expected (closes the slice-pivot risk). Covers FR-4,
  FR-17.
- [x] **T7.4** Writer and whole conversion: model → `.nnhsprite` (capacities = counts, CRC), validated by core's
  `validate_sprite_library`; `convert_sprite` from a `FileSource`; the `sprite` CLI command with `--assets` and
  `--depfile` (JSON + PNG); version bump; mini-fuzz bodies and `fuzz_assetc_sprite`. Files:
  `sprite_writer.{hpp,cpp}`, `convert_sprite.{hpp,cpp}`, `cli.cpp`, `tools/assetc/CMakeLists.txt`,
  `tests/fuzz_assetc.*`, `tests/libfuzzer_assetc_sprite.cpp`. Test: `test_sprite_writer.cpp` (a converted fixture
  loads in core and plays the expected frames), `test_cli.cpp` (sprite command, depfile contents),
  `test_fuzz_assetc.cpp`; `fuzz_assetc_sprite` 10 minutes on VM 120. Covers FR-16, FR-17, X.
- [x] **T7.5** Build integration: `.ase.json` → `.nnhsprite` step with depfile in `nnh_sprites/`, `.ase`/`.aseprite`
  not packed, converter needed for `.tmj` or `.ase.json` (configure message names the kind); fixtures
  `tests/data/aseprite/` (`make_fixtures.py`, good and `bad_*.ase.json` with PNGs) and one ctest per bad case expecting
  its `file:line: reason`. Files: `cmake/nnh_assets.cmake`, `cmake/nnh_host_tools.cmake`, `tests/CMakeLists.txt`,
  `tests/data/aseprite/*`. Test: the ctests; a 3DS configure of `hello` doesn't build `nnh-assetc`; editing one
  export rebuilds only that step (Ninja `-d explain`, time recorded in plan.md). Covers FR-17, FR-19, AC-4, AC-9.
- [x] **T7.6** Coordination with spec 014 (D29, D31). If spec 014 is already implemented: put the `.ase.json` match
  before the data step in `nnh_assets.cmake`, add `.ase.json`/`.ase`/`.aseprite` to its owned list and `nnhsprite`,
  `ase`, `aseprite` to the keys the schema program refuses, and add a test that a schema game's `hero.ase.json` is
  converted as a sprite, not as data. If not, there is nothing to do now: D31 and its change-log note carry the rule
  to spec 014's implementer. Files: as above. Covers FR-17, XI.

  Stage end: ctest green on PC and VM 120; `3ds-release` builds a game with an export (host converter built once).

## Stage 8: the example `examples/animation` (PC) and the platformer

- [x] **T8.1** Art and data: `art/make_art.py` writes `gallery.png`, `hero.png` + `hero.ase.json` (Aseprite Hash
  format: trimmed frames, idle/run/jump/fall/attack/hurt tags, `@step`, `@hit=2`, a `pivot` slice), `sfx/step.wav`;
  outputs committed. Files: `examples/animation/art/make_art.py`, `examples/animation/assets/**`. Test: the build
  converts it; `nnh-assetc sprite` on it by hand matches the expected clip list. Covers AC-5, FR-17.
- [x] **T8.2** The self-check as pure code: 64 animators, the fixed 600-frame script, FNV-1a checksum of frames and
  notices logged every 60 frames, `animation: PASS <checksum>` / `animation: FAIL ...` against a golden constant from
  the simulator. Files: `examples/animation/self_check.{hpp,cpp}`. Test: `tests/animation_example/test_self_check.cpp`
  (compiled from the example, as `tests/platformer`; also on VM 120). Covers AC-6, FR-14.
- [x] **T8.3** The showcase scene: top gallery (six modes, counters, speed slider), bottom hero on the state machine
  (run, jump, uninterruptible attack, hurt that interrupts, `@step` sound and dot, `@hit` box), one library in code and
  one loaded. Files: `examples/animation/main.cpp`, `showcase_scene.{hpp,cpp}`, `gallery.{hpp,cpp}`,
  `hero.{hpp,cpp}`, `CMakeLists.txt`, `examples/CMakeLists.txt`. Test: PC screenshots read from the PNG
  (`NNH_EXIT_AFTER_FRAMES=120 NNH_SCREENSHOT=build/animation.png`, a deterministic frame). Covers AC-5.
- [x] **T8.4** [P] Self-check and bench scenes (`ANIMATION_DEMO=check|bench`, SELECT cycles showcase → check →
  bench): the bench runs 512 animators inside `NNH_PROFILE_SCOPE`, logs avg/worst against 0.5 ms and shows the
  animator size and library bytes. Then the script `examples/animation/tests/check.nnhs`. Files:
  `self_check_scene.{hpp,cpp}`, `bench_scene.{hpp,cpp}`, `tests/check.nnhs`. Test: `nnh run animation --pc --script
  examples/animation/tests/check.nnhs` passes. Covers AC-6, AC-7.
- [x] **T8.5** The platformer: `PlayerSprite` (library in code, idle/run/jump/fall states, `step` marker → dust
  puff), `Player::velocity()`/`facing_left()`, `walk_frames_` and `Player::draw` removed, `LevelPlay` draws through
  the animator. Files: `examples/platformer/player_sprite.{hpp,cpp}`, `player.{hpp,cpp}`, `level_play.{hpp,cpp}`,
  `CMakeLists.txt`, `tests/CMakeLists.txt`. Test: `tests/platformer/test_player_sprite.cpp` (state chosen per
  physics case, footstep on run frames, flip); the existing platformer tests unchanged and green;
  `platformer_replay_matches_script` green; `nnh run platformer --pc --script` with `gate.nnhs` and `level.nnhs`; a
  screenshot of the player running (`PLATFORMER_DEMO=replay`, a frame mid-run) read from the PNG. Covers AC-8.

  Stage end: both PC scripts pass; screenshots checked; `3ds-release` builds `animation.3dsx` and `platformer.3dsx`.

## Stage 9: opt-in and size checks

- [x] **T9.1** Release strings: `scripts/check_release_strings.sh` on the animation example's Release ELF (no debug
  strings, no live-serial table). Files: none. Covers X.
- [x] **T9.2** [P] Header cost: preprocessed lines and compile time of a one-line TU with `<nnh/animation.hpp>`
  against T0.1's `<nnh/events.hpp>` number. Files: plan.md ("Stage 9 results"). Covers III.
- [x] **T9.3** Opt-in: `hello.exe`/`hello.elf` sizes equal to T0.1, no `Sprite`/`animation` symbols in `nm`; `examples/
  scenes`, `sprites`, `text` unchanged; build times against T0.1. Files: plan.md. Covers FR-19, AC-9, III.

## Stage 10: console check (the only step on the 3DS)

- [x] **T10.1** Build the Development flavour that `nnh run` uses and the Release `.3dsx`; the example logs
  `sizeof(SpriteAnimator)` (64) at start. Files: plan.md. Covers II.
- [ ] **T10.2** On the Old 3DS: the maintainer opens the Homebrew Launcher and presses Y, then runs
  `nnh run animation --script examples/animation/tests/check.nnhs`. It passes when the saved log in `build/logs/` has
  `animation: PASS <checksum>` with the PC's checksum and the bench line for 512 animators (target < 0.5 ms). Then
  `nnh run platformer --script examples/platformer/tests/level.nnhs` and `gate.nnhs` still pass. The bench number
  goes into plan.md and the docs. Covers AC-5, AC-6, AC-7, AC-8, FR-14.

## Stage 11: docs, roadmap, status

- [x] **T11.1** `docs/api/animation.md`: sheets and frames, clips and modes with FR-4's sequences (including
  Aseprite's ping-pong passes), the animator, notices (order, callback, returned view and its lifetime, re-entrancy,
  the bounds), drawing with trim and flips, the state machine with its rule table, the frame clock, capacities,
  record and animator sizes, the measured cost, the library lifetime rule. Header comments in `animation.hpp`; the
  frame clock section in `docs/api/time.md`; a link from `docs/api/gfx.md`. Covers AC-10, VIII.
- [x] **T11.2** [P] Aseprite workflow in `docs/api/assets.md` (export settings: JSON Hash or Array, Frame Tags,
  Slices, trim, power-of-two sheet; `--sheet hero.png --data hero.ase.json`; what is read; `@` marker tags; the
  `pivot` slice; error examples); `docs/tech/cmake.md` (the sprite step, the converter's trigger);
  `docs/architecture/security.md` (two more fuzzed parsers); `docs/architecture/project-structure.md` (the module,
  `tools/assetc` Aseprite files, `tests/animation_example`); `docs/REFERENCES.md` (Godot, Unity, Aseprite sprite
  sheet and CLI docs); the CLAUDE.md layout paragraph and build commands (`ANIMATION_DEMO`). Covers AC-10.
- [x] **T11.3** D31's change-log note "implemented" in `docs/architecture/decisions.md`; record the final numbers in
  plan.md and the ROADMAP baseline table if they changed. Covers AC-10.
- [x] **T11.4** `docs/ROADMAP.md`: tick "Sprite animation". Covers AC-10.
- [x] **T11.5** Status → implemented in spec.md, plan.md, tasks.md and `specs/README.md`;
  `python -I scripts/docs_tool.py check` reports 0 broken links.
