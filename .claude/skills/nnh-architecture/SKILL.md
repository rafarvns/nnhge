---
name: nnh-architecture
description: NNHGE architecture rules. Use when designing or placing engine/game code — adding a feature or module, deciding between an event and a direct call, choosing a design pattern, or deciding which folder/file code belongs in. Covers event bus + scenes, approved patterns, layers and file-size rules.
---

# NNHGE architecture

Full docs: `docs/architecture/` (README, events, scenes, patterns, project-structure, decisions).
Constitution: `docs/process/constitution.md`. Read the relevant page before a non-trivial design.

## Mental model

**Query for state, publish events for changes.** Modules/scenes talk through a module's public API
(queries, commands it owns) or the event bus (facts, requests nobody owns alone). Never reach into
another module's internals or hold pointers to other scenes.

## Core + platform libraries (D15, D16)

Portable `nnh::core` + one static `nnh::platform_<system>` per system implementing the platform contract
(`nnh/platform/*.hpp`), linked by the preset — no virtual dispatch. System-only features: capability
queries (`nnh::system::has`) or explicit extension headers `<nnh/<system>/...>`. Never `#ifdef` a system
in core or game code. Design: `docs/architecture/platforms.md` (split happens in spec 002).

## Foundation services (spec 002)

Use them instead of ad-hoc code: `NNH_LOG_*` (never `printf` for diagnostics), `NNH_ASSERT`/`VERIFY`/`PANIC`
for bugs, `nnh::Result<T>` for expected failures, tagged allocators, `NNH_PROFILE_SCOPE`, `nnh::fs` for
every file access (confined to the game folder, atomic saves). Pages: `foundation.md`, `logging.md`,
`errors.md`, `debugging.md`, `security.md` in `docs/architecture/`.

## Layers (dependencies point down only)

game (scenes) → public API `engine/include/nnh/<module>.hpp` → modules `engine/src/<module>/` →
platform interface `engine/src/platform/platform.hpp` → backends `engine/src/platform/{3ds,pc,fake}/`.

## Events

- Event = past-tense fact, 0..n listeners (`CoinCollected`). Request = imperative, one owner (`PlaySound`),
  only when the sender must not know the receiver. Never use events for queries.
- Trivially copyable small structs (≤ 32 bytes target). One struct per meaning, no generic `GameEvent`.
- `subscribe<T>` → RAII `Subscription`; `publish<T>` queues; dispatch once per frame (after input,
  before scene update), FIFO, single thread. Re-entrancy: removed listener not called; added listener
  gets later publishes; depth limit stops loops.
- No RTTI, no per-frame heap, fixed capacities.
- Don't publish continuous state every frame; query it.

## Scenes (State pattern)

`on_enter/on_exit/update/draw`; `update` changes state, `draw` only reads. Scene stack push/pop/replace,
applied at end of frame. Subscriptions and resources are scene members (RAII). One scene per file pair.

## Approved patterns

Game Loop, Observer/Event Bus, Event Queue, State, Facade (public headers), link-time Bridge
(platform.hpp + one backend per target, no virtual), RAII handles, Object Pool, Command (input, later).

Anti-patterns: singletons/hidden globals in game code, god objects, deep inheritance, `virtual` per object,
platform `#ifdef` in game code or public headers, generic events, giant files.

## Where code goes

| Adding | Location |
|--------|----------|
| Engine feature | new module (use the `nnh-new-module` skill) |
| Platform-specific code | `engine/src/platform/<target>/<module>_<target>.cpp` behind `platform.hpp` |
| Internal API another module needs | `engine/src/<module>/<module>_internal.hpp` |
| Test | `tests/<module>/test_<concept>.cpp` |
| Demo | `examples/<feature>/` |

## File rules

One concept per file. `.cpp` aim ≤ 200 lines, hard 400. Headers aim ≤ 150. Functions aim ≤ 40 lines.
One public header per module. Explicit CMake source lists. Split by concept before a file grows past the aim.

## Checklist before proposing a design

- [ ] Which module owns the concept? Is it new (→ spec + new module)?
- [ ] Event or direct call? (fact/request vs. query/command)
- [ ] Which approved pattern applies? Any anti-pattern?
- [ ] Files and folders named per project-structure.md
- [ ] 3DS cost: memory, per-frame allocations, virtual calls
- [ ] New decision? Add a row to `docs/architecture/decisions.md`
