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
| **Batch draw command** (deferred expansion) | tile layers (spec 012, D26) and particle emitters (spec 024, D39; [particles.md](../api/particles.md#drawing)): one draw-list command per batch, expanded at submit | Many small quads keep their place among the sprites (layer, then call order) while skipping a record, a sort entry and a texture lookup each; the batch's owner is read at submit (the camera and state at the end of the frame), culls what the view can't show, and writes into one small shared scratch within a per-frame budget (`max_tiles`, `max_particles`). gfx sees only `{expand function, source}`, never the owning module; a destroyed source is forgotten. |
| **RAII handles** | `Subscription`, textures, sounds | Resources and subscriptions are released automatically; no manual cleanup lists. |
| **Object Pool** | bullets, particles, entities: `<nnh/pool.hpp>` ([api/pool.md](../api/pool.md)) and `<nnh/entities.hpp>` ([api/entities.md](../api/entities.md)), spec 020, D35 | No heap allocation per frame; predictable memory on 3DS. |
| **Prototype** | prefabs: `<nnh/prefabs.hpp>` ([api/prefabs.md](../api/prefabs.md)), spec 021, D36 | A kind of thing is declared once, by name, as component values (variants flattened from a base) and spawned by copying them: no spawn function per kind, levels name what to spawn. |
| **Transform hierarchy (Composite)** | nodes: `<nnh/hierarchy.hpp>` over `<nnh/transform.hpp>`, spec 022, D37 (implemented) | Parent/child entities whose local transforms follow their parent: one explicit pass computes world transforms (no dirty flags, no per-object `virtual`), destroying a parent destroys its subtree through the world's destroy queue. |
| **Optional frame hook** | scene attachments (D30), after-input hooks (D32), the scene stack's transition hook and the input filter (spec 023, D38, implemented), the 3D pass reached by gfx through `detail::g_gfx_3d_ops` (spec 026, D43, design); localization's language switch as an after-input hook and game fonts reached by gfx and the UI through `detail::g_font_resolve`, set by the first `load_font` (spec 029, D49, D50, implemented); the text layout path reached by `draw_text` through a weak reference on ELF targets, linked by `load_font` and `measure_text(utf8, options)` (spec 029, lead decision) | An optional module takes over or extends one step of the frame through a function pointer (or an intrusive list) that is null unless the module is in use: core never names the module, games that don't use it pay a null check and link nothing, and no `virtual` is needed. |
| **Time-sliced job** (with **Double Buffer** for its readers) | path searches and distance fields: `begin` / `step(n)` / finished (spec 025, D40: `<nnh/pathfinding.hpp>`, [api/pathfinding.md](../api/pathfinding.md)) | Long work spreads over frames with a bounded cost per call: any slice sizes give exactly the one-shot result; a change to the input meanwhile flags the result stale (never undefined behaviour); readers keep answering from the last finished result while the next one is built. |
| **Versioned schema** (stable field ids + migration chain) | structured saves: `NNH_SAVE_TYPE` with `NNH_SAVE_FIELD(member, id)`, `NNH_SAVE_RETIRED`, `NNH_SAVE_STEP` (spec 027, D45, D46; [saves.md](../api/saves.md)) | Data that outlives a build is written field by field under ids that never change meaning (never by struct layout or member name), so added, removed, renamed and reordered fields load without code; a version number runs one small conversion per version, in order, for what can't be automatic. The reader validates everything before it changes anything. |
| **Retained widget tree** (Composite, with focus navigation) | UI widgets: `<nnh/ui.hpp>` (spec 028, D48; [api/ui.md](../api/ui.md)) | Menus are built once in code as a tree of plain records in one block and changed by setters; layout is computed in integers only when something changed, focus moves by fixed rules (neighbour, box axis, geometric fallback) and is remembered per container, and the tree draws itself through the same public gfx calls a hand-written menu makes. State that must survive frames (focus memory, repeats, scroll) lives in the tree, not in the game. |
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
