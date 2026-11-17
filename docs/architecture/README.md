# Architecture

NNHGE is an **event-driven engine** organized in layers, with games split into **scenes**.

## Mental model

> **Query for state, publish events for changes.**
>
> "Is A held?" → ask the module (`nnh::input::held(Button::A)`).
> "A was pressed", "the player died", "load the next level" → publish an event; whoever cares subscribes.

Modules and scenes never reach into each other. They talk through their **public API** (queries and
commands owned by that module) or through the **event bus** (facts and requests nobody owns alone).

## Layers

```
 game code (scenes)              your game: examples/, your project
        │  uses only <nnh/...>
 public API (engine/include/nnh) one header per module: app, gfx, input, debug, events, scene, ...
        │
 engine modules (engine/src/<module>)   platform-independent logic, unit tested
        │  depends only on the platform interface
 platform backends (engine/src/platform/<target>)   3ds (libctru/citro2d), pc (SDL3), fake (tests)
```

Dependencies only point **down**. Lower layers never include headers from higher ones.

## Frame flow

```
nnh::frame()
 ├─ present previous frame, wait vsync (60 Hz)
 ├─ platform: poll system events, read input
 ├─ input module: compute pressed/released → publish input events
 ├─ event bus: dispatch queued events (one fixed point, single thread)
 └─ scene stack: active scene update() → draw()      (scenes arrive with spec 001)
```

## Documents

| Document | What it covers |
|----------|----------------|
| [events.md](events.md) | The event bus: event types, publish/subscribe, dispatch rules |
| [scenes.md](scenes.md) | Scenes and the scene stack (State pattern) |
| [patterns.md](patterns.md) | Approved design patterns and anti-patterns |
| [project-structure.md](project-structure.md) | Folders, file rules, naming, where code goes |
| [decisions.md](decisions.md) | Every architecture decision and why |

How we work (SDD, TDD, SOLID): [../process/](../process/). Technology notes: [../tech/](../tech/).
