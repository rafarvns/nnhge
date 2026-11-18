# Plan 001: Event bus and scenes

Status: implemented (2026-11-17)
Spec: [spec.md](spec.md)

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | Pure core code; no platform work except nothing — the bus and scenes live entirely in `nnh::core` |
| II 3DS constraint | ✅ | Default bus ≈ 26 KB (16 KB queue + 256 × ~40 B subscribers), allocated once at `init()` from tag `events` |
| III Build speed | ✅ | Templates kept tiny in public headers (`subscribe<T>`, `publish<T>`, small callable); measured at the end |
| IV Event-driven | ✅ | This spec implements it |
| V No hidden costs | ✅ | No RTTI (type id = address of a per-type tag), no heap per publish/dispatch, `virtual` only per scene |
| VI Spec + test first | ✅ | Pure building blocks first, each with unit tests; fake platform for the frame-level rules |
| VII Small files | ✅ | Queue, subscriber table, bus, small callable, scene stack, run loop: separate files |
| VIII Documented API | ✅ | `docs/api/events.md`, `docs/api/scene.md`, `examples/scenes` |
| IX Clean and legal | ✅ | Own implementation (small callable inspired by the well-known "inplace function" idea, no code copied) |
| X Safe by default | ✅ | Overflow and misuse caught by asserts/logs; destroying a scene that is still on the stack is caught |

## Delivery in stages

| Stage | Content | Covers |
|-------|---------|--------|
| 1 | Building blocks: type ids, small callable, event queue, subscriber table (pure, unit-tested) | FR-1, FR-5 (no heap) |
| 2 | Bus API: `subscribe`/`publish`/`Subscription`, dispatch in `nnh::frame()`, re-entrancy, overflow, configurable capacities | FR-2–FR-6 |
| 3 | Input events: `ButtonPressed`, `ButtonReleased` | FR-7 |
| 4 | Scenes: `nnh::Scene`, scene stack (push/pop/replace, deferred), draw order, `nnh::run()` | FR-8–FR-10 |
| 5 | `examples/scenes`, docs, 3DS check, build budgets | AC-2–AC-5 |

## Design

### Public API (sketch)

```cpp
// <nnh/events.hpp>
struct CoinCollected { int value; };                       // any trivially copyable struct (FR-1)

nnh::Subscription sub = nnh::events::subscribe<CoinCollected>(
    [this](const CoinCollected& e) { score_ += e.value; }); // small callable, no heap (FR-2)
nnh::events::subscribe<CoinCollected>(this, &Hud::on_coin);  // member function helper
nnh::events::publish(CoinCollected{10});                    // queued, delivered next dispatch (FR-3)
sub.reset();                                                 // or let it go out of scope

// <nnh/app.hpp> — capacities (FR-5, decision c)
nnh::init({.title = "My Game", .events = {.queue_bytes = 32 * 1024, .max_subscribers = 512}});

// <nnh/scene.hpp>
class Level final : public nnh::Scene {
    void on_enter() override;  void on_exit() override;
    void update() override;    void draw() override;
};
Title title; Level level;                  // the game creates and owns its scenes (decision b)
nnh::scenes::push(title);                  // deferred to the end of the frame
nnh::scenes::replace(level);  nnh::scenes::pop();
nnh::run(title);                           // pushes `title`, runs until the stack is empty or the app quits
```

### Event bus internals

- **Type id**: `detail::type_id<T>()` returns the address of an `inline` per-type tag variable — unique per
  type, constant, no RTTI. `static_assert(std::is_trivially_copyable_v<T>)` on publish and subscribe.
  Events up to 256 bytes (`static_assert`, see events.md: large data goes elsewhere, send an id).
- **Small callable** (`nnh/detail/small_function.hpp`): stores a callable in 16 bytes of inline storage
  (enough for a lambda capturing two pointers or `this` + a member pointer). Too large a capture →
  `static_assert` with "capture too large for nnh::events handlers (max 16 bytes): capture a pointer
  instead". Typed adapter turns `void(const T&)` into `void(const void*)`.
- **Event queue** (`events/event_queue.hpp`): one ring buffer of bytes, allocated at `init()` through
  `nnh::mem::alloc(Heap, events)`. Record = `{type id, size, sequence, depth}` header + payload,
  8-byte aligned. Pure class over a caller-provided buffer → unit-testable without the engine.
- **Subscriber table** (`events/subscriber_table.hpp`): fixed array (`max_subscribers`) of slots
  `{type id, callable, generation, since_sequence, active}`; lookup by linear scan per event type
  (≤ 256 slots; good cache behaviour on ARM11). `Subscription` = `{slot, generation}`, move-only;
  destroying/resetting it frees the slot if the generation still matches (so a stale handle after
  `nnh::shutdown()` or a reused slot is harmless).
- **Dispatch** (`events/bus.cpp`): called by `nnh::frame()` after input is read and input events are
  published, before the frame returns to the game (so before the scene updates). Delivers queued
  records in FIFO order, including records appended during this dispatch. Re-entrancy rules (FR-4):
  - a slot removed during dispatch is marked inactive immediately → not called again;
  - a slot added during dispatch gets `since_sequence = next sequence` → only later publishes reach it;
  - each record carries `depth` (0 when published outside dispatch, parent + 1 inside); records deeper than
    8 are dropped with a warning naming the event chain's type ids.
- **Overflow** (FR-6): queue full or subscriber table full → `NNH_ASSERT` in Debug/Development; in Release
  the event/subscription is dropped and a warning is logged once per frame.
- **Lifecycle**: `init()` allocates and resets; `shutdown()` drops pending events, bumps every slot's
  generation and frees the memory (leak report stays clean).

### Scenes internals

- `nnh::Scene`: `virtual ~Scene()`, `on_enter()`, `on_exit()`, `update()`, `draw()`; non-copyable. The
  destructor checks it is not on the stack (`NNH_VERIFY`, "scene destroyed while still on the scene stack")
  — the game owns scenes, so this catches lifetime bugs (decision b).
- Scene stack (`scene/scene_stack.hpp`): fixed array of 8 non-owning pointers + a queue of up to 8
  pending operations. Pure logic, unit-tested with test scenes that record calls.
- Pending operations are applied at the **start of the next `nnh::frame()`** (i.e. the end of the frame
  that requested them), in order: `pop` → top `on_exit`; `push` → new `on_enter`; `replace` = pop + push.
  Pushing a scene already on the stack is a bug (`NNH_VERIFY`).
- `nnh::scenes::update_and_draw()`: top scene `update()`, then every scene `draw()` bottom to top (FR-9).
- `nnh::run(Scene& first)`: `push(first)`, then `while (nnh::frame() && !empty) update_and_draw();`, then
  pops everything (each `on_exit`). The game still calls `init()`/`shutdown()`. A hand-written
  `while (nnh::frame())` loop keeps working (FR-10, AC-3).

### Events introduced

| Event | Published by | Payload | Typical listeners |
|-------|--------------|---------|-------------------|
| `nnh::ButtonPressed` | input module, once per button that went down this frame | `Button button` | scenes, menus |
| `nnh::ButtonReleased` | input module, once per button that went up | `Button button` | scenes |

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/events.hpp` | new | `subscribe`, `publish`, `Subscription`, limits |
| `engine/core/include/nnh/detail/small_function.hpp` | new | fixed-size callable |
| `engine/core/include/nnh/detail/type_id.hpp` | new | per-type ids without RTTI |
| `engine/core/include/nnh/scene.hpp` | new | `Scene`, `scenes::*`, `run` |
| `engine/core/include/nnh/app.hpp` | changed | `AppConfig::events` capacities |
| `engine/core/include/nnh/input.hpp` | changed | `ButtonPressed`, `ButtonReleased` |
| `engine/core/src/events/event_queue.hpp/.cpp` | new | byte ring buffer of records |
| `engine/core/src/events/subscriber_table.hpp/.cpp` | new | slots, generations, sequences |
| `engine/core/src/events/bus.cpp` | new | public API, dispatch, overflow |
| `engine/core/src/events/events_internal.hpp` | new | init/shutdown/dispatch hooks for `app.cpp` |
| `engine/core/src/scene/scene_stack.hpp/.cpp` | new | stack + pending operations |
| `engine/core/src/scene/scenes.cpp` | new | public API, `run()` |
| `engine/core/src/app/app.cpp`, `input/input.cpp` | changed | call dispatch / apply scene changes; publish input events |

### Data and memory

Defaults: queue 16 KB + subscribers 256 × ~40 B ≈ 26 KB on the heap under tag `events`, allocated once.
Scene stack: fixed, < 200 B. Nothing allocated per frame.

### Platform work

None: everything is in `nnh::core`. The fake platform drives the frame-level tests.

## Test strategy

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `tests/events/test_type_id.cpp` (distinct ids, trivially-copyable check is compile-time) |
| FR-2 | `tests/events/test_small_function.cpp`, `tests/events/test_subscriptions.cpp` (RAII, move, reset, stale handles) |
| FR-3 | `tests/events/test_event_queue.cpp` (FIFO, wrap-around, alignment), `tests/events/test_dispatch.cpp` (delivery point in the frame) |
| FR-4 | `tests/events/test_reentrancy.cpp` — one test per rule (nested publish, remove during dispatch, add during dispatch, depth limit) |
| FR-5 | `tests/events/test_capacities.cpp` (defaults, configured values, memory tag usage, no allocation per publish) |
| FR-6 | `tests/events/test_overflow.cpp` (assert via `run_expecting_crash` in Debug/Dev; drop + log in Release) |
| FR-7 | `tests/events/test_input_events.cpp` (fake platform buttons → events) |
| FR-8, FR-9 | `tests/scene/test_scene_stack.cpp` (deferred ops, enter/exit order, draw order, double push, destroy while on stack) |
| FR-10 | `tests/scene/test_run.cpp` (run ends when the stack empties or the app quits; manual loop unaffected) |
| AC-2, AC-3 | `examples/scenes` screenshots (title, level, pause) + run on 3DS; `hello` unchanged |
| AC-5 | build times measured before/after |

## Risks and alternatives

- **Re-entrancy bugs** are the main risk → each rule has its own test; mutation checks on the rule code.
- **Handler capture limit** (16 bytes) may feel tight → clear compile error pointing to "capture a pointer";
  the limit is a named constant and can grow if real games need it.
- **Scene lifetime with game-owned scenes** → the destructor check catches the common mistake.
- **Template bloat / build time** → small headers; templates only wrap a `void*`-based core.
- Alternatives rejected: `std::function` (may allocate — decision a); engine-constructed scenes
  `push<Level>()` (decision b); per-type queues (more memory, more code).

## Constitution Check (after design)

Unchanged: all ✅.
