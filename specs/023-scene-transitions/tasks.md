# Tasks 023: Scene transitions

Status: implemented 2026-11-24 (stages 0–8 and 11; Linux checks T9.1 passed; console run 1 done, optimisation round
T10.3 done; console run 2 done 2026-11-25 and the budgets set; merge T11.7 by the lead)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task is one or a few TDD loops (test first, then code). `[P]` means the task can be done in parallel with the
previous one. Every stage ends with something verifiable on PC (`ctest --preset pc-debug`, a screenshot, a `--pc`
script). The work happens in a **separate git worktree** (branch `spec-023`) while spec 022 is built on `main`; follow
plan.md's "Merge notes" for every shared file (one block per CMake file at the stated anchor, never the alphabetical
lists; two one-line hunks in the platformer's `level_play.cpp`; **never** `examples/platformer/tests/level.nnhs`).
The header moves come first (no behaviour change), then the input block, the pure geometry and timeline, the scene
integration with AC-2's per-frame proof and AC-3's golden, the platformer's doors, the example and the ARM pre-check
(T7.7) before the first console run. Stage 9 is run on the Linux build VM by the lead; the only console stage is stage
10: four scripted runs the maintainer only starts (Homebrew Launcher → Y). Deviations found while building go into
plan.md as "Stage N refinements", as in plans 012–022.

Guards held at every stage end (the "stage-end guards"): ctest pc-debug and pc-release; 3ds-release without a warning;
`hello` sizes within T0.1 + 96 bytes of 3DS text (and its `nm` list unchanged but for the input path's accessors);
`tweens: PASS B44CA3F6` (`examples/tweens/tests/check.nnhs --pc`); ctest `platformer_replay_matches_script` with the
replay lines **present in the tree** (never edited by this spec); `actions` and `input` `--pc` scripts PASS.

## Stage 0: baseline

- [x] **T0.1** Baseline numbers (Windows, 3 runs each; scripts in `build/ref-023/`, not committed): `hello.exe`
  (pc-release, pc-debug) and `hello.elf`/`.3dsx` (3ds-release, 3ds-dev) sizes and `nm -C` lists; `.text` of the
  3ds-release `scenes`, `actions` and `platformer` ELFs; edit `examples/scenes/title_scene.cpp` → simulator and `.3dsx`
  times (the reference for the transitions example's edit); clean `nnh_core`; preprocessed lines and `-fsyntax-only`
  time of one-line TUs including `<nnh/scene.hpp>`, `<nnh/gfx.hpp>` and `<nnh/tween.hpp>`; 3ds-dev disassembly of
  `detail::input_update`, `scenes::update_and_draw`, `nnh::run` and `detail::scenes_shutdown` saved in
  `build/ref-023/dis0/`. Files: plan.md ("Stage 0 results"). Covers III, FR-23, AC-9.
- [x] **T0.2** [P] Reference screenshots for AC-9 in `build/ref-023/ref/`: `hello`, `sprites`, `scenes` (title at 30,
  level at 90 with `NNH_SIM_INPUT`), `tweens`, `actions`, `camera` (`CAMERA_DEMO=zoom1`), platformer level 1 at 90 and
  `PLATFORMER_DEMO=replay` at 64 and 312; `build/ref-023/compare.sh` retakes them into `now/`. Files: plan.md. Covers
  AC-9.
- [x] **T0.3** [P] Goldens and counts today: the replay ctest's last line as in the tree, `B44CA3F6`, `C8FB9A41`,
  `7BB7901E`; `nnh_tests` count (raise `kMaxTests` if fewer than 300 are left: this spec adds ≈ 200). Files: plan.md.
  Covers VI.

## Stage 1: header moves (no behaviour change)

- [x] **T1.1** `<nnh/ease.hpp>`: `Ease`, `kEaseCount`, `EaseFn`, `ease`, `Easing` moved verbatim from `<nnh/tween.hpp>`,
  which includes it. No new test: spec 015's tests compile unchanged and pass; `tweens: PASS B44CA3F6`; a probe TU
  including only `<nnh/ease.hpp>` compiles and preprocesses to `<cstdint>` only. Files: `include/nnh/ease.hpp`,
  `include/nnh/tween.hpp`. Covers FR-3, FR-23 (constraint "XI and spec 015's header").
- [x] **T1.2** [P] `<nnh/detail/finish_event.hpp>`: `FinishEvent` moved verbatim from `tween_detail.hpp`, which includes
  it. Test: spec 015's `test_tween_finish.cpp` and the compile-fail tests unchanged. Files:
  `include/nnh/detail/finish_event.hpp`, `include/nnh/detail/tween_detail.hpp`. Covers FR-16.
- [x] **T1.3** Stage end: the stage-end guards; the 3ds-dev objects of `src/tween/*` identical to T0.1's (only headers
  moved). Files: plan.md. Covers AC-9.

## Stage 2: the input block (input module)

- [x] **T2.1** `detail::InputBlock` (pure): blockers (game, transition), the filtered button mask, the touch/Circle
  Pad/C-Stick "held when blocked" flags, raw masks; the per-frame filter from a raw sample to the game's sample; "still
  needed" result. Test: `tests/input/test_input_block.cpp` (blocked → idle sample; held buttons, sticks out of the dead
  zone and touches stay filtered after unblocking until released, then a new press passes; a button pressed during
  the block and held after it stays filtered; the two blockers independent; raw masks unchanged). Files:
  `src/input/input_block.{hpp,cpp}`, CMake (one line after `src/input/input.cpp`). Covers FR-12.
- [x] **T2.2** The module's path: `detail::g_input_filter` checked once in `input_update` (filtered sample into the
  existing code: D-Pad mirror from the filtered Circle Pad), engine masks `input_engine_held_mask()`/
  `input_engine_pressed_mask()`, `app.cpp`'s `overlay_update` on them; public `input::set_blocked`, `input::blocked`;
  `detail::input_block(InputBlocker, bool)`. Test: `tests/input/test_input_block_api.cpp` (queries idle, no
  `ButtonPressed`/`Released`/`TouchStarted`/`TouchEnded` queued while blocked, events again after release-and-press,
  `touch_position()` keeps the last point, remote input pushed through the fake's `remote_push` is filtered, the
  overlay's SELECT + L toggles while blocked, `set_blocked(false)` doesn't lift a transition's block); spec 003's input
  tests unchanged. Files: `src/input/input.cpp`, `input_internal.hpp`, `src/app/app.cpp`, `include/nnh/input.hpp`.
  Covers FR-12, FR-23.
- [x] **T2.3** [P] Actions under a block (D32 unchanged): an `ActionSet` evaluated while blocked reports nothing and no
  `ActionPressed`; a held binding stays unpressed after unblocking until released. Test: in
  `test_input_block_api.cpp`. Covers FR-12.
- [x] **T2.4** Stage end: the stage-end guards; `hello`'s 3ds-release text growth recorded (expected ≤ 64 bytes:
  the pointer check and the engine-mask accessors); `nm` shows no `InputBlock` symbol in `hello`; `compare.sh`
  (identical). Files: plan.md ("Stage 2 results"). Covers FR-23, AC-9.

## Stage 3: effect geometry (pure)

- [x] **T3.1** The geometry core: `effect_geometry.hpp` (the sink, the area, `q = ⌊c·65536 + ½⌋`, `ext(L)`), fade, wipe
  ×4, split ×2, blinds N ×4 edges (one `udivmod`, running remainder) and their continue forms. Test:
  `tests/transition/test_effect_fill.cpp` (exact rectangles at q = 0, 1, 16384, 32768, 49152, 65535, 65536 on both
  screen sizes; fade alpha rounding table; blinds 2/8/32 boundaries = ⌊i·L/N⌋; split ⌈e/2⌉/⌊e/2⌋; continue: opposite
  edge, centred split band). Files: `src/transition/effect_geometry.hpp`, `effect_fill.cpp`, CMake
  `_nnh_transition_sources` after the camera block (`-ffp-contract=off`). Covers FR-4, FR-5.
- [x] **T3.2** Iris: exact integer square root (bit method), R0 from the farthest corner, bands sampled at their middle
  row, circle and diamond half-widths, merged fully covered rows, clipping, the emission order. Test:
  `tests/transition/test_effect_iris.cpp` (isqrt against a brute force for every value below 2²⁰ and seeded larger
  ones; centres inside, on an edge, outside (−30, 260) and at ±4096; bands 1/4/16; nothing at q = 0, one full
  rectangle at q = 65536; counts ≤ 2·⌈H/band⌉ + 2 = 122 at band 4). Files: `src/transition/effect_iris.cpp`. Covers
  FR-4, FR-21.
- [x] **T3.3** Screens and the tall area: area coordinates (400 × (480 + gap)), bottom-screen inset 40, clipping and
  translation per screen, a bottom-screen iris centre, the gap drawn nowhere; per-screen resolution of `same()`/
  `none()` (top in, bottom out, bottom in) and `reveal_style`. Test: `tests/transition/test_effect_area.cpp` (tall wipe
  Top at gaps 0 and 24 continuing onto the bottom screen, tall iris on a bottom point covering both screens, the
  resolution table, `none()` screens). Files: `src/transition/effect_area.cpp`. Covers FR-5, FR-6.
- [x] **T3.4** The public geometry (FR-9): `effect_rects` (capacity smaller than the count: the total returned,
  `capacity` written), `effect_color`, `max_rects`, `draw_effect` (fill_rect, `ignore_camera`, the placement's layer);
  `<nnh/transition.hpp>` with the effect types and makers. Test: `tests/transition/test_effect_query.cpp` (no init, no
  stack; `draw_effect`'s recorded items equal `effect_rects`). Files: `include/nnh/transition.hpp`,
  `src/transition/effect_query.cpp`. Covers FR-9, FR-18.
- [x] **T3.5** [P] Properties and fuzz: `tests/transition/test_geometry_properties.cpp` (20 000 seeded effects ×
  coverages, rasterised: every rectangle inside its screen, nothing at 0, full cover at 1 for built-ins, covered area
  non-decreasing in q, count ≤ `max_rects`, non-overlapping for wipe/split/blinds); `tests/fuzz/fuzz_transition_geometry.cpp`
  (random effect fields, coverage bits including NaN/inf, placements: no UB, counts bounded, clamps applied); mini-fuzz
  in `nnh_tests`, libFuzzer with `NNH_BUILD_FUZZERS`. Covers FR-4, FR-21, FR-22, X.
- [x] **T3.6** Stage end: the stage-end guards; include check (`<nnh/transition.hpp>` pulls no tween set, audio,
  tilemap, actions, camera or entities header; preprocessed lines and `-fsyntax-only` time against T0.1's
  `scene.hpp` + `gfx.hpp`). Files: plan.md ("Stage 3 results"). Covers FR-23, III.

## Stage 4: the timeline and the requests (pure + fake)

- [x] **T4.1** The timeline: whole frames from `FrameTime` (half up, 0..600), the four phases, `ease(k/O)` and
  `ease((I − k)/I)`, clamp and ratchets, forced 1 and 0 at the phase ends, at least one covered frame, the covered
  phase's end rule (loading done and hold), phase progress. Test: `tests/transition/test_timeline.cpp` (frame counts
  for many (O, H, I) incl. zeros and all zero; coverage bit tables for linear and QuadIn; in = out backwards; Back,
  Elastic, Bounce monotonic; rounding of `Duration::from_ms(500)` = 30 frames). Files:
  `src/transition/transition_timeline.cpp`, `transition_internal.hpp` (state, `static_assert` 256/320). Covers FR-2,
  FR-3, FR-21.
- [x] **T4.2** The scene hook (scene module): `SceneStack::update_top()`/`draw_all()` (and `update_and_draw()` built from
  them), `Op::TransitionPop` (tolerant), `scene_hook.hpp` (`g_scene_transition`, pending-count access through
  `SceneAccess`), `scenes::update_and_draw()`'s null check, `run()`'s loop condition and its call through
  `scenes::update_and_draw()`, the cancel before `clear()` in `run()` and `scenes_shutdown()`. Test:
  `tests/scene/test_scene_hook.cpp` (halves equal the old `update_and_draw` call for call; tolerant pop on an empty
  stack; a test hook receives every frame and the cancel; `run()` loops while the hook is set on an empty stack);
  `test_scene_stack.cpp`, `test_run.cpp`, `test_scene_attachments.cpp` unchanged. Files: `src/scene/scene_hook.hpp`,
  `scene_stack.{hpp,cpp}`, `scenes.cpp`, `include/nnh/scene.hpp` (comments). Covers FR-10, FR-11, FR-19, FR-23.
- [x] **T4.3** Requests and validation: `replace/push/pop/cover`, refusal while running (one debug log per running
  transition), FR-22's misuse table (assert; Release clamp or refuse with an error log), resolution into the state,
  the pending count, the hook pointer, the input blocker, the change enqueued at once when O = 0; `reveal_at`. Test:
  `tests/transition/test_requests.cpp` (each request; each misuse in Debug and on the Release path — pc-release run;
  queries `running/phase/coverage/phase_progress/loading_progress/frames/timing/seconds`). Files:
  `src/transition/transition_request.cpp`, `transition_queries.cpp`. Covers FR-1, FR-15, FR-19, FR-22.
- [x] **T4.4** Stage end: the stage-end guards; `hello`'s and `scenes`' sizes (the hook check: expected ≤ 16 bytes in
  `scenes`, nothing in `hello`); `compare.sh` (identical). Files: plan.md ("Stage 4 results"). Covers FR-23, AC-9.

## Stage 5: the transition in the frame (scenes, overlay, loading, input), AC-2, AC-3

- [x] **T5.1** The hook's frame: profile scope `transition`, the step, `on_covered` then the loading step, which scene
  updates (`update_out`/`update_in`), `draw_all`, the overlay (built-in through the sink into `fill_rect`, custom
  through `Options::custom`), the end-of-frame transitions (enqueue, In or done, skip the in phase over an empty
  stack), done (block lifted, hook cleared, `on_finished` from a copy, the finish event), cancel. Test:
  `tests/transition/test_scene_updates.cpp` (no update in any phase by default; the options; the first update the frame
  after done; a bound `TweenSet` freezes; D30 `AfterExit` after `on_exit`), `test_stack_change.cpp` (frames of
  `on_exit`/`on_enter`; slow `on_enter` (fake time jump) skips nothing; a waiting scene destroyed → crash caught; the
  target pushed elsewhere → caught; a loading scene that starts a second transition from its `update()`),
  `test_overlay_order.cpp`, `test_custom_effect.cpp`. Files: `src/transition/transition_frame.cpp`,
  `transition_overlay.cpp`. Covers FR-7, FR-8, FR-10, FR-11, FR-14.
- [x] **T5.2** Loading and notifications: the loading step per covered frame (clamp, NaN), the hold, the warning at
  covered frame 600 (once, Debug/Development), `on_covered`/`on_finished`/finish event frames, none on cancel, a
  request from `on_finished` in the next frame. Test: `tests/transition/test_loading.cpp`,
  `tests/transition/test_notifications.cpp`. Files: `src/transition/transition_loading.cpp`, `transition_frame.cpp`.
  Covers FR-13, FR-16.
- [x] **T5.3** Interruptions and plain loops: refused requests, plain stack changes in each phase, the transition's pop
  after the game emptied the stack, pop of the last scene under `run()` (covered phase runs, `run()` returns, no in
  phase) and in a manual loop, `shutdown()` and the end of `run()` in each phase (silent, the waiting scene released,
  the block lifted); a hand-written loop equals `run()` frame for frame; `cover()` with no scenes. Test:
  `tests/transition/test_interruptions.cpp`, `tests/transition/test_plain_loop.cpp`. Covers FR-18, FR-19.
- [x] **T5.4** Input during transitions (FR-12, AC-4): the block from the first transition frame's input to the frame
  after done; held controls filtered until released; remote input filtered; `block_input = false`. AC-4: an `ActionSet`
  with contexts bound to the outgoing and incoming scenes; the confirm press, a button held through, a tap during — no
  action, button or touch event, no `pressed()` in either scene; the held button reaches the new scene only after
  release and press. Test: `tests/transition/test_transition_input.cpp`, `test_transition_actions.cpp`. Covers FR-12,
  AC-4.
- [x] **T5.5** No wrong frame (AC-2): replace, push, pop and cover × fade, wipe ×4, split ×2, blinds, iris circle and
  diamond, a tall wipe, a custom effect that covers; per frame from the fake's submitted items (rasterised per screen):
  while the outgoing scene's items are submitted, the overlay is below full only before the change; from the first
  frame the incoming scene's items are submitted until the cover shrinks, every covered screen is fully covered by
  overlay items after them; no frame submits the incoming scene on a covered screen without a full cover above. Test:
  `tests/transition/test_no_wrong_frame.cpp`; fake `kMaxRecordedItems` 1024. Files:
  `engine/platforms/fake/include/nnh/platform_fake.hpp`. Covers AC-2, FR-11.
- [x] **T5.6** Cost and misuse sweep: state size `static_assert`s; the allocation counter unchanged over 1000
  transitions of every kind; no log line in the normal path; `events::stats()` shows only the game's finish events.
  Test: `tests/transition/test_transition_cost.cpp`. Covers FR-21, FR-22.
- [x] **T5.7** The golden script (AC-3): `examples/transitions/check_script.{hpp,cpp}` and `check_scenes.{hpp,cpp}`
  (scenes A, B, C; plan's ≈ 24 steps over 1200 frames; the per-frame FNV-1a of phase, coverage bits, loading bits,
  `effect_rects`/`effect_color` per screen, event frames; a checkpoint every 120 frames); goldens from pc-debug in
  `kTransitionsCheckGolden`/`kTransitionsCheckpoints`, equal in pc-release. Test:
  `tests/transitions_example/test_self_check.cpp` (on the fake platform; a one-frame shift of any step fails). Files:
  those, `tests/CMakeLists.txt` (023's block). Covers AC-3, FR-20.
- [x] **T5.8** Stage end: the stage-end guards; `compare.sh` (identical); `linkcheck.sh`: a scenes-only probe links no
  `transition` symbol and no `InputBlock`. Files: plan.md ("Stage 5 results": goldens and checkpoints). Covers FR-23,
  AC-9.

## Stage 6: the platformer's doors

- [x] **T6.1** Doors through an iris: `ShowDemo{demo, at, door}` (`game_events.hpp`); `LevelPlay::enter_doors()` publishes
  the player's centre (one line); `door_transition.{hpp,cpp}` (`kDoorTransition`: tall iris, band 4, 36/6/36, QuadIn);
  `Demos::on_show` → `transition::replace` for a door when no replay runs (out centre from the top camera), instant
  otherwise; `LevelScene::on_enter()` → `reveal_at(player's top-screen point)` when a transition runs. Test:
  `tests/platformer/test_door_run.cpp` (part 1: a door request starts a transition with the iris at the player's
  screen point, the reveal at the spawn point, input blocked; the replay's `ShowDemo` and SELECT stay instant). Files:
  `examples/platformer/game_events.hpp`, `level_play.cpp`, `door_transition.*`, `demos.*`, `level_scene.*`,
  `CMakeLists.txt`. Covers AC-7.
- [x] **T6.2** The door run: `LevelPlay::start_at_door(bool)` (one setter, one member next to `in_door_`, one line in
  `restart()`), `door_run.{hpp,cpp}` (Y in a level or `PLATFORMER_DEMO=doors`; input "run right"; counts door
  transitions; a `FrameMeter`; 10 irises; the log lines of plan.md), `main.cpp` (`doors`). Test: `test_door_run.cpp`
  (part 2: 10 transitions alternate level 1 ↔ bonus, the lines' frames, the verdict). Files:
  `examples/platformer/level_play.hpp`, `level_play.cpp`, `door_run.*`, `level_scene.*`, `main.cpp`. Covers AC-7, AC-8.
- [x] **T6.3** `examples/platformer/tests/doors.nnhs`: Y → `doors: started`, the ten lines, the verdict; then `press
  RIGHT` into the next door, a fixed `wait`, `screenshot door-iris`; `fail-on "doors: 60 Hz FAIL"`, `fail-on "] E "`;
  a `wait 3` after every `expect` an input follows; ctest `platformer_doors_script_pc` (`RESOURCE_LOCK
  nnh_remote_port`). Test: `nnh run platformer --pc --no-build --script examples/platformer/tests/doors.nnhs` → PASS,
  twice. Covers AC-7, AC-8.
- [x] **T6.4** The replay is untouched and the screenshot: ctest `platformer_replay_matches_script` passes with the
  tree's `level.nnhs` **unedited** (`git diff --exit-code examples/platformer/tests/level.nnhs`); `level.nnhs --pc`
  PASS; `gate.nnhs --pc`, `zoom_gate.nnhs --pc` PASS; `PLATFORMER_DEMO=doors NNH_EXIT_AFTER_FRAMES=<mid-iris frame>
  NNH_SCREENSHOT=build/doors.png` read back (the circle closing on the player over both screens); `compare.sh` (level 1
  and replay references identical). Files: plan.md ("Stage 6 results": the door lines' frames, the screenshot frame).
  Covers AC-7, AC-9.

## Stage 7: `examples/transitions` (PC) and the ARM pre-check

- [x] **T7.1** Art, music and scenes: `art/make_art.py` (tiles for two rooms, the square, curtain pieces, bar frame),
  `art/make_music.py` (`assets/music/loop.ogg`, 32 kHz, 4 bars), `title_scene`, `room_scene` (×2, tile layers top,
  map and HUD bottom, the square), `options_menu` (effect, reveal, screens → `Options`), `hud` (queries), `input_log`
  (logs `input: <button>`, counts inputs seen during a transition), `main.cpp`, `demos.hpp`, `CMakeLists.txt`,
  `examples/CMakeLists.txt`. Test: PC screenshots of the title and a room read back. Files: `examples/transitions/*`.
  Covers AC-5.
- [x] **T7.2** Changes, loading, music: A/B/X changes (B with a loading step and the curtain's bar, X through
  `loading_scene`), the curtain custom effect with the example's sprites, the touch → iris centre, music stop/fade-in
  from the timing, the log lines of plan.md; `TRANSITIONS_SHOT` modes. Test: PC screenshots at frame 40 (mid-coverage)
  of each built-in effect and the curtain, at 61 (full cover), the tall wipe, the loading bar — read back (shapes, both
  screens, no gaps at full cover). Files: `room_scene.*`, `loading_scene.*`, `curtain.*`, `demos.hpp`. Covers AC-5,
  FR-8, FR-13, FR-14, FR-17.
- [x] **T7.3** Check scene (AC-6): SELECT runs T5.7's script on the real engine and returns to the showcase
  (`transitions: showcase`); `TRANSITIONS_DEMO=check`; ctest `transitions_self_check_pc`. Files: `check_scene.*`,
  `tests/CMakeLists.txt`. Covers AC-3, AC-6.
- [x] **T7.4** `NNH_PROFILE_SCOPE("gfx submit")` in `detail::gfx_submit()` (Development/Debug; Release objects
  identical to T0.1's). Files: `src/gfx/gfx_frame.cpp`. Covers AC-8.
- [x] **T7.5** [P] Bench (AC-8): `bench.{hpp,cpp}` (workloads base, fade, wipe, split, blinds8, iris, loading; 300 out
  frames each), `bench_scene.{hpp,cpp}` (per-frame `transition` + submit − base submit; the loading slice's scope
  subtracted; one line per workload, the iris's rectangle count, the verdict; Debug `not judged (Debug)`); Y and
  `TRANSITIONS_DEMO=bench`. Test: a pc-dev run logs every line and PASS. Covers AC-8.
- [x] **T7.6** The script `examples/transitions/tests/check.nnhs`: SELECT → every checkpoint and the PASS line;
  `transitions: showcase`; A (fade) with B pressed during it → `transition: done … seen 0`; L/R/D-Pad to the iris, a
  touch → `iris centre: bottom 100,120`, A → its start and done lines; R to tall, A; B (loading) with screenshots at
  fixed waits (mid-coverage, full cover, the bar); a `wait 3` after every `expect` an input follows; Y → the bench lines
  and `transitions bench: PASS`; `fail-on "transitions: FAIL"`, `fail-on "transitions bench: FAIL"`, `fail-on "] E "`;
  ctest `transitions_check_script_pc` (`RESOURCE_LOCK nnh_remote_port`). Test: `nnh run transitions --pc --no-build
  --script examples/transitions/tests/check.nnhs` → PASS, twice. Covers AC-5, AC-6, AC-8.
- [x] **T7.7** ARM11 pre-check (before any console run): `arm-none-eabi-objdump -drC` of the 3ds-dev objects
  `src/transition/*`, `src/input/input.o`, `input_block.o`, `src/scene/scenes.o`, `scene_stack.o`,
  `src/gfx/draw2d.o`, `gfx_frame.o`, `draw_list.o` and the example's `bench.o`; per AC-8 hot path record instructions,
  calls, stack reloads, `vmrs`, `vdiv`, `__aeabi_*`, `memcpy`/`memset`: the iris band loop (≈ 100–130 instructions per
  band, no call but the sink, no `__aeabi_*`), blinds (one `__aeabi_uidivmod` per screen), the step (one `ease` call,
  one `vdiv`), `input_update` (one load and branch added), `scenes::update_and_draw` (one load and branch), and the
  rectangle path `fill_rect` → `DrawList::record` → build → `draw_2d` counted against plan.md's table; compare and fix
  what differs without changing a result (every golden holds). Files: plan.md ("Stage 7 ARM pre-check": the table and
  the expected console numbers). Covers AC-8, II.

## Stage 8: mutation testing, opt-in checks

- [x] **T8.1** Mutation pass as in plans 019–022: hand mutants (`build/ref-023/mutants.py`, applied one at a time by
  `mutate.py`, the whole `nnh_tests` each) of `src/transition/*`, `src/input/input_block.cpp`, the scene hook and the
  tolerant pop, the input path's check, the platformer's door code; Release-path mutants in pc-release; survivors become
  tests or are argued equivalent. Files: plan.md ("Mutation testing"). Covers VI.
- [x] **T8.2** [P] Release strings: `scripts/check_release_strings.sh` on the 3ds-release and pc-release `transitions`
  executables passes (add Development-only texts to its list if any). Covers FR-22.
- [x] **T8.3** [P] Opt-in by linking: probes (`build/ref-023/probe_*`, not committed) linked against the 3ds-release
  libraries: a scenes-only game has no `transition` or `InputBlock` symbol; a game calling only `draw_effect` links the
  geometry but no `transition_frame`/`transition_request`; `hello`'s sizes within T0.1 + 96 bytes (3DS text) and its
  `nm` list differing only by the input path's accessors. Files: plan.md ("Stage 8 results"). Covers FR-23, AC-9.
- [x] **T8.4** Regression and build times: `compare.sh` (every reference of T0.2 identical); every example's console
  script passes with `--pc` (actions, animation, camera, data, entities, foundation ×2, input ×2, platformer `gate`,
  `level`, `zoom_gate`, `doors`, prefabs, queries, tweens, transitions); `.text` of `scenes`, `actions`, the
  platformer and `transitions` recorded; edit `examples/transitions/room_scene.cpp` → simulator and `.3dsx` times
  against T0.1's and constitution III. Files: plan.md ("Stage 8 results"). Covers AC-9, III.

## Stage 9: Linux checks (VM 120, run by the lead)

- [x] **T9.1** (done 2026-11-24, plan "Stage 9 results"). GCC 13, GCC 14 and clang 18 ASan/UBSan: ctest 271/271 each
  after spec 024's fixes; the first round's failures were in particles, not here. `fuzz_transition_geometry` 600 s,
  6 500 974 runs, cov 198, no finding. The doors script under ASan failed only on the environment (no preset build
  folder). Original task: On `nnhge-build`: GCC 13, GCC 14 and clang 18 (ASan/UBSan): `nnh_tests` and ctest pass; AC-3's golden
  and checkpoints, the geometry exact tables and properties, the replay ctest with the tree's lines, `B44CA3F6`
  identical on all three (they are assertions); ASan/UBSan clean over the hook's cancel paths, `on_finished` starting a
  transition, the sink and the filter; libFuzzer `fuzz_transition_geometry` 10 minutes, clean. Files: tasks.md
  (results), plan.md ("Stage 9 results"). Covers I, X, FR-20, AC-1, AC-3.

## Stage 10: console check (the only steps on the 3DS)

- [ ] **T10.1** (on the PC) Builds and checks: `scripts/dkp.sh cmake --build --preset 3ds-release` and the Development
  builds of `transitions`, `platformer` and `actions` without a warning; T7.7's table still true; `hello.elf` within
  T0.1 + 96 bytes; plan.md's "Stage 10: console run (prepared)" completed with the recorded goldens, checkpoints,
  showcase lines and door frames. Covers AC-9, II.
- [x] **T10.2** Old 3DS, nobody touching it after Y. Four runs, each: Homebrew Launcher, press **Y**, then on the PC:
  1. `build/pc-debug/tools/nnh/nnh run transitions --script examples/transitions/tests/check.nnhs` → every
     `transitions: <frame> sum` line and `transitions: PASS` equal the PC's, every `transition: done … seen 0`, the iris
     centre line, the bench lines and the verdict (numbers recorded); screenshots read back (geometry equal to the
     PC's at the same frames, fade colours within one step);
  2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/doors.nnhs` → the ten door lines
     at the PC's frames, `doors: done 10 irises … 60 Hz PASS`, the `door-iris` screenshot;
  3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` → the tree's replay
     lines, `60 Hz PASS`;
  4. `build/pc-debug/tools/nnh/nnh run actions --script examples/actions/tests/check.nnhs` → PASS as before.
  Files: plan.md ("Console run 1": verdicts, bench numbers, log paths). Covers AC-6, AC-7, AC-8.
  *Runs 1 and 2 done 2026-11-24: `PASS BFE1D810`, doors `60 Hz PASS` (worst 20.9 ms); bench FAIL (split 0.1106 ms,
  loading engine 0.1046 ms), numbers in plan.md "Console run 1".*
- [x] **T10.3** (only if a bench target is missed) Console bench refinement: find the cost in the 3ds-dev disassembly,
  apply plan risk 1's list without changing a result (every golden holds, or AC-3's golden re-recorded on the PC first
  for option (c)); then T10.2 run 1 again; if still missed, the budget is set from the measurement with ≈ 25 % margin,
  written in the spec's AC-8, `bench_scene.cpp` and `docs/api/transition.md` with the numbers. Files: plan.md
  ("Optimisation round", "Console run 2 and budgets set"). Covers AC-8.
  *Optimisation round done on the PC 2026-11-24 (plan.md "Optimisation round (T10.3)"): the bench's profiler self-cost,
  nested loading scope and music removed from the measurement, a `rect` line for gfx's share; the overlay records
  rectangles directly (187 → 73 instructions each) and `isqrt` starts with `clz`; every golden unchanged. Pending: the
  console rerun of run 1 (and doors), then budgets from the measurement if loading still misses.*
  *Console run 2 done 2026-11-25: `PASS BFE1D810`, doors `60 Hz PASS` (worst 20.4 ms); every workload in its expected
  range, loading engine 0.0825 ms still over 0.05; budgets set from run 2 + ≈ 25 % (plan.md "Console run 2 and budgets
  set").*

## Stage 11: docs, roadmap, status

- [x] **T11.1** (the Old 3DS costs added after T10.2) `docs/api/transition.md`: requests and phases, the frame clock
  (first frame, whole frames, easing per phase, ratchet), every built-in effect with a picture
  (`docs/images/transitions/*.png` from T7.2's screenshots), reveal styles, screens and the tall area, custom effects,
  FR-9's geometry, what scenes do in each phase, input blocking, loading steps and loading scenes, queries and
  notifications, music patterns, interruptions (refusals, plain changes, pop of the last scene, shutdown),
  determinism (the fade's ±1), costs and draw counts, misuse; header comments (allocates? per-frame safe? misuse?).
  Covers VIII, AC-10.
- [x] **T11.2** Other pages: `docs/api/scene.md` (the update exception, `update_and_draw()` with a transition, the
  manual-loop condition), `docs/architecture/scenes.md` (a "Transitions (D38)" section between "Frame flow with scenes"
  and "Scene attachments (D30)", the frame-flow line), `docs/api/input.md` (the block), `docs/api/actions.md` (actions
  under a block), `docs/api/audio.md` (link to the music patterns), `docs/api/tween.md` (`<nnh/ease.hpp>`),
  `docs/api/gfx.md` (overlays on layer 15 and drawing after `update_and_draw()`), `docs/architecture/patterns.md` (the
  row: implemented), `docs/REFERENCES.md` (Godot, Unity, HaxeFlixel, Defold Monarch, citro2d, Butano), `CLAUDE.md`
  (layout paragraph after "Camera helpers", the transitions and doors script commands after the `queries` command), the
  `nnh-architecture` skill's pattern list; `python -I scripts/docs_tool.py check` → 0 broken links. Covers AC-10.
- [x] **T11.3** `docs/architecture/decisions.md`: D38 change-log note "implemented" with the stage refinements. Covers
  AC-10.
- [x] **T11.4** Final measurements (header costs, build times, `hello` sizes, `.text` growth) in plan.md and
  `docs/ROADMAP.md`'s baseline table if it changed. Covers III.
- [x] **T11.5** `docs/ROADMAP.md`: the "Scene transitions" checkbox, with the implemented summary.
- [x] **T11.6** spec, plan, tasks and `specs/README.md` status → implemented (or "implemented, console T10.2 pending").
- [ ] **T11.7** (the lead, after both specs are done) Merge `spec-023` into `main` per plan.md's "Merge notes"; then the
  full ctest (pc-debug, pc-release), every example's `--pc` script, AC-3's golden, spec 022's goldens and the replay
  ctest with spec 022's lines, one 3ds-release build; T10.2 run 3 is repeated if the merge happened after it.
