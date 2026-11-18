# Tasks 001: Event bus and scenes

Status: implemented (2026-11-17); T5.4 (run `examples/scenes` on a 3DS) is the maintainer's
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each stage ends green: PC (Debug + Release) and 3DS builds, tests, docs updated (nnh-docs); Linux checks on
the homelab build VM when useful. `[P]` = parallelizable.

## Stage 1 — Building blocks (FR-1, FR-5)
- [x] **T1.1** `nnh/detail/type_id.hpp` — test: `tests/events/test_type_id.cpp` — covers FR-1
- [x] **T1.2** [P] `nnh/detail/small_function.hpp` (16-byte inline storage, typed adapter, clear static_assert) — test: `tests/events/test_small_function.cpp` — covers FR-2, FR-5
- [x] **T1.3** [P] `events/event_queue` (records, FIFO, wrap-around, alignment, full) — test: `tests/events/test_event_queue.cpp` — covers FR-3, FR-5
- [x] **T1.4** [P] `events/subscriber_table` (slots, generations, since-sequence) — test: `tests/events/test_subscriber_table.cpp` — covers FR-2, FR-4

## Stage 2 — Bus API (FR-2–FR-6)
- [x] **T2.1** `<nnh/events.hpp>`, `events/bus.cpp`, `AppConfig::events`; allocation at init (tag `events`), release at shutdown — test: `tests/events/test_capacities.cpp`
- [x] **T2.2** `Subscription` RAII/move/reset/stale handles — test: `tests/events/test_subscriptions.cpp`
- [x] **T2.3** dispatch in `nnh::frame()` (after input, before returning to the game) — test: `tests/events/test_dispatch.cpp`
- [x] **T2.4** re-entrancy rules + depth limit — test: `tests/events/test_reentrancy.cpp` — covers FR-4
- [x] **T2.5** overflow: assert (Debug/Dev) / drop + log (Release) — test: `tests/events/test_overflow.cpp` — covers FR-6
- [x] **T2.6** `docs/api/events.md`; `docs/architecture/events.md` status → implemented

## Stage 3 — Input events (FR-7)
- [x] **T3.1** `ButtonPressed` / `ButtonReleased` published by the input module — test: `tests/events/test_input_events.cpp`
- [x] **T3.2** `docs/api/input.md` events section

## Stage 4 — Scenes (FR-8–FR-10)
- [x] **T4.1** `nnh::Scene` + `scene/scene_stack` (deferred push/pop/replace, enter/exit order, double push, destroy-while-on-stack check) — test: `tests/scene/test_scene_stack.cpp`
- [x] **T4.2** `scenes::update_and_draw` (top updates, all draw bottom → top) — test: `tests/scene/test_scene_stack.cpp`
- [x] **T4.3** `nnh::run(first)` — test: `tests/scene/test_run.cpp`
- [x] **T4.4** `docs/api/scene.md`; `docs/architecture/scenes.md` status → implemented

## Stage 5 — Example and closing (AC-2–AC-5)
- [x] **T5.1** `examples/scenes`: title → level (score via `CoinCollected` event) → pause on START (pushed over the level); screenshots of each scene
- [x] **T5.2** `hello` unchanged; Release strings check; 3DS builds
- [x] **T5.3** build-time measurements in ROADMAP
- [ ] **T5.4** run `examples/scenes` on a 3DS — maintainer
- [x] **T5.5** ROADMAP checkbox; spec/plan/tasks → implemented
