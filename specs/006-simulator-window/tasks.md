# Tasks 006: Simulator window — layouts, scale, fullscreen

Status: in progress (approved 2026-11-18)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops. `[P]` = can be done in parallel with the previous task.

## Stage 1 — pure layout and settings

- [x] **T1.1** layout math: natural sizes, fit, centering, rectangles per layout — files: `pc/src/sim_layout.{hpp,cpp}` — test: `tests/simulator/test_sim_layout.cpp` — covers FR-1, FR-2
- [x] **T1.2** window → bottom-screen mapping per layout (clamped while held, none in top only) — test: same file — covers FR-4
- [x] **T1.3** [P] settings format: parse/format, validation, size limit — files: `pc/src/sim_settings.{hpp,cpp}` — test: `tests/simulator/test_sim_settings.cpp` — covers FR-6

## Stage 2 — screen targets and composition

- [x] **T2.1** render targets per screen; `draw_2d` and crash text in native pixels; compose in `end_frame` (stacked); no logical presentation — files: `video_pc.cpp`, `draw2d_pc.cpp`, `debug_text_pc.cpp`, `app_pc.cpp`, `context_pc.hpp` — verified: default screenshots of every example compared with before — covers FR-5, FR-7 (hello, input, scenes, text identical; sprites differs only in the rotated sprite, now rasterized at native resolution like the 3DS; Linux headless == Windows)
- [x] **T2.2** touch through the layout — files: `touch_pc.cpp` — covers FR-4

## Stage 3 — layouts, scale, fullscreen

- [ ] **T3.1** keys F2/F3/F4/F5/F9/Alt+Enter, window resize to natural size, fit, fullscreen — files: `window_pc.cpp`, `devtools_pc.cpp`, `app_pc.cpp` — covers FR-1, FR-2, FR-3
- [ ] **T3.2** title notice — covers FR-8

## Stage 4 — settings and scripted runs

- [ ] **T4.1** settings file load/save (pref path, per game), skipped for scripted runs — files: `window_pc.cpp` — covers FR-6, FR-7
- [ ] **T4.2** `NNH_SIM_LAYOUT` / `NNH_SIM_SCALE`; screenshots of `examples/sprites` in each layout — covers FR-7, AC-2

## Stage 5 — docs and closing

- [ ] **T5.1** docs: `docs/api/simulator.md` (layouts, keys, settings, screenshots), `docs/tech/sdl3.md` (targets, composition), `docs/api/input.md` (touch in layouts) — covers AC-4
- [ ] **T5.2** budgets, Linux gcc + clang ASan/UBSan, 3DS build unaffected — covers AC-4
- [ ] **T5.3** maintainer: layouts, scales, fullscreen, touch in each layout, settings restored — covers AC-3
- [ ] **T5.4** `docs/ROADMAP.md` checkbox; spec, plan and tasks status → implemented
