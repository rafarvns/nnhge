# prefabs — named prototypes of entities

Header: `<nnh/prefabs.hpp>` · Spec: [021](../../specs/021-prefabs/spec.md) ·
Plan: [021](../../specs/021-prefabs/plan.md) · Decision: [D36](../architecture/decisions.md) ·
Example: [`examples/prefabs`](../../examples/prefabs/room.cpp) · In a game:
[`examples/platformer`](../../examples/platformer/level_prefabs.cpp) · Worlds: [entities.md](entities.md)

A **prefab** is a thing declared once, by name: a list of component values (the game's own structs), up to 8 tags, an
optional **placement member** (where a spawn's position goes) and an optional **spawn hook** (what values can't say),
optionally **based on** another prefab. Prefabs live in a `PrefabLibrary` the game owns; `spawn` makes one entity in a
spec 020 `World` — **bit for bit** what `spawn()`, `add<T>(e, value)` per component and `add_tag` per tag would make —
with per-copy **overrides**. A Tiled object names the prefab it stands for, and one call spawns a map's objects. Values
may come from [data assets](data.md). One block of memory per library; nothing allocates while playing; the same
registrations and spawns give the same handles and bytes on PC and 3DS.

Optional (constitution XI): a game that doesn't include the header links none of it, and a game that spawns only from
code links no tilemap code (the map calls are in their own object file). The header includes only
`<nnh/entities.hpp>` and `<nnh/math.hpp>`.

```cpp
struct Body { nnh::Rect box{}; nnh::collision::Layers layers = nnh::collision::kNoLayers; };
struct Coin { int value = 1; };
constexpr nnh::Tag kCoin = nnh::tag("coin");

auto made = nnh::PrefabLibrary::create({.max_prefabs = 16, .max_entries = 32, .value_bytes = 512});
nnh::PrefabLibrary prefabs = std::move(made.value()); // check it in real code
const nnh::Prefab coin = prefabs.add("coin",
    {.tags = {kCoin},
     .place = nnh::place(&Body::box),        // a spawn's position goes into Body::box
     .on_spawn = [](nnh::World& w, nnh::Entity e, const nnh::SpawnInfo& info) {
         if (info.object) w.get<Coin>(e)->value = info.object->properties.get_int("value", 1);
     }},
    Body{{0, 0, 12, 12}, kCoinLayer}, Coin{1}).value();

prefabs.spawn(world, coin, {64, 32});        // a coin at (64, 32)
prefabs.spawn_with(world, coin, {80, 32}, Coin{5}); // a gold coin: Coin overridden for this copy only
prefabs.spawn_map_objects(world, map);       // every Tiled object of type `coin` (or `prefab` = "coin")
```

## Libraries and capacities

`PrefabLibrary::create(desc)` allocates one block: `PrefabLibraryDesc{.max_prefabs (1..1024), .max_entries (component
entries over all prefabs, 0..65 535), .value_bytes (stored values with their alignment, 0..1 MiB), .when_full (see
[All or nothing](#all-or-nothing)), .memory_tag (default `prefabs`)}`. Errors: `InvalidArgument` (a capacity out of
range), `OutOfMemory`. A library is movable, not copyable; its destructor destroys the stored values. A default or
moved-from library is empty: commands assert, queries answer nothing. Several libraries may spawn into one world, and
one library into several worlds. Nothing is unregistered or edited: a library only grows until it is destroyed (one per
level is a good shape).

**Memory** (3DS; 64-bit hosts in brackets): ≈ 64 B of header, per prefab 80 B [88 B] (a 72 B [80 B] record — entry
range, placement, hook link, 8 inline tags, the hook — a 4 B hash and 4 B of name table) plus 32 B for its name in
Debug/Development, per entry 16 B [32 B], plus the values (`sizeof(T)` with alignment; a variant stores only its own).
128 prefabs × 4 components of 16 bytes: ≈ 26 KB.

## Registering

`add(name, options, values...)` copies each value into the block in call order and returns the prefab's handle. Each
value type must be a class, copy-constructible and aligned to at most 16 bytes (compile-time checks); spec 020's
components in general need not be copyable — a non-copyable component can be added by the hook. `PrefabOptions`:

| Field | What |
|-------|------|
| `base` | a prefab of this library: this one is a [variant](#variants) of it |
| `tags` | `{kEnemy, kFlying}` — up to 8; the first `kNoTag` ends the list; duplicates are dropped |
| `place` | `nnh::place(&Body::box)` or `nnh::place(&Mover::at)`: a `Vec2` or `Rect` member of one of the prefab's kinds; `place(&Body::box, true)` keeps the Rect's size when a rectangle map object spawns it |
| `on_spawn` | the [spawn hook](#hooks-and-spawninfo) |

Every check runs before anything is written; a refused registration changes nothing. Errors: `InvalidArgument` (null or
empty name, a name whose hash is already registered, a kind twice among the values, a base not of this library, a
placement kind not among the entries or a member past its end, `kNoTag` before another tag), `LimitReached` (more than
32 entries, 8 tags or 8 hooks per prefab; a library capacity). The array form `add(name, options, const
ComponentValue*, count)` takes values made by `nnh::component_value(v)`. Registration is load-time work; it never
allocates.

## Names and hashes

A prefab's name is its FNV-1a 32 hash, the hash of tags, data paths and sprite names: `nnh::prefab_name("coin")` at
compile time and `find("coin")` at run time give the same bits. Every name of a library is registered in one place, so
**a hash collision is always caught**: the second name is refused (`"costarring"` and `"liquid"` collide, for
instance). `find(name)` / `find(text, size)` / `find(prefab_name(...))` return the handle or a null one: a hash and a
probe of a small table (the hashes are in their own array, so a find never touches the records). Spawns take a handle:
look names up at load, never per frame. Release builds store no names (`name()` returns `""`); Debug and Development
keep 31 bytes per name for messages.

A `Prefab` is a `Handle<PrefabLibrary>`: the prefab's index and the library's 16-bit stamp, so a handle from another
library is refused (asserts) in all but 1 case in 65 535.

## Variants

`add("flying_walker", {.base = walker, .tags = {kFlying}, .on_spawn = start_bob}, Bob{}, Walker{0.75f})` registers a
variant, **flattened at registration**: it starts with the base's entries in the base's order, each replaced **in
place** by the variant's value of the same kind (here `Walker`), then the variant's other values **appended** in call
order (`Bob`); tags are the base's then the variant's; the base's placement unless the variant gives one; the base's
hooks run first, then the variant's. Entries the variant doesn't replace **share** the base's stored value (no copy:
variants cost their own values only). Variants of variants work (up to 32 entries, 8 tags and 8 hooks in all). Because
a library has no editing, there is nothing to propagate later: register a change to the base before its variants.

## Spawning

`spawn(world, prefab)` makes one entity exactly as the hand-written code would: `spawn()` (the lowest free entity
slot), then for each entry in order a component in its kind's lowest free slot copy-constructed from the stored value,
then the placement, the tags in order, then the hooks. Handles, slots, component values and tags are identical to
`spawn()` + `add<T>(e, value)` per entry + `add_tag` per tag (the example's self-check proves it over 50 000 operations,
on PC and 3DS). Spawning inside an iteration, a remove hook or another spawn hook follows spec 020's rules (a copy made
during `each` is not visited). The world is reached through **one** reservation per spawn: no per-component kind checks,
tags written in one go. `spawn` returns a stale handle if a hook destroyed the copy.

A copy keeps **no link** to its prefab (no engine component, no automatic tag): add your own tag or component if you
want to know "which prefab". No event is published; publish your own (`EnemySpawned{e}`) from the hook.

## Overrides and placement

`spawn(world, prefab, at)` writes `at` into the placement member: a `Vec2` member gets it, a `Rect` member gets its `x`,
`y`. `spawn_with(world, prefab, [at,] overrides...)` gives component values for this copy only: a value of a kind the
prefab has **replaces** the stored value (the component is constructed once, from the override); a value of another
kind is **added** to this copy, after the prefab's entries. Each override type at most once and never a bare
`nnh::Vec2` (it would read as a position): compile errors. An override of the placement kind is constructed first and
the position is written over it. A position given to a prefab without a placement member asserts and is ignored.

## Hooks and SpawnInfo

The hook, `[captures](nnh::World& w, nnh::Entity e, const nnh::SpawnInfo& info)`, runs once per copy after its
components, placement and tags are in place and before `spawn` returns (captures ≤ 16 bytes, trivially copyable, as
spec 020's remove hooks: `[this]`, `[tweens]`). `SpawnInfo`: `has_position`, `position`, and for copies made from a map
`object` (the `MapObject`, valid during the hook only — include `<nnh/tilemap.hpp>` to read it) and `object_index`.
The place to start a tween, read a Tiled property into a component, spawn a turret's base, or publish your own event.
A hook may spawn (this library or another), destroy (the new entity too), add and remove. Destroying the **library**
from its own hook is misuse: it asserts; in Release the block is freed when the outermost spawn returns.

## All or nothing

Before creating anything, a spawn checks that the world has a free entity slot and a free slot in every kind the copy
needs (overrides included), and that the world stores each kind. If not, **nothing** is created — never a body without
its coin value — and the spawn returns null, counts a miss in the library's stats and, like spec 020's own refusals, in
the world's (its spawn misses, or the full kind's misses). What else happens is the library's `WhenFull`:

| `WhenFull` | A full capacity |
|------------|-----------------|
| `Assert` (default) | logs `spawn of prefab "<name>": ...` (`#<index>` in Release) and asserts in Debug/Development, as a full world: the capacity is the game's budget |
| `Miss` | quiet: budgeted spawns (touch spawns, effects) |

A kind the world doesn't store always asserts. Inside the world's `clear()` (spawns refused by spec 020) a spawn
returns null without a miss or an assert.

## Map objects

`spawn_map_objects(world, map[, {.object_layer = n}])` walks the map's objects in map order (one object layer, or all):

- an object with a string property **`prefab`** names that prefab;
- otherwise its **type** (Tiled's class) does;
- a type naming no prefab — doors, spawn points, empty types — is **skipped**: objects the game reads itself are normal;
- a `prefab` property naming no prefab (or not a string) is a **level error**: logged in every flavour as
  `object 12 "coin" (layer things): no prefab "coinn"` and asserting in Debug/Development.

Each copy is placed at the object's bounds through the placement member (a rectangle object also sets a `Rect`
member's `w`, `h` unless the placement keeps its size; a `Vec2` member gets the top-left), with the object in the hook's
`SpawnInfo`. A prefab without a placement member is spawned from a map without a position (no assert). The call returns
a `MapSpawnSummary{spawned, misses, skipped, unknown, first_unknown}`; `check_map_objects(map)` answers the same report
without spawning, logging or asserting (a test, a loading screen). The same objects in the same order after a world
`clear()` get the first load's slots: a level restart is `clear()` + `spawn_map_objects`. See
[Placing prefabs in Tiled](../guides/levels.md#placing-prefabs-in-tiled).

## Values from data assets

Prefab values are plain structs, so a value loaded by `nnh::data::load` registers like any other. One prefab per data
asset of a type, named by the asset's name, registered in a loop at load time:

```cpp
for (int i = 0; i < nnh::data::count<EnemyData>(); ++i) {
    const char* name = nnh::data::name<EnemyData>(i);     // "enemies/imp"
    EnemyData enemy;
    if (!nnh::data::load(name, enemy)) continue;
    const float side = enemy.size;
    prefabs.add(name, {.tags = {kEnemy}, .place = nnh::place(&Body::box)},
                Body{{0, 0, side, side}, enemy.color}, Walker{enemy.speed}, Stats{enemy.hp});
}
```

A designer adds an enemy by adding a text file; spawning never reads files. The prefab module itself doesn't include or
link the data module ([`examples/prefabs/roster.cpp`](../../examples/prefabs/roster.cpp)).

## Queries and stats

`count()`, `capacity()`, `valid(p)`, `name(p)`, `kinds(p, out, capacity)` (kind ids in entry order: compare with
`nnh::kind_id<Body>()`), `tags(p, out, capacity)`, `has<T>(p)`, `value<T>(p)` (the stored value, read-only; a variant may
read its base's) and `stats()`: prefabs, entries and value bytes used and their capacities, spawns, misses, map objects
skipped and unknown since creation — a debug overlay line.

## Determinism

Spawning uses spec 020's deterministic rules plus fixed orders (entry order, flattening order, map object order);
values are copied, never recomputed. The same registrations and spawns give the same handles, slots, component bytes
and tags on every host compiler and on the 3DS (the self-check's golden `7BB7901E`).

## Costs

Per spawn: one handle check, one call resolving the entries' kinds, one reservation in the world, one copy call per
component (as `add<T>(e, value)`), the placement's float stores, the hook chain when there is one. No allocation, no
virtual call. `find`: one pass of FNV-1a over the name (one multiply per byte) and a probe in a table of 2-byte
indices. `spawn_map_objects` reads the map's entries in place: per object a memoised check of its property names and
of its type or `prefab` name (one `find` per distinct name per call), then the spawn; the `MapObject` is built only for
objects that spawn.

Old 3DS numbers (`examples/prefabs`' bench, Development build, 2026-11-23; spec 021 AC-7 sets the budgets from them):

| Workload | Old 3DS | Budget |
|---|---|---|
| 256 spawns of a 3-component prefab with 2 tags, plus their destroys and a flush | 1.63 ms (1.07 × the same calls written by hand) | < 2 ms, ≤ 1.25 × |
| the same with a component override and a position each | 1.81 ms | < 2.3 ms |
| 512 map objects (two prefabs, a hook reading one property each) | 3.56 ms | < 4.5 ms |
| 1000 `find` of 9-character names over 128 prefabs | 0.50 ms | < 0.65 ms |

Look names up at load and spawn by handle; a level of a few hundred objects costs a few milliseconds once.

## Misuse

Asserts in Debug/Development, harmless in Release (an error logged, nothing done or nothing answered): a null or
foreign prefab, an empty or moved-from library or world, a kind overridden twice (the second ignored), a position
without a placement member (ignored), a kind the world doesn't store, more prefab tags than the world's
`tags_per_entity` (Release keeps the first ones), destroying the library from its own hook. Creation and registration
errors are `Result`s.

## Pitfalls

- **Padding.** A copy of a stored value copies its bytes, padding included; `add<T>(e)` without a value leaves the
  slot's `T{}`. The members are the same; padding bytes may not be. Hash members, not raw bytes, of padded types.
- **Prefabs are load-time objects.** Register and `find` at load; spawn by handle.
- **A hook that destroys the world** it was given leaves the remaining hooks an empty world (they assert).
- **`SpawnInfo::object`** is valid during the hook only (a view into the map, like spec 012's `MapObject`).
- **A variant registered before a change to its base** keeps the old values: register bases first.
