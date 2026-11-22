# Architecture

NNHGE is an **event-driven engine** made of a **portable core** plus **one library per system** that
plugs into it, with games split into **scenes**.

## Mental model

> **Query for state, publish events for changes.**
>
> "Is A held?" → ask the module (`nnh::input::held(Button::A)`).
> "A was pressed", "the player died", "load the next level" → publish an event; whoever cares subscribes.

Modules and scenes never reach into each other. They talk through their **public API** (queries and
commands owned by that module) or through the **event bus** (facts and requests nobody owns alone).

## Layers

```
 game code (scenes)                         your game: examples/, your project
        │  <nnh/...>  (+ optional <nnh/<system>/...> extensions)
 nnh::core   public API + modules           portable: loop, events, scenes, log, memory, fs rules
        │  platform contract (nnh/platform/*.hpp), resolved at link time
 nnh::platform_<system>                     pc (SDL3), 3ds (libctru/citro2d), fake (tests), later switch...
        │
 foundation inside core: time · log · assert/result · memory · fs · debug · system
```

Dependencies only point **down**. Core never includes a system SDK header (checked in CI).

## Frame flow

```
nnh::frame()
 ├─ present previous frame, wait vsync (60 Hz)
 ├─ platform: poll system events, read input
 ├─ input module: compute pressed/released → publish input events
 ├─ after-input hooks (D32): input action sets evaluated → ActionPressed/ActionReleased queued
 ├─ event bus: dispatch queued events (one fixed point, single thread)
 └─ scene stack: active scene update() → draw()      (scenes arrive with spec 001)
```

## Documents

| Document | What it covers |
|----------|----------------|
| [platforms.md](platforms.md) | Core + platform libraries, the platform contract, capabilities and extensions |
| [foundation.md](foundation.md) | Core foundation: time, build flavors, memory; index of the foundation modules |
| [logging.md](logging.md) | Logging levels, channels, sinks |
| [errors.md](errors.md) | `Result`, asserts, crash screen and report |
| [debugging.md](debugging.md) | Overlay, profiler, simulator controls, debuggers |
| [security.md](security.md) | Threat model, confined files, atomic saves, fuzzing, supply chain |
| [events.md](events.md) | The event bus: event types, publish/subscribe, dispatch rules |
| [scenes.md](scenes.md) | Scenes and the scene stack (State pattern) |
| [rendering.md](rendering.md) | Recorded draw list, passes per screen, one texture type: 2D now, 3D later |
| [patterns.md](patterns.md) | Approved design patterns and anti-patterns |
| [project-structure.md](project-structure.md) | Folders, file rules, naming, where code goes |
| [decisions.md](decisions.md) | Every architecture decision and why |

How we work (SDD, TDD, SOLID): [../process/](../process/). Technology notes: [../tech/](../tech/).
