# Tasks 015: Tweens and timers

Status: implemented (T7.2 passed on the Old 3DS XL, 2026-11-22) · approved 2026-11-22 (maintainer pre-approved)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task is one or a few TDD loops. `[P]` means the task can be done in parallel with the previous one. Every
stage ends with something verifiable on PC (`ctest --preset pc-debug`, a screenshot, a `--pc` script). The only
console step is T7.2, a scripted check where the maintainer only starts the game (Homebrew Launcher → Y).
Deviations found while building go into plan.md as "Stage N refinements", as in plan 012.

## Stage 0: baseline and toolchain probe

VM 120 results (2026-11-22, after the merge into main): GCC 13, GCC 14 and clang 18 each ran `nnh_tests` 1367/1367;
the easing goldens (T1.4) and the self-check checksum are identical on all three. ASan/UBSan found one leak, in a
misuse test (a `TweenSet` local to a call that asserts and jumps out); fixed in `9757250`, then clean.

- [x] **T0.1** Baseline numbers (Windows, 3 runs each). Measure: the `hello.exe` and `hello.3dsx`/`.elf` sizes
  (`size`); edit `examples/hello/main.cpp` → simulator and `.3dsx` build times; clean engine build; the
  preprocessed line count and compile time of a one-line TU including `<nnh/events.hpp>` (the future
  `tween.hpp` comparison). Files: plan.md ("Stage 0 results"). Covers III, AC-8.
- [x] **T0.2** Determinism probe (scratch, not committed). Files: a 60-line `probe.cpp` with draft `sin_quarter`,
  `exp2_neg` and `ease_elastic_out`. Build it with WinLibs GCC, devkitARM (`scripts/dkp.sh`), and GCC 13/14 and
  clang 18 on VM 120. Test: `objdump -d` shows no fused multiply-add with `-ffp-contract=off`, and records what
  the default does; the printed bit patterns agree on all hosts. Record GCC's C++ default for `-ffp-contract` and
  the libctru FPSCR finding (`0x03000000` in `__system_initSyscalls`/`initThreadVars`) in plan.md. Covers FR-3.

## Stage 1: easing, the frame clock and the timer value (pure)

- [x] **T1.1** `FrameTime`, `Frames`, the `Duration` → ticks rounding, the invalid marker and the limits. Files:
  `engine/core/include/nnh/tween.hpp` (first part). Test: `tests/tween/test_frame_clock.cpp` (conversion table:
  1 s = 245 760, 0.5 s, 16.67 ms = 4096, 1 ms, 0, negative and > 1 h invalid, `Frames{n}`, `duration()` round
  trip). Covers FR-13, FR-16.
- [x] **T1.2** Approximations `sin_quarter`, `sin_turns`, `exp2_neg`. Files: `engine/core/src/tween/ease_math.{hpp,cpp}`.
  Test: `tests/tween/test_ease_math.cpp` (error bounds against `<cmath>` on a dense sweep, exact endpoints,
  reduction across periods). Covers FR-3.
- [x] **T1.3** The 19 curves, `ease()`, `Easing` (built-in and custom), and the t ≤ 0 / NaN / tiny / ≥ 1 rules.
  Files: `tween/ease.cpp`, `tween.hpp`, `engine/core/CMakeLists.txt` (sources + `-ffp-contract=off` on
  `src/tween/*`). Test: `tests/tween/test_ease.cpp` (endpoints, range, overshoot, against the double reference
  within 1/4096, custom function, no init). Covers FR-1, FR-2.
- [x] **T1.4** Golden bit table: 19 curves × 33 points, generated once on PC, then frozen. Files:
  `tests/tween/test_ease_golden.cpp`. Test: passes with WinLibs GCC and on VM 120 (GCC 13/14, clang 18).
  Covers FR-3.
- [x] **T1.5** [P] The `Timer` value type. Files: `tween/timer.cpp`, `tween.hpp`. Test: `tests/tween/test_timer.cpp`
  (once, repeating, several fires in one `advance`, carry, pause/resume/stop/restart, remaining/elapsed/progress,
  no `nnh::init`). Covers FR-14, FR-16.

  Stage end: `ctest --preset pc-debug` green; `3ds-release` builds; T1.4 green on VM 120.

## Stage 2: tween sets: storage, values, handles, the clock (pure)

- [x] **T2.1** `TweenBlock` layout, `SlotHot`/`SlotCold`/`Step`, size `static_assert`s, construction and destruction
  (one `mem::alloc`, tag `tween`), move, set serials, capacities and their limits, log channel `tween` (defined in
  the module, not in `engine_channels.cpp`). Files: `nnh/detail/tween_detail.hpp`, `tween/tween_block.hpp`,
  `tween/tween_internal.hpp`, `tween/tween_set.cpp`. Test: `tests/tween/test_tween_set.cpp` (tag stats, freed on
  destruction, move, absurd capacities assert). Covers FR-4, FR-18.
- [x] **T2.2** Value packing and interpolation for float/Vec2/Color, writes by pointer and by setter, capture from a
  pointer, Color rounding and clamping, exact final write. Files: `tween/tween_values.cpp`, `tween_detail.hpp`
  (`Setter<T>`, `is_tween_value`). Test: `tests/tween/test_tween_values.cpp`;
  `tests/tween/compile_fail_setter_capture.cpp` (ctest that expects a compile failure, as the bus's
  `compile_fail_big_capture.cpp`). Covers FR-5.
- [x] **T2.3** Starting tweens: `to`/`by`/`from_to` (pointer and setter), the `from` write at creation, the full set
  (empty handle + log + assert), invalid times. Files: `tween/tween_start.cpp`, `tween.hpp`. Test:
  `tests/tween/test_tween_start.cpp`. Covers FR-4, FR-5, FR-6.
- [x] **T2.4** Advancing one slot (delay, capture at start, progress, loops, restart, yoyo with mirrored time, carry)
  and the update loop (start-order list, `fresh` flag, sweep, `update`/`advance`/speed with remainder, profile
  scope). Files: `tween/tween_advance.cpp`, `tween/tween_update.cpp`. Test: `tests/tween/test_tween_loops.cpp`,
  the clock cases of `test_frame_clock.cpp` (30/60/600 updates, speed 0.5/0/2, `Frames{n}`, two identical runs give
  the same bits). Covers FR-6, FR-7, FR-16, FR-17, FR-18.
- [x] **T2.5** Handles and commands: queries, pause/resume/kill/complete (without callbacks yet), stale handles,
  foreign handles, group commands, `kill_target` (standalone). Files: `tween/tween_commands.cpp`. Test:
  `tests/tween/test_tween_handles.cpp`, `test_tween_group.cpp` (two tweens on one target, later wins). Covers FR-8,
  FR-9.

  Stage end: ctest green on PC and VM 120; `3ds-release` builds, which also checks the 32-bit size bound.

## Stage 3: completion, timers and re-entrancy (fake platform)

- [x] **T3.1** Completion: final write → `on_finish` → event (`FinishEvent`, `events_publish`), `on_loop`, `complete`
  running completion, nothing on kill/clear. Files: `tween/tween_finish.cpp`, `tween_detail.hpp` (`FinishEvent`).
  Test: `tests/tween/test_tween_finish.cpp` (events delivered at the next `nnh::frame()` on the fake platform);
  `tests/tween/compile_fail_big_event.cpp`. Covers FR-8, FR-11.
- [x] **T3.2** Timers `after` (callback and event form) and `every` (n and forever, several fires per `advance`,
  minimum period). Files: `tween/tween_start.cpp`, `tween/tween_advance.cpp`. Test:
  `tests/tween/test_tween_timers.cpp`. Covers FR-13, FR-16.
- [x] **T3.3** Re-entrancy: start/pause/complete/kill from callbacks (including self and `kill_all`), fresh tweens,
  the depth limit 8 with one logged error. Files: `tween/tween_update.cpp`, `tween/tween_finish.cpp`. Test:
  `tests/tween/test_tween_reentrancy.cpp`. Covers FR-12.
- [x] **T3.4** Misuse matrix: Debug asserts and Release-path logs for full sets, foreign handles, invalid times, bad
  speed, publishing before init. Files: the files above as needed. Test: `tests/tween/test_tween_misuse.cpp`.
  Covers FR-18.

  Stage end: ctest green on PC and VM 120.

## Stage 4: sequences (pure)

- [x] **T4.1** `SequenceBuilder`: steps (to/by/from_to, `join`, `wait`, `call`), the per-sequence and per-set limits,
  a failed build (full) → empty handle and the partial sequence freed, the first-group `from` written at append,
  total length. Files: `tween/sequence_build.cpp`, `tween.hpp`. Test: `tests/tween/test_sequence.cpp` (building).
  Covers FR-10.
- [x] **T4.2** Playing: groups, longest member, carry across groups (three 0.5 s steps end on update 90), zero-length
  call groups in the same update, capture when reached, a child's `on_finish`. Files: `tween/sequence_advance.cpp`.
  Test: `test_sequence.cpp` (playing), `test_frame_clock.cpp` (90). Covers FR-10, FR-16.
- [x] **T4.3** Sequence loops, restart and yoyo (groups in reverse order, mirrored child time, calls in both
  directions), sequence-level `on_loop`/`on_finish`/event, `complete` and `kill_target` on sequences, re-entrancy
  inside call steps. Files: `sequence_advance.cpp`, `tween_commands.cpp`. Test: `test_sequence.cpp`,
  `test_tween_group.cpp`, `test_tween_reentrancy.cpp`. Covers FR-9, FR-10, FR-11, FR-12.
- [x] **T4.4** No allocation while playing: a counting global `operator new`/`delete` in the tests binary plus
  `mem::live_allocations()` around creating, starting, building, playing, finishing and killing tweens, timers and
  sequences; the leak report is clean after destruction. Files: `tests/tween/alloc_counter.cpp`,
  `tests/tween/test_tween_alloc.cpp`. Covers FR-4, AC-4.

  Stage end: ctest green on PC and VM 120.

## Stage 5: scene binding (D30)

- [x] **T5.1** Scene attachments: node, attach in order, detach, notify pass with epoch (attached during a pass waits)
  and cursor (detached during a pass is safe), the `Scene` destructor check. Files: `nnh/scene.hpp`,
  `engine/core/src/scene/scene_attachments.{hpp,cpp}`. Test: `tests/scene/test_scene_attachments.cpp` (a fake
  attachment that records calls). Covers FR-15.
- [x] **T5.2** Scene stack calls: `BeforeUpdate` before the top `update()` only, `AfterExit` after `on_exit()` on
  pop/replace/`clear()`. Files: `scene/scene_stack.cpp`. Test: `tests/scene/test_scene_stack.cpp` (new cases),
  existing scene tests unchanged and green. Covers FR-15.
- [x] **T5.3** Bound sets: `TweenSet(Scene&, config)`, attach/detach in the block, update via the hook, silent clear
  after exit, hand `update`/`advance` asserts, re-entering works, destruction while on the stack detaches. Files:
  `tween/tween_scene.cpp`, `tween_set.cpp`. Test: `tests/tween/test_tween_scene.cpp` with `RecordingScene` and the
  fake platform's `run()`/`shutdown()` (order relative to `update()`, frozen under a pushed scene, cleared on
  pop/replace/end of run/shutdown, no callbacks or events, targets untouched). Covers FR-15, AC-3.
- [x] **T5.4** Opt-in check: `hello.exe` and `hello.elf` sizes against T0.1 (budget: + ≤ 256 B of text from the
  hook, no `tween` symbol in `nm`); `examples/scenes` screenshot unchanged; the platformer's `level.nnhs` and
  `gate.nnhs` with `--pc`. Files: plan.md ("Stage 5 results"). Covers FR-19, AC-8.

  Stage end: ctest green on PC and VM 120; `3ds-release` builds; the numbers are in plan.md.

## Stage 6: the example `examples/tweens` (PC)

- [x] **T6.1** The self-check script as pure code: a fixed script of tweens, sequences, loops, yoyos, timers and
  `Timer` values over 600 frames; an FNV-1a checksum of every target's bits and of the callback order, logged every
  60 frames; the rounding probe (`1 + 1.5·2⁻²⁴`); the verdict `tweens: PASS <checksum>` / `tweens: FAIL ...`
  against a golden constant recorded from the simulator. Files: `examples/tweens/self_check.{hpp,cpp}`. Test:
  `tests/tweens/test_self_check.cpp` (compiled from the example, like `tests/platformer`, on PC and VM 120).
  Covers AC-5, FR-3, FR-16.
- [x] **T6.2** The showcase scene. Top screen: the easing gallery, 19 cells, each with its curve plotted from
  `ease()` and a marker looping along it. Bottom screen: the intro sequence (a panel slides in with `BackOut` while
  a colour fades, a title pops, then "READY" after a timer), a blink driven by `nnh::Timer`, and buttons for
  pause/resume (A), complete (B), restart (Y) and push the overlay (X). The overlay scene has its own bound set,
  which keeps playing while the showcase is frozen; START pops it. Files: `examples/tweens/main.cpp`,
  `showcase_scene.{hpp,cpp}`, `easing_gallery.{hpp,cpp}`, `intro.{hpp,cpp}`, `overlay_scene.{hpp,cpp}`,
  `CMakeLists.txt`, `examples/CMakeLists.txt`. Test: PC screenshots read from the PNG
  (`NNH_EXIT_AFTER_FRAMES=90 NNH_SCREENSHOT=build/tweens.png`, plus `TWEENS_DEMO=overlay`). Covers AC-6.
- [x] **T6.3** [P] The self-check and bench scenes. Selected by `TWEENS_DEMO=check|bench` or from the showcase
  (SELECT cycles: showcase → self-check → bench). The bench runs 256 mixed tweens (all kinds and curves, elastic
  included) in an unbound set and measures `update()` with `time::now()` over 600 frames (average and worst). It
  logs `tweens bench: 256 tweens avg X.XX ms worst Y.YY ms (target < 1 ms) PASS|OVER` and shows the slot size and
  the set's bytes. Files: `self_check_scene.{hpp,cpp}`, `bench_scene.{hpp,cpp}`. Test: PC runs of both modes.
  Covers AC-5, AC-7.
- [x] **T6.4** The console script. With the game started, it presses SELECT to reach the self-check, waits for
  `tweens: PASS` (`fail-on "tweens: FAIL"`, `fail-on "] E "`), then SELECT to the bench and waits for its line, with
  screenshots. Files: `examples/tweens/tests/check.nnhs`. Test: `nnh run tweens --pc --script
  examples/tweens/tests/check.nnhs` passes. Covers AC-5.

  Stage end: the PC script passes; screenshots checked; `3ds-release` builds `tweens.3dsx`.

## Stage 7: console check (the only step on the 3DS)

- [x] **T7.1** Build and size check: `scripts/dkp.sh cmake --build --preset 3ds-release` and the Development build
  that `nnh run` uses; log the 32-bit slot size from the example at start. Files: plan.md. Covers II.
- [x] **T7.2** On the Old 3DS: the maintainer opens the Homebrew Launcher and presses Y, then runs
  `nnh run tweens --script examples/tweens/tests/check.nnhs`. Pass when the saved log in `build/logs/` has
  `tweens: PASS <checksum>` with the PC's checksum, the rounding probe line and the bench line under 1 ms. The bench
  number and the screenshots go into plan.md and the docs. A quick look with the debug overlay at 60 Hz in the
  showcase. Covers AC-5, AC-6, AC-7, FR-3.

  **Result (2026-11-22, Old 3DS XL, Development):** `nnh: PASS`; every checkpoint and `tweens: PASS B44CA3F6` equal
  the PC's (the easings give the same bits on ARM11 VFPv2 with libctru's FPSCR); bench 256 tweens avg 0.71 ms, worst
  0.84 ms (target < 1 ms).

## Stage 8: docs, roadmap, status

- [x] **T8.1** `docs/api/tween.md`: easings with a picture of the curves (rendered by the example or a script into
  `docs/`), tweens, targets and the lifetime rule, sequences, callbacks vs events, timers and `Timer`, scene binding,
  the frame clock and determinism (approximation bounds, the subnormal limit), capacities, sizes and the measured
  cost. Also the header comments in `tween.hpp`, links from `docs/api/scene.md` and `docs/api/time.md`, and
  `docs/architecture/scenes.md` (attachments, D30). Covers AC-9, VIII.
- [x] **T8.2** [P] `docs/REFERENCES.md` (Godot Tween/Timer, DOTween, Penner), the CLAUDE.md layout paragraph (module
  and example), `docs/architecture/project-structure.md` if the examples list changes, and the example's README
  comment block. Covers AC-9.
- [x] **T8.3** Final measurements: `hello` sizes and build times against T0.1, the compile cost of
  `<nnh/tween.hpp>`, the `tweens` sizes; record them in plan.md and the ROADMAP baseline table if it changed.
  Covers III, AC-8.
- [x] **T8.4** `docs/ROADMAP.md`: tick "Tweens and timers" (and "tweens" in the M7 line). Covers AC-9.
- [x] **T8.5** Status → implemented in spec.md, plan.md, tasks.md and `specs/README.md`; D30's change-log note
  "implemented"; `python -I scripts/docs_tool.py check` reports 0 broken links.
