# entities — worlds of entities, components and tags

Header: `<nnh/entities.hpp>` · Spec: [020](../../specs/020-entities/spec.md) ·
Plan: [020](../../specs/020-entities/plan.md) · Decision: [D35](../architecture/decisions.md) ·
Example: [`examples/entities`](../../examples/entities/arena.cpp) · In a game:
[`examples/platformer`](../../examples/platformer/level_play.cpp) · Pools: [pool.md](pool.md)

A `World` holds **entities** — 32-bit generational handles — whose **components** are the game's own plain structs (a
body, a velocity, a sprite animator, its `Health`), stored kind by kind in fixed arrays with **stable slots**: a
component's address never changes while it exists, so pointers, an animator's callback and a tween's target stay
valid, and other modules read a kind's storage **in place** (a collision box view over the bodies). Entities carry
**tags** hashed at compile time and are found by tag. Destroying is safe anywhere, loops included, and runs the
components' **remove hooks** first. A world bound to a scene is flushed with it every frame and emptied when it is
left. One block of tagged memory sized at creation; nothing allocates while playing; the same calls give the same
handles and visiting orders on PC and 3DS.

The module is optional (constitution XI): a game that doesn't include the header links none of it. It doesn't include
`<nnh/pool.hpp>`, collision, tweens, animation or graphics; they meet through the data.

```cpp
struct Body { nnh::Rect box{}; nnh::collision::Layers layers = nnh::collision::kNoLayers; };
struct Velocity { nnh::Vec2 v{}; };
constexpr nnh::Tag kEnemy = nnh::tag("enemy");
constexpr nnh::Tag kCoin = nnh::tag("coin");
static_assert(nnh::tags_distinct(kEnemy, kCoin));

class Level final : public nnh::Scene {
public:
    Level() {
        const nnh::ComponentDesc kinds[] = {
            nnh::component<Body>(256, [this](nnh::World& w, nnh::Entity e, Body& b) {
                if (!w.clearing()) explode_at(b.box); // a remove hook: the data is still there
            }),
            nnh::component<Velocity>(256),
        };
        world_ = std::move(nnh::World::create({.max_entities = 256}, kinds).value()); // checked in real code
        world_.bind(*this); // flushed before each update, cleared after on_exit()
    }
    void on_enter() override {
        const nnh::Entity e = world_.spawn();
        world_.add<Body>(e, {{40, 40, 16, 16}, kEnemyLayer});
        world_.add<Velocity>(e, {{1, 0}});
        world_.add_tag(e, kEnemy);
    }
    void update() override {
        world_.each<Velocity, Body>([](nnh::Entity, Velocity& v, Body& b) { b.box.x += v.v.x; });
    }
    void explode_at(nnh::Rect where);
    nnh::World world_;
};
```

## Worlds

`World::create(desc, kinds)` builds a world in one block: `WorldDesc{.max_entities (1..65 535), .tags_per_entity
(0..8, default 4), .memory_tag (default `entities`)}` and an array of component kinds (1..32). Errors come back as a
`Result`: `InvalidArgument` (a capacity or tag count out of range, no kind or more than 32, a kind listed twice, a kind
capacity outside 1..max_entities), `OutOfMemory`. Every component slot is constructed with `T{}` at creation. A world
is movable while unbound, not copyable; its destructor destroys every entity (remove hooks run, ascending slots) and
frees the block. A default-constructed or moved-from world is empty: commands assert, queries answer "nothing".
To make many entities of one kind — the same components, values and tags — declare it once as a
[prefab](prefabs.md) and spawn it by handle (or from a Tiled object): the result is bit for bit what the calls below
make.

Memory: a header and kind tables (≈ 200 + 90·kinds bytes on the 3DS), per entity `8 + 4·tags + 2·(kinds + 1)`
bytes rounded to 4 plus 2.5 bytes, per component slot `sizeof(T) + 2.5` bytes, plus one scratch `T` per kind.
1024 entities with 8 kinds of 1024 slots and 4 tags: ≈ 68 KB besides the components. The platformer's world (256
entities, 5 kinds, 2 tags, 96 sparkles): ≈ 18 KB with its components.

## Components and kinds

A component kind is any default-constructible type with a non-throwing destructor; it need not be copyable or movable
(a `SpriteAnimator` works). Register it with `nnh::component<T>(capacity)` or, with a **remove hook**,
`nnh::component<T>(capacity, [this](nnh::World& w, nnh::Entity e, T& c) { ... })` (captures ≤ 16 bytes, trivially
copyable, as event handlers). Kinds are identified without RTTI and ordered by registration (never by address).

| Call | What |
|------|------|
| `spawn()` | a new entity in the lowest free slot, no components, no tags; a full world returns null and asserts |
| `add<T>(e)` / `add<T>(e, value)` | the slot's `T{}` (or a copy of `value`) attached to `e`; null when `e` is stale; asserts for a full or unregistered kind, or when `e` already has a `T` (the existing one is returned unchanged) |
| `get<T>(e)` / `has<T>(e)` | the component or null; constant time, no search |
| `remove<T>(e)` | runs `T`'s hook, then the slot holds `T{}`, quarantined; false when stale, absent, dying, or its hook is running |
| `at<T>(e)` | checked access: asserts on a stale entity or a missing component; Release returns the kind's scratch `T{}` |
| `alive(e)` | whether `e` names a live entity (also during its own remove hooks) |

Free and destroyed slots hold `T{}`: give a body's layers member the default `collision::kNoLayers` and they are
invisible to every collision query.

## Destroy and remove hooks

`destroy(e)` works at any time, inside iterations and hooks included:

1. the **remove hooks** of `e`'s components run, in kind registration order, with every component still attached
   (`get` of other kinds works) — the place to stop a sound, kill a tween or spawn an explosion where the body was;
2. every component is destroyed (`T{}` in its slot, quarantined until the flush), the tags are removed and the handle
   is stale.

A destroy called from a hook is **queued** and finished before the outermost call returns (first in, first out, each
entity once), so cascades never recurse; inside that hook `alive(x)` stays true for an entity it just destroyed until
then. Hooks may spawn; `add` to an entity being destroyed asserts and is refused; `remove<T>` of a component whose hook
is running returns false. Destroying a stale or dying entity returns false (two bullets on one enemy is normal).

The engine publishes **no events** for spawns, destroys or component changes (constitution IV): state is queried, and
an engine event delivered at the next dispatch would carry an already stale handle. Publish your own facts from a hook
or from game code, and check the handle on delivery:

```cpp
struct EnemyDefeated { nnh::Entity who; int score; };
nnh::component<Health>(64, [](nnh::World& w, nnh::Entity e, Health& h) {
    if (!w.clearing()) nnh::events::publish(EnemyDefeated{e, h.score});
});
// a listener: `if (world.alive(ev.who)) ...` is false by then — the handle identifies, it doesn't keep alive
```

## Flush and clear

Destroyed entities and removed components leave their slots **quarantined** until `flush()`, so nothing created in the
same frame takes their place while loops and views of that frame run. `clear()` destroys every entity in ascending
slots (hooks run; `spawn()` returns null without asserting while `clearing()` is true) and flushes, so a level rebuilt
after `clear()` gets the slots of its first load (with new generations). Both assert and are ignored inside an
iteration or a hook. Stats (`stats()`: entities, peak, capacity, spawn and add misses; `storage<T>().stats()`: live,
peak, capacity, misses) count since creation.

## Iteration

`each<A, B, ...>(fn)` calls `fn(nnh::Entity, A&, B&, ...)` for every live entity that has **all** the listed kinds, in
**ascending slot order of the first kind's storage** — list the rarest kind first for speed; the order rule never
depends on counts, so replays see the same order. Destroying, removing, spawning and adding inside are safe: what is
destroyed or removed is not visited afterwards, and entities or components created during the iteration are not
visited by it. The callable is inlined (no stored function, no allocation). `for (nnh::Entity e : world.entities())`
visits every live entity with the same rules. Iterations nest; `break` is fine.

## Storage views

`world.storage<T>()` returns a `Storage<T>`: the kind's array, valid for the world's life, the kind lookup done once.

| Call | What |
|------|------|
| `data()`, `extent()`, `stride()` | slot 0, one past the highest slot used since the last clear, `sizeof(T)` |
| `is_live(slot)`, `owner(slot)` | whether a slot holds a live component; its entity (null otherwise) |
| `slot_of(e)`, `get(e)` | `e`'s slot (−1) / component (null); the fastest lookup by handle (no kind scan) |
| `items()` | `for (auto [e, component] : view.items())`, ascending slots, `each`'s rules |
| `count()`, `capacity()`, `stats()` | as the world's per kind |

**Box views (spec 019):** a body storage feeds the collision queries in place, and a result's box index is a slot:

```cpp
const nnh::Storage<Body> bodies = world.storage<Body>();
const auto view = nnh::collision::box_view(bodies.data(), bodies.extent(), &Body::box, &Body::layers);
int hit = -1;
if (nnh::collision::boxes_at(view, point, &hit, 1, {.mask = kEnemyLayer}) > 0) world.destroy(bodies.owner(hit));
```

## Tags and groups

`nnh::Tag` is the FNV-1a 32 hash of a name (the hash of data paths and sprite names): `nnh::tag("enemy")` at compile
time (a `constexpr` constant, a `case` label) or `nnh::tag(text, size)` at run time from a string the game read (a
Tiled object's type), with the same bits. Comparing tags is comparing integers; the engine stores no names (Release
binaries carry no tag strings). `static_assert(nnh::tags_distinct(kEnemy, kCoin, kFlying))` rules out a collision
among your tags (and `kNoTag`, which is never a tag).

| Call | What |
|------|------|
| `add_tag(e, t)` | false when stale or already there; asserts when `e` has `tags_per_entity` tags, or for `kNoTag` |
| `remove_tag(e, t)`, `has_tag(e, t)` | |
| `find(t, out, capacity)` | the live entities tagged `t` in ascending slots into `out`; returns the total (may exceed `capacity`; `nullptr, 0` just counts) |
| `first(t)` | the lowest-slot one, or null |
| `each_tagged(t, fn)` | `fn(Entity)` for each, with `each`'s rules |

Godot's **groups** are these tags: "call a group" is `each_tagged`. Pools have no tags: a pool is already a group of
one kind.

## Scene binding

`world.bind(scene)` attaches the world through [D30](../architecture/decisions.md)'s scene attachments: right before
the scene's `update()` (only while it is on top) the world is **flushed**; right after its `on_exit()` it is
**cleared** (hooks run; test `clearing()` in hooks that start effects). Bind the world **before** the tween sets its
hooks use: attachments run in attach order, so its hooks see live tweens and may kill them, and the set is cleared
afterwards. A bound world must not move (asserts, nothing moves) or outlive its scene; destroying it detaches it.
Unbound worlds are flushed and cleared by the game.

## Animators and tweens as components

- A `SpriteAnimator` is a component like any other: stable address, updated in place
  (`world.each<SpriteAnimator>([](nnh::Entity, nnh::SpriteAnimator& a) { a.update(); })`).
- A tween may target a component **through a setter that captures the world and the handle** (recommended:
  stale-safe), or by pointer while the component lives, with the kind's remove hook killing it:

```cpp
nnh::World* w = &world_;
tweens_.from_to(nnh::Setter<float>{[w, e](float v) { if (Fade* f = w->get<Fade>(e)) f->alpha = v; }},
                1.0f, 0.0f, nnh::Frames{24}, {.on_finish = [w, e] { w->destroy(e); }});
```

A tween set stays one per scene, not a component.

## Determinism and costs

Integer state only, fixed rules: lowest free slot, ascending iteration, hooks in registration order, quarantine until
flush, generations +1 per destroy. Type ids are keys, never ordered. The self-check of `examples/entities` (100 000
operations, golden `C8FB9A41`) passes on every host compiler and must on the console.

After creation nothing allocates, logs in the normal path or calls `virtual` functions; per-kind construct, destroy and
hook functions are plain function pointers called once per component created or destroyed. `get` and `alive`: a
bound check, one load and one compare, plus the kind lookup for `get` — one compare each for the first four kinds
registered (register the kinds you look up most first), a scan for the rest. For loops of lookups take
`storage<T>()` once: its `get` keeps the record layout in the view (19 ARM instructions against 33). Random lookups
over more than the Old 3DS's 16 KB data cache are bound by memory, not instructions.
`spawn`/`add`: one scan of a free bitmap from a hint word. Iterations: proportional to the first kind's extent. `find`:
proportional to the entity extent × tags per entity.

Measured on an Old 3DS (Development build, `examples/entities` bench, average per frame):

| Workload | Time | Per element |
|---|---|---|
| `each<Body, Velocity>` over 1024 entities, box += velocity | 0.48 ms | 0.46 µs |
| 256 × (spawn, 3 `add`, 2 `add_tag`), 256 `destroy`, one `flush` | 1.30 ms | 5.1 µs per entity |
| `find` of one tag over 1024 entities × 4 tags | 0.14 ms | 0.14 µs per entity |
| 10 000 `get<Body>` on random handles | 4.77 ms | 0.48 µs |
| the same through `storage<Body>().get` | 4.20 ms | 0.42 µs |

Random lookups are bound by memory: here the handles reach 12 KB of records and 20 KB of bodies, and each lookup
misses the cache about twice. Keep a frame's lookups few, or walk the kind you need with `each` or a storage view.

## Pitfalls

- **A callback stored inside the component it destroys** (an animator's `on_notice` lambda that destroys its own
  entity) loses its captures once `destroy()` returns: the slot holds a fresh `T{}`. Copy what it needs into locals
  first and make `destroy()` the last thing it does, or destroy after `update()` from the notices it returns.
- **Hooks of an unbound world's destructor** run while the object that owns the world is being destroyed: declare the
  world after the members its hooks use (members are destroyed in reverse order).
- **A handle in an event** may be stale by the time it is delivered (next frame): `alive(e)` tells.
- **Generation wrap:** as for pools, a handle kept across 65 535 reuses of its slot reads as live again
  ([pool.md](pool.md#handles)). Handles carry no world id: a handle used with another world is caught only by its
  generation.

## Misuse

Asserts in Debug/Development, harmless in Release (nothing happens or nothing is answered): an unregistered kind, a
null buffer with capacity > 0 or a negative capacity in `find`, `flush`/`clear` inside an iteration or a hook, moving
a bound world, a command on an empty or moved-from world, `at` on a stale entity, adding to a dying entity,
`add_tag(kNoTag)` or past `tags_per_entity`, `bind` twice or while iterating/clearing, destroying a world inside
its own iteration or hook (Release frees it when the outermost operation ends, without more hooks). A full world
asserts too: its capacity is the game's budget (pools are where "full" is normal).

## The engine's node kind (spec 022)

The engine ships exactly one component kind: `nnh::Node` (`nnh::node_component(capacity)`), which puts an entity in a
[hierarchy](hierarchy.md). It is registered like any kind; its remove hook makes destroying a parent destroy its
children through the destroy queue (breadth-first, each descendant's hooks with its data intact) and removing it make
the children roots. Worlds without it are unchanged.
