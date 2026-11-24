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
| **Object Pool** | bullets, particles, entities: `<nnh/pool.hpp>` ([api/pool.md](../api/pool.md)) and `<nnh/entities.hpp>` ([api/entities.md](../api/entities.md)), spec 020, D35 | No heap allocation per frame; predictable memory on 3DS. |
| **Prototype** | prefabs: `<nnh/prefabs.hpp>` ([api/prefabs.md](../api/prefabs.md)), spec 021, D36 | A kind of thing is declared once, by name, as component values (variants flattened from a base) and spawned by copying them: no spawn function per kind, levels name what to spawn. |
| **Transform hierarchy (Composite)** | nodes: `<nnh/hierarchy.hpp>` over `<nnh/transform.hpp>`, spec 022, D37 (design) | Parent/child entities whose local transforms follow their parent: one explicit pass computes world transforms (no dirty flags, no per-object `virtual`), destroying a parent destroys its subtree through the world's destroy queue. |
| **Optional frame hook** | scene attachments (D30), after-input hooks (D32), the scene stack's transition hook and the input filter (spec 023, D38, design) | An optional module takes over or extends one step of the frame through a function pointer (or an intrusive list) that is null unless the module is in use: core never names the module, games that don't use it pay a null check and link nothing, and no `virtual` is needed. |
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
