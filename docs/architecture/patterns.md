# Design patterns

Use these patterns. If a problem seems to need something else, propose it in a spec's plan and add it
here once accepted.

## Approved patterns

| Pattern | Where | Why |
|---------|-------|-----|
| **Game Loop** | `nnh::frame()` | Fixed 60 Hz rhythm on every platform; one place where frame phases happen. |
| **Observer / Publish-Subscribe (Event Bus)** | `nnh::events` | Decouples whoever causes something from whoever reacts. See [events.md](events.md). |
| **Event Queue** | inside the bus | Decouples *in time*: events are delivered at one deterministic point per frame. |
| **State** | `nnh::Scene` + scene stack | Each game mode is a class; switching scenes replaces mode flags. See [scenes.md](scenes.md). |
| **Facade** | public headers `<nnh/*.hpp>` | Games see a small, stable API; platform and internals stay hidden. |
| **Bridge (link-time)** | `engine/core/contract/nnh/platform/*.hpp` + one backend per target | Core depends on an interface; the backend is chosen by the build, so there is no `virtual` cost. |
| **RAII handles** | `Subscription`, textures, sounds | Resources and subscriptions are released automatically; no manual cleanup lists. |
| **Object Pool** | bullets, particles, entities (later) | No heap allocation per frame; predictable memory on 3DS. |
| **Command** | input actions (later) | Map buttons to actions, enable remapping and replays. |

### Example: Bridge without virtual

```cpp
// engine/core/contract/nnh/platform/*.hpp   (the interface)
namespace nnh::platform { void read_input(InputSample& out); }

// engine/platforms/3ds/src/input_3ds.cpp  (one implementation per target, picked by CMake)
void nnh::platform::read_input(InputSample& out) { hidScanInput(); /* ... */ }
```

The frame loop calls `platform::read_input()` without knowing which platform it runs on.

## Anti-patterns (don't)

| Don't | Do instead |
|-------|------------|
| Hidden global state / singletons in game code | Pass data explicitly; engine-owned services through `nnh::` APIs |
| "God" objects (a `Game` class that does everything) | Scenes + small modules with one responsibility |
| Deep inheritance trees, `virtual update()` per object | Data in arrays processed by a scene/system; `virtual` only per scene/system |
| `#ifdef __3DS__` in game code or public headers | Platform backend files; capability queries |
| Events used as queries ("send me your health") | Call a function |
| Generic catch-all events (`GameEvent{kind, data}`) | One small struct per event meaning |
| Giant files mixing several concepts | One concept per file, see [project-structure.md](project-structure.md) |

## Further reading

- Robert Nystrom, [*Game Programming Patterns*](https://gameprogrammingpatterns.com/contents.html) (free online).
- Brooke Hodgman, [OOP is dead, long live OOP](https://gamedev.net/blogs/entry/2265481-oop-is-dead-long-live-oop/) — interfaces without virtual-per-object.
- Richard Fabian, [*Data-Oriented Design*](https://www.dataorienteddesign.com/dodbook/) — why per-object virtual calls hurt on weak CPUs.
