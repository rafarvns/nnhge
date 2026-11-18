# Tasks 003: Full input — sticks, touch, gamepad

Status: in progress (approved 2026-11-18)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops. `[P]` = can be done in parallel with the previous task.

## Stage 1 — pure logic

- [x] **T1.1** stick shaping (dead zone, rescale, clamp, non-finite) — files: `core/src/input/stick.{hpp,cpp}` — test: `tests/input/test_stick.cpp` — covers FR-1
- [x] **T1.2** [P] touch edges and last point — files: `core/src/input/touch_state.hpp` — test: `tests/input/test_touch_state.cpp` — covers FR-4
- [x] **T1.3** [P] Circle Pad → D-Pad mirror with hysteresis — files: `core/src/input/dpad_mirror.{hpp,cpp}` — test: `tests/input/test_dpad_mirror.cpp` — covers FR-12

## Stage 2 — contract, fake, public API, events

- [x] **T2.1** `InputSample` + `read_input()` replace `read_buttons()` in the contract, fake, pc and 3ds (buttons only, behaviour unchanged) — files: contract `input.hpp`, `input_{fake,pc,3ds}.cpp`, `app.cpp` — test: existing input/event tests still pass; conformance `idle_input_is_zero` — covers FR-10
- [x] **T2.2** fake setters for sticks and touch — files: `platform_fake.hpp`, `control_fake.cpp`, `input_fake.cpp` — test: used by T2.3 — covers FR-10
- [x] **T2.3** public queries `circle_pad`, `c_stick`, `touching`, `touch_started`, `touch_ended`, `touch_position`, mirror switch — files: `nnh/input.hpp`, `input.cpp`, `input_internal.hpp` — test: `tests/input/test_input_queries.cpp` — covers FR-1, FR-2, FR-4, FR-6, FR-12
- [x] **T2.4** `TouchStarted`/`TouchEnded` events after button events — files: `input.cpp` — test: `tests/events/test_input_events.cpp` — covers FR-5, FR-6

## Stage 3 — PC simulator

- [x] **T3.1** keyboard sticks (I/J/K/L, numpad 8/4/2/6) and capabilities — files: `input_pc.cpp`, `system_pc.cpp` — verified: `NNH_SIM_INPUT`-free run + T3.4 screenshot — covers FR-7, FR-2
- [x] **T3.2** mouse = touch on the bottom screen — files: `touch_pc.cpp`, `context_pc.hpp` — covers FR-8
- [x] **T3.3** gamepad: hot-plug, buttons by position, sticks, triggers, combine with keyboard — files: `gamepad_pc.cpp`, `app_pc.cpp` — covers FR-9, FR-3
- [x] **T3.4** `NNH_SIM_INPUT` for headless screenshots (dev builds only) — files: `input_override_pc.cpp` — verified by T5.2 screenshots and the Release strings check

## Stage 4 — 3DS

- [x] **T4.1** Circle Pad and touch — files: `input_3ds.cpp` — covers FR-1, FR-4 (verified on an Old 3DS XL, 2026-11-18)
- [x] **T4.2** C-Stick via `irrst` on New 3DS only — files: `app_3ds.cpp`, `input_3ds.cpp` — covers FR-2, FR-3 (code done; Old 3DS correctly reports none — a New 3DS run is still pending)
- [x] **T4.3** measure Circle Pad / C-Stick full range on the console (example raw readout) and set the constants — maintainer runs, I adjust (Circle Pad 159–167 on an Old 3DS XL → 156 kept; C-Stick not measured, no New 3DS)

## Stage 5 — example, docs, closing

- [x] **T5.1** `examples/input/`: buttons, both sticks (dot in a circle drawn with debug text), touch point, mirror toggle (SELECT) — covers AC-2
- [x] **T5.2** simulator screenshots with `NNH_SIM_INPUT` (sticks deflected, touch) — covers AC-2
- [ ] **T5.3** `docs/api/input.md` (queries, events, mirror, PC keyboard/mouse/gamepad tables), `docs/api/system.md` (PC capabilities), `docs/tech/sdl3.md` (gamepad, mouse), `docs/tech/devkitpro-3ds.md` (`irrst`), header comments; the docs that mention `read_buttons()` (`patterns.md`, `solid.md`, `tdd.md`, `nnh-devkitpro` skill) — covers AC-5
- [ ] **T5.4** build budgets, Release strings check, Linux gcc/clang ASan on VM 120 — covers AC-6
- [x] **T5.5** PC play test with keyboard, mouse and a hot-plugged gamepad — maintainer — covers AC-3 (2026-11-18)
- [ ] **T5.6** run `examples/input` on a New 3DS (and Old 3DS if available) — maintainer — covers AC-4
- [ ] **T5.7** `docs/ROADMAP.md` checkbox; spec, plan and tasks status → implemented
