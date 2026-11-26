# Plan 021: Prefabs

Status: implemented 2026-11-23; Linux T9.1 and Old 3DS T10.2 passed (2026-11-23) · approved 2026-11-23 by the maintainer, with the plan's
six decisions as recommended
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decision D36 ([decisions.md](../../docs/architecture/decisions.md))
Uses spec 020 (worlds, kinds, `ComponentDesc`, tags, handles, quarantine, `SmallFunction` hooks; D35), spec 002
(asserts, logs, `Result`, tagged memory, profiler), spec 012 (`Tilemap`, `MapObject`, `Properties`; the converter and
its fixtures), spec 014 (`data::load`, `data::name<T>`; the test game fixture), spec 015 (`TweenSet` in hooks), spec
013 (scripts), spec 016/014 (`detail::fnv1a32`)

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | Portable core only; a spawn uses spec 020's deterministic rules plus fixed orders (entry order, flattening order, map object order); values are copied, never recomputed; the AC-2 golden on every host compiler and on the console |
| II 3DS constraint | ⚠️→✅ | Pressure point: AC-7's four targets predate any measurement, and spec 020's console run showed what costs on the ARM11 (no L2, random access over 16 KB is memory-bound; per-call kind lookups; calls and stack structs). Answered by the ARM11 cost plan below: one out-of-line call per spawn into an all-or-nothing reservation inside the world, every AC-7 workload touching < 16 KB at random, a separate 4-byte hash array and an open-addressing table for `find`, a one-pass run-time hash, and a disassembly check before the console (T7.6). One target (`find`) is at risk; AC-7's measurement-with-margin rule applies. Memory: one block per library; the spec's "well under 16 KB" example is ≈ 26 KB (see Data and memory: its entries and values alone are 16 KB) |
| III Build speed | ✅ | One header over `<nnh/entities.hpp>` and `<nnh/math.hpp>` (no includes of its own), templates only for registration, typed overrides and typed queries (thin wrappers that build a `ComponentValue` array and call out-of-line code); measured before/after (T0.1, T11.4) |
| IV Event-driven | ✅ | No events published (FR-12); hooks and the game's own events cover spawns |
| V No hidden costs | ✅ | No exceptions (`Result` at creation and registration), no RTTI (`detail::type_id<T>()`), no `virtual` (per-entry copy/destroy function pointers, called once per component copied), one allocation per library at creation |
| VI Spec + test first | ✅ | Pure logic first on the fake platform; the world's reservation tested against spawn/add/add_tag on twin worlds; AC-2's golden equivalence; the platformer swaps with unchanged golden lines |
| VII Small files | ✅ | One module, one public header (+ one detail header), eight source files of one concept each (plus one in `src/entities/`), all planned ≤ 200 lines |
| VIII Documented API | ✅ | `docs/api/prefabs.md`, links from `entities.md` and `data.md`, the levels guide, `examples/prefabs`, the platformer, D36 |
| IX Clean and legal | ✅ | Own code from published designs (Unity, Godot, flecs, Bevy manuals; *Game Programming Patterns*' Prototype); cited in `docs/REFERENCES.md` |
| X Safe by default | ⚠️→✅ | Pressure point (spec Constraints, XI's "checked at build time"): a map object's prefab name can only be resolved at run time. Answered as the spec says: an unknown explicit `prefab` property is a loud level error at load (logged in every flavour, asserted in Debug/Development), `check_map_objects` answers it without spawning, and the example's console script covers it. No parser is added |
| XI Opt-in, never in the way | ⚠️→✅ | Pressure point: spec 020's non-goal said prefabs need no change in it. Answered with **entities-internal additions only** (an all-or-nothing reservation and a kind lookup by type id in `src/entities/`), spec 020's public header untouched, its tests and goldens unchanged (`C8FB9A41`, the platformer's `B2418A28`). The tilemap types are forward-declared and only one prefab source file includes `<nnh/tilemap.hpp>`, so a game that spawns only from code links no tilemap code |

## Design

### Overview

```
game structs (its own)            <nnh/prefabs.hpp>                                   <nnh/entities.hpp>
  Body, Coin, Turret, Spark ...    PrefabLibrary (one block):                            World (spec 020)
                                     records  [prefab]: entry range, tags[8], placement,     ↑
                                              own hook, hook-chain link                       │ entities-internal (src/entities/
                                     entries  [entry]: type id, value offset, copy, destroy   │   entities_internal.hpp):
                                     values   bytes: copies of the game's structs             │   world_kind_of(world, type id)
                                     hashes[] + open-addressing table: name → index           │   world_reserve(...): all-or-nothing
                                   spawn(world, prefab[, at][, overrides...]) ───────────────┘   entity + kind slots + tags
                                   spawn_map_objects(world, map)  (prefab_map.cpp: the only file including <nnh/tilemap.hpp>)
```

### Decision D36: a library of flattened prototypes, spawned through an all-or-nothing reservation in the world

**Library.** A `PrefabLibrary` is one block of tagged memory (tag `prefabs` or the game's), sized at `create()` by three
capacities (prefabs 1..1024, component entries 0..65 535, value bytes 0..1 MiB) and a policy for full capacities
(`WhenFull`). Nothing is allocated afterwards; nothing is unregistered or edited (FR-4: there is nothing to propagate).

**Prefab record** (per prefab): the range of its entries, up to 8 tags, its placement (the entry holding the member, the
member's byte offset, `Vec2` or `Rect`, keep-size), its own spawn hook (a `SmallFunction`, empty when none) and a
**hook-chain link** (the nearest base prefab that has a hook, and the chain length ≤ 8). **Entry** (per component
entry): the kind's type id, the offset of its stored value in the value bytes (bit 31: this entry owns the value),
and the copy and destroy function pointers of its type. **Values**: each a copy-constructed object of the game's
struct, aligned to its type (≤ 16 bytes alignment, checked at compile time).

**Names.** A name is its FNV-1a 32 hash (`detail::fnv1a32`, the hash of tags, data paths and sprite names). The hashes
live in their own array (4 bytes per prefab: a `find` touches 1 KB for 128 prefabs, not the records) and an
open-addressing table of `u16` indices (a power of two ≥ 2 × `max_prefabs`, linear probing, never more than half full)
maps a hash to its prefab. Registering a hash already present is `InvalidArgument`, so a collision between two names of
one library can never pass. A `Prefab` is a `Handle<PrefabLibrary>`: bits 0–15 the index, bits 16–31 the library's
**stamp** (a 16-bit number given to each library at `create()`, 1..65 535 in turn), so a handle from another library
is caught (Debug/Development) in all but 1 in 65 535 cases. Debug and Development builds keep each name (up to 31
bytes, truncated) for messages and `name()`; Release stores none (`name()` returns `""`).

**Variants (FR-4) are flattened at registration.** The new prefab's entries are the base's, in the base's order, each
replaced in place by the variant's own value of the same kind, then the variant's other values appended in call
order; tags are the base's then the variant's (duplicates dropped; more than 8 → `LimitReached`); placement and hook
chain are inherited, a placement given by the variant replaces the base's; the variant's own hook runs after the base's
chain (chains of up to 8 hooks; a 9th → `LimitReached`). An entry the variant doesn't replace **shares the base's
stored value** (no edits exist, so sharing is safe; the owner bit says who destroys it): a variant costs its entries
(16 bytes each) and its own values only. Spawning a variant reads one entry range, as any prefab.

**Spawning: the world reserves everything first.** `spawn` resolves each entry's kind in the world
(`world_kind_of`, the straight compares of spec 020's `kind_index` for the first four kinds, then a scan), adds the
override-only kinds, and calls **one** entities-internal function, `world_reserve`, which either takes **all** of an
entity slot and one slot per kind (lowest free, with exactly the bookkeeping of `spawn()` and `add<T>()`: live bits,
fresh bits and `pending` while iterating, owners, record slots, kinds mask) and writes the prefab's tags into the new
record, or takes **nothing** and says why (clearing, world full, kind *k* full) after counting the refusal where
`spawn()`/`add()` would have (the world's spawn misses or kind *k*'s misses). The prefab module then, in entry order,
destroys the slot's `T{}` (when `T` has a destructor) and copy-constructs the stored value — or the copy's override —
into it, then the override-only kinds in call order, then writes the placement member, then runs the hook chain base
first. The bytes are those of `spawn()`, `add<T>(e, value)` per entry, the member write and `add_tag` per tag, in
FR-5's order (no hook or other code runs between the reservation and the copies, so the order of slot taking across
distinct kinds can't change a slot). Shared inline helpers (`take_entity`, `attach_component` in
`world_internal.hpp`) are used by `spawn()`, `world_add()` and `world_reserve()`, so the bookkeeping is one piece of
code (spec 020's goldens prove it unchanged).

**Rejected:** a global registry filled by static registration (hidden global; data assets are not loaded yet; spec
decision 2); a live `IsA` link resolved per spawn (a lookup per spawn; spec decision 4); public `World::can_add()` /
`World::add_bytes()` (they would widen spec 020's API for one user and make "check, then spawn and add" three passes
with `find_free` three times per kind); checking capacities through public queries and then calling `spawn`/`add<T>`
(the same, plus no way to add by type id); a linear scan of the records for `find` (9 KB per find for 128 prefabs:
memory-bound on the Old 3DS); storing names in Release; a sorted hash array with binary search (unpredictable branches,
≈ 7 dependent loads); duplicating a base's values into each variant (memory for nothing).

### Public API (sketch)

```cpp
// <nnh/prefabs.hpp>: <cstddef>, <cstdint>, <type_traits>, nnh/entities.hpp, nnh/math.hpp, nnh/result.hpp,
//                    nnh/detail/prefab_values.hpp — not tilemap, data, collision, tween, animation or gfx
namespace nnh {
class Tilemap;  struct MapObject;  struct MemTag;  class PrefabLibrary;   // forward declarations only

/// A registered prefab: index (16 bits) and the library's stamp (16 bits); 0 = null. Trivially copyable.
using Prefab = Handle<PrefabLibrary>;

inline constexpr int kMaxPrefabs = 1024;
inline constexpr int kMaxPrefabEntries = 32; ///< component entries per prefab (as spec 020's 32 kinds)
inline constexpr int kMaxPrefabTags = 8;     ///< as spec 020's 8 tags per entity
inline constexpr int kMaxPrefabHooks = 8;    ///< a prefab's hook plus its bases' hooks

/// A prefab's name as its FNV-1a 32 hash: the same bits from a literal at compile time and from a string at run time.
enum class PrefabName : std::uint32_t {};
constexpr PrefabName prefab_name(const char* name) { return PrefabName{detail::fnv1a32(name)}; }
constexpr PrefabName prefab_name(const char* text, std::size_t size) { return PrefabName{detail::fnv1a32(text, size)}; }

/// A component kind in a prefab's queries: nnh::kind_id<Body>().
using KindId = detail::TypeId;
template <typename T> constexpr KindId kind_id() { return detail::type_id<T>(); }

/// What a spawn hook knows about the copy it runs for.
struct SpawnInfo {
    bool has_position = false;
    Vec2 position{};                   ///< the position given to the spawn (a map object's x, y)
    const MapObject* object = nullptr; ///< copies made from a map: the object, valid during the hook (include <nnh/tilemap.hpp> to read it)
    int object_index = -1;             ///< its index in the map
};
/// A spawn hook: `[this](nnh::World& w, nnh::Entity e, const nnh::SpawnInfo& info) { ... }` (captures ≤ 16 bytes,
/// trivially copyable, as spec 020's remove hooks). Called once per copy, after its components, placement and tags.
using SpawnHook = detail::SmallFunction<void(World&, Entity, const SpawnInfo&)>;

/// A prefab's placement member (FR-7): where a spawn's position goes. Made by place().
struct Placement {
    KindId kind = nullptr;
    std::uint16_t offset = 0; ///< the member's byte offset in the component
    bool rect = false;        ///< a Rect member (x, y; and w, h from a rectangle map object); else a Vec2
    bool keep_size = false;   ///< a rectangle map object doesn't change the member's w, h
};
/// `nnh::place(&Body::box)`: M is nnh::Vec2 or nnh::Rect (compile-time check). Builds one C{} to measure the offset.
template <typename C, typename M> Placement place(M C::*member, bool keep_size = false);

/// One component value, type-erased (the array form of add()): `nnh::component_value(Body{...})`. Points at the game's
/// value (copied by add(), not kept).
struct ComponentValue {
    KindId id = nullptr;
    const void* value = nullptr;
    int size = 0, align = 0;
    void (*copy)(void* to, const void* from) = nullptr; ///< placement-new copy construction
    void (*destroy)(void*) = nullptr;                   ///< null when trivially destructible
};
template <typename T> ComponentValue component_value(const T& value); ///< T copy-constructible, alignof ≤ 16

/// How a prefab is registered (besides its component values).
struct PrefabOptions {
    Prefab base{};                 ///< a variant of this prefab (registered earlier in the same library)
    Tag tags[kMaxPrefabTags] = {}; ///< `.tags = {kEnemy, kFlying}`; the first kNoTag ends the list
    Placement place{};             ///< `.place = nnh::place(&Body::box)`
    SpawnHook on_spawn{};          ///< runs after the base's hooks
};

/// What a spawn does when a capacity it needs is full (FR-8): Assert (default) — counts a miss and asserts in
/// Debug/Development, logs an error in Release, as a full world; Miss — counts a miss quietly (budgeted spawns: touch
/// spawns, effects). Either way nothing is created.
enum class WhenFull : std::uint8_t { Assert, Miss };

struct PrefabLibraryDesc {
    int max_prefabs = 0;    ///< 1..1024
    int max_entries = 0;    ///< component entries over all prefabs, 0..65535 (a variant's entries count)
    int value_bytes = 0;    ///< stored values with their alignment, 0..1 MiB (a variant stores only its own values)
    WhenFull when_full = WhenFull::Assert;
    MemTag* memory_tag = nullptr; ///< default: the engine's `prefabs`
};
struct PrefabStats {
    int prefabs = 0, capacity = 0, entries = 0, entry_capacity = 0, value_bytes = 0, value_capacity = 0;
    int spawns = 0, misses = 0, map_skipped = 0, map_unknown = 0; ///< since creation
};
struct MapSpawnOptions { int object_layer = -1; }; ///< one object layer, or −1 = every layer
/// spawned; refused by a full capacity (FR-8); type naming no prefab (skipped: doors, spawn points, empty types);
/// explicit `prefab` property naming no prefab (level errors; the first one's object index).
struct MapSpawnSummary { int spawned = 0, misses = 0, skipped = 0, unknown = 0, first_unknown = -1; };

class PrefabLibrary {
public:
    PrefabLibrary() = default;  ///< empty: commands assert, queries answer nothing
    /// Errors: InvalidArgument (a capacity out of range), OutOfMemory.
    static Result<PrefabLibrary> create(const PrefabLibraryDesc& desc);
    ~PrefabLibrary();           ///< destroys the stored values, frees the block
    PrefabLibrary(PrefabLibrary&&) noexcept;  PrefabLibrary& operator=(PrefabLibrary&&) noexcept;
    PrefabLibrary(const PrefabLibrary&) = delete;  PrefabLibrary& operator=(const PrefabLibrary&) = delete;

    // ---- Registration (load time; FR-2..FR-4, FR-10) ----
    /// `lib.add("coin", {.tags = {kCoin}, .place = nnh::place(&Body::box)}, Body{{0, 0, 12, 12}, kCoinLayer}, Coin{1})`.
    /// Copies each value into the block. Errors (nothing registered): InvalidArgument (null or empty name, a name
    /// whose hash is registered, a kind twice among the values, a foreign or null-but-set base, a placement kind
    /// not among the entries, kNoTag inside the list), LimitReached (32 entries, 8 tags or 8 hooks per prefab; a
    /// library capacity). Each T: copy-constructible, alignof ≤ 16 (compile-time checks).
    template <typename... T>
    Result<Prefab> add(const char* name, const PrefabOptions& options, const T&... values);
    Result<Prefab> add(const char* name, const PrefabOptions& options, const ComponentValue* values, int count);

    Prefab find(PrefabName name) const;           ///< null when none; no allocation
    Prefab find(const char* name) const;          ///< hashes in one pass (the same bits as prefab_name)
    Prefab find(const char* text, std::size_t size) const;

    // ---- Spawning (FR-5..FR-8) ----
    /// One entity, bit for bit what spawn() + add<T>(e, value) per entry + add_tag per tag would make, then the
    /// hooks. Null when refused (FR-8: see WhenFull; inside world.clear(): null, no assert, no miss). May return a
    /// stale handle (a hook destroyed the copy). No allocation; no virtual call.
    Entity spawn(World& world, Prefab prefab);
    Entity spawn(World& world, Prefab prefab, Vec2 at);  ///< `at` into the placement member (asserts without one)
    /// With component overrides (FR-7): each replaces the prefab's value of its kind for this copy or adds a kind;
    /// a kind twice is a compile error here (and asserts in the array form). No override may be a bare nnh::Vec2.
    template <typename T, typename... More>
    Entity spawn_with(World& world, Prefab prefab, const T& override, const More&... more);
    template <typename T, typename... More>
    Entity spawn_with(World& world, Prefab prefab, Vec2 at, const T& override, const More&... more);

    // ---- Map objects (FR-9) ----
    /// Every object (or one layer's) in map order: the `prefab` string property names the prefab, else the type;
    /// spawned at its bounds through the placement member, with the object in SpawnInfo. An explicit name that
    /// isn't registered is a level error: logged (`object 12 "coin" (layer things): no prefab "coinn"`) in every
    /// flavour, asserted in Debug/Development. Load time.
    MapSpawnSummary spawn_map_objects(World& world, const Tilemap& map, const MapSpawnOptions& options = {});
    /// The same summary without spawning, logging or asserting (spawned = what would be spawned, misses = 0).
    MapSpawnSummary check_map_objects(const Tilemap& map, const MapSpawnOptions& options = {}) const;

    // ---- Queries (FR-11) ----
    int count() const;  int capacity() const;
    bool valid(Prefab prefab) const;              ///< registered here
    const char* name(Prefab prefab) const;        ///< Debug/Development: the name (≤ 31 bytes); Release: ""
    int kinds(Prefab prefab, KindId* out, int capacity) const; ///< entry order; returns the total
    int tags(Prefab prefab, Tag* out, int capacity) const;
    template <typename T> bool has(Prefab prefab) const;
    template <typename T> const T* value(Prefab prefab) const; ///< the stored value, read-only; null when absent
    PrefabStats stats() const;

private:
    detail::PrefabBlock* block_ = nullptr;
};
} // namespace nnh
```

Exact names may still change while building; stage notes in this file record any change. The templates are thin:
`add<T...>` builds `const ComponentValue list[] = {component_value(values)...}` and calls the array form;
`spawn_with` builds the same array from its overrides (`static_assert` on duplicate types and on `Vec2`) and calls
`detail::prefab_spawn(block, world, prefab, at_or_null, list, n)`; `has<T>`/`value<T>` call one out-of-line lookup by
`kind_id<T>()`.

### Events introduced

None (FR-12, constitution IV). The docs show a hook publishing the game's own `EnemySpawned{Entity who}`.

### Semantics fixed by the plan

**Registration** (`library_add.cpp`, `prefab_flatten.cpp`). Every check runs before anything is written: the name
(non-null, non-empty; its hash not in the table), the base (null, or valid here), the own values (no kind twice;
`count` ≤ 32), the flattened result (entries ≤ 32, tags ≤ 8 with no `kNoTag` before the end of the list, hooks ≤ 8,
the placement kind among the entries), and the capacities (prefabs, entries, value bytes with each value's alignment
padding). On success: the record, the entries (inherited ones copied with the owner bit clear, own ones with it set),
own values copy-constructed into the block in call order, the hash in the array and the table, the name (Debug/
Development). `LimitReached`/`InvalidArgument` leave the block byte-for-byte unchanged (tested by hashing the block).

**Spawn** (`prefab_spawn.cpp`, `detail::prefab_spawn`):

1. Empty library, or a prefab handle that is null, out of range or of another stamp → misuse (assert; Release: log an
   error, return null). Empty world → spec 020's misuse.
2. Kinds: for each entry, `world_kind_of(world, id)` into a local `int kinds[64]`; then each override's kind is matched
   against the entries (a scan of ≤ 32 type ids) → its entry takes the override's value, or it is appended as an
   override-only kind. A kind not stored by the world → misuse (always asserts) and a miss; a kind overridden twice
   (array form) → assert, the second ignored. A position without a placement member → assert, ignored. More tags than
   the world's `tags_per_entity` → assert; Release keeps the first ones (as `add_tag` would).
3. `world_reserve(world, kinds, n, slots, tags, tag_count, &why)` → the entity, or nothing. Refused while clearing →
   null (no miss, no assert). Refused because full → library miss (+ the world's own counter, inside reserve), then
   `WhenFull::Assert`: error log + assert naming the prefab (Debug/Development) and the full capacity; `Miss`: quiet.
4. For each entry, then each override-only kind: `dst = data + slot × size`; `destroy(dst)` if the type has one;
   `copy(dst, value)`.
5. Placement (when given a position and the prefab has one): `Vec2` member ← position; `Rect` member ← x, y, and w, h
   when the source is a rectangle map object and not `keep_size`. Written as plain float stores into the component
   (the override, if any, was constructed first: the position wins).
6. Hook chain: the chain's prefabs collected by following the links into a local `u16[8]`, run base first with
   `(world, entity, info)`. Hooks may spawn (this library or any), destroy (the new entity too), add and remove: spec
   020's rules. `spawns + 1` before the hooks (the copy exists).
7. Return the entity's handle (stale if a hook destroyed it).

**Map objects** (`prefab_map.cpp`): for `i` in `0..object_count()` (filtered by layer): `o = map.object(i)`;
`explicit = o.properties.get_string("prefab", nullptr)`. With an explicit name → `find`; unknown → level error
(`unknown + 1`, `first_unknown`, the log line and assert; a `prefab` property that is not a string is the same error,
worded `property "prefab" is not a string`). Else the type: empty or unknown → `skipped + 1`. Found → spawn with
`SpawnInfo{true, {o.bounds.x, o.bounds.y}, &o, i}` and the source shape (rectangle or point) for the `Rect`
placement → `spawned + 1` or `misses + 1`. The type → prefab resolution is memoised per call by the type string's
pointer (a map stores equal strings once; a different pointer with equal text is only a cache miss, never a wrong
answer). Library stats gain `map_skipped`/`map_unknown`. Inside the world's clear: every spawn is refused silently
(counted as nothing). `check_map_objects` runs the same resolution with no world.

**Copies and padding.** A prefab copy is `new (dst) T(stored)`; for trivially copyable types that copies the stored
object's bytes, padding included, whereas `add<T>(e)` without a value leaves the slot's `T{}`. Equivalence is
therefore over values (every member), and AC-2's golden uses padding-free kinds
(`static_assert(std::has_unique_object_representations_v<T>)` in the fixtures), so "every component byte" is exact.
The platformer's replay hashes members, so its turret (`bool` + `int`) is unaffected.

**Misuse (FR-15)** — assert in Debug/Development; Release logs an error and does nothing or answers nothing: null or
foreign prefab handle; a kind overridden twice; a position without a placement member; a kind the world doesn't
store; an empty or moved-from library (commands; queries answer nothing without asserting, as spec 020's empty
worlds); more prefab tags than the world holds; destroying a library from inside one of its own spawn hooks (Release:
the block is freed when the outermost spawn returns, as spec 020's doomed world). Creation errors are `Result`s;
registration errors are `Result`s and change nothing.

### Entities-internal additions (no public change)

`engine/core/src/entities/entities_internal.hpp` (the internal API another module needs, per project-structure):

```cpp
namespace nnh::detail {
/// The registration index of the kind with type id `id` in `world`, or −1 (empty world: −1).
int world_kind_of(const World& world, TypeId id);
enum class ReserveRefusal : std::uint8_t { None, Clearing, WorldFull, KindFull, Empty };
/// All-or-nothing (spec 021 FR-8): the lowest free entity slot and, for each of `count` distinct registered kinds, the
/// lowest free slot, taken with spawn()/add()'s bookkeeping; `tags` (distinct, not kNoTag, ≤ the world's tags per
/// entity) written into the record. Returns the entity, or null with `why` (and `*full_kind` for KindFull) after
/// counting the refusal as spawn()/add() would (world spawn misses / that kind's misses; nothing while clearing).
/// The components' slots hold T{}; `slots[i]` receives kind kinds[i]'s slot. No log, no assert.
Entity world_reserve(World& world, const int* kinds, int count, std::uint16_t* slots, const Tag* tags, int tag_count,
                     ReserveRefusal& why, int* full_kind);
/// The component storage of kind `kind`, slot `slot` (a reserved slot).
void* world_component(World& world, int kind, int slot);
}
```

`world_reserve.cpp` (≈ 70 lines): empty → `Empty`; `clearing` → `Clearing`; `entities.find_free()` < 0 → spawn misses
+ 1, `WorldFull`; each kind's `find_free()` < 0 → that kind's misses + 1, `KindFull` (no state changed yet: `find_free`
only moves the hint word forward past full words, which keeps its invariant and is what `spawn`/`add` do); then
`take_entity` and `attach_component` per kind (the helpers `spawn()` and `world_add()` now share), tags written into
the record's empty words (all are empty in a new record). `spawn()` and `world_add()` are rewritten over the same two
inline helpers with no behaviour change (spec 020's tests, `C8FB9A41`, `B2418A28`, the T7.6 instruction counts).

### Header and build cost (FR-16, III)

- `<nnh/prefabs.hpp>` includes `<nnh/entities.hpp>`, `<nnh/math.hpp>` (no includes of its own) and
  `detail/prefab_values.hpp` (`<new>`, `<type_traits>`: the copy/destroy function templates and `component_value`).
  Forward-declares `Tilemap`, `MapObject`, `MemTag`. Nothing includes it but games, tests and examples.
- Templates: `add<T...>`, `spawn_with<T...>`, `place<C, M>`, `component_value<T>`, `has<T>`, `value<T>`: each builds an
  array or a key and calls one out-of-line function; per component type one `copy_component<T>` (and a destroy when
  non-trivial) per program.
- Budgets (measured in T0.1 and T11.4, 3 runs): `-fsyntax-only` of a one-line TU including `<nnh/prefabs.hpp>` within
  0.05 s of one including `<nnh/entities.hpp>`; editing `examples/prefabs/room.cpp` → simulator < 2 s (or within
  0.1 s of the `examples/entities` edit measured in spec 020, 2.11–2.20 s, which the link dominates), `.3dsx` < 5 s.
- **Code-only games link no tilemap code.** Only `prefab_map.cpp` includes `<nnh/tilemap.hpp>` and refers to tilemap
  functions; it defines only `spawn_map_objects`/`check_map_objects`, which no other prefab object references. A
  static library pulls an object only for a referenced symbol, so a game that never calls them never pulls
  `prefab_map.o` (checked: `nm -u` of the module's objects, and a probe game's 3ds-release symbol list, T8.4).

### ARM11 cost plan (AC-7)

Lessons carried from spec 020's console run (plan 020, "Console bench refinement"): random access over more than 16 KB
is memory-bound on the Old 3DS (16 KB L1D, no L2: `storage get` ran at ≈ 113 cycles per lookup for 19 instructions);
sequential walks over large tables miss on every line without a prefetch (`find` needed `pld`); per-call kind lookups
and header reloads cost (`get<T>` 33 vs `Storage::get` 19 instructions); out-of-line calls, stack structs and
`memcpy`/`memset` calls per element show in the numbers; ≈ 1.8–2.2 cycles per instruction otherwise. Built in:

1. **One out-of-line call per spawn into the world** (`world_reserve`), instead of `spawn` + one `world_add` per kind
   + one `add_tag` per tag: no per-kind "stale? dying? already has it?" re-checks (the entity is new), tags written by
   word stores. Kinds are resolved once per spawn per entry (straight compares for the first four kinds).
2. **Every AC-7 workload stays under 16 KB of random access.** A spawn touches one record (72 B), its entries (16 B
   each), its values, and the world's lowest free slots (ascending: sequential). `find` touches the 4-byte hash array
   and the `u16` table (1.5 KB for 128 prefabs), never the records. The map workload walks the map's objects in order
   (sequential; the tilemap's `object()` is out of line, so no prefetch from here: noted as a risk).
3. **A one-pass run-time hash** in `find(const char*)` (`detail::fnv1a32(const char*)` counts then hashes: two passes),
   checked against the constexpr hash on test vectors.
4. **No struct returns on the hot path**: `prefab_spawn` returns an `Entity` (`r0`); `world_reserve` returns an
   `Entity` and writes slots through a pointer; `SpawnInfo` is built only when a hook exists.
5. **No VFP in the module** except the placement's float stores (no arithmetic, no `vmrs`); no division (alignment by
   masks, table index by `& mask`).
6. **Copies through the type's own function**, once per component (as `add<T>(e, value)` does inline); a 64-byte
   trivially copyable value may become an `ldm`/`stm` run or a `memcpy` call inside the copy function — the
   hand-written `add<T>(e, value)` makes the same copy, so the ratio is unaffected.

Estimates (instructions × ≈ 2 cycles; 1 ms = 268 000 cycles; spec 020's measured spawn workload: 1.295 ms, ≈ 700
instructions per entity, about half of it `destroy`):

| AC-7 workload | Hot path | Estimate | Target | Verdict |
|---------------|----------|----------|--------|---------|
| 256 × prefab spawn (3 kinds, one 64 B; 2 tags) + 256 `destroy` + flush | spawn ≈ 260 (handle check 8, 3 kind lookups 18, reserve ≈ 140, 3 copies ≈ 75, hook test, stats) vs hand-written ≈ 470 (spawn 65, 3 × `world_add` ≈ 276, 2 × `add_tag` ≈ 70, 3 copies ≈ 60); destroy ≈ 300 in both | prefab ≈ 1.0–1.3 ms, hand ≈ 1.3–1.45 ms, ratio ≈ 0.75–0.95 | < 2 ms, ≤ 1.25× | confirmed |
| the same with one override and a position each | + override match (≈ 12) and 2 float stores | ≈ 1.05–1.35 ms | < 2.2 ms | confirmed |
| 512 map objects (two prefabs, a hook reading one property each) into an empty world | per object: `map.object(i)` (out of line, a 40-byte struct through the stack) ≈ 50, `get_string("prefab")` ≈ 30, type cache hit ≈ 5, spawn ≈ 260, hook: `get_int` ≈ 40 + `get<T>` ≈ 35 | ≈ 2.0–3.0 ms | < 4 ms | confirmed, moderate margin (risk 2) |
| 1000 `find` by run-time name (`enemy_000`..`enemy_127`, 9 characters) over 128 prefabs | one-pass FNV-1a ≈ 5 cycles per byte (dependent `eor`/`mul`), call, table probe with two dependent loads in L1 | ≈ 0.37–0.50 ms | < 0.5 ms | **at risk** (risk 1) |

As AC-7 says, the targets stay as written; T7.6 checks the disassembly against this table before the console run, the
console measures (T10.2), a missed target is optimised first (risks 1–3), then the budget is set from the measurement
with margin, written in the spec and the docs with the numbers.

### Self-check and bench (AC-2, AC-6, AC-7)

- **Self-check** (`examples/prefabs/self_check.{hpp,cpp}`, pure). Kinds (padding-free, `static_assert`ed): `A`
  (`Rect box` + `u32 layers`, 20 B, a `Rect` placement), `B` (`i32[16]`, 64 B), `C` (`Vec2`, 8 B, a `Vec2` placement),
  `D` (`u32`, with a user-provided destructor: the destroy path), `E` (`u16, u16`). Two worlds of 1024 entities, 4 tags
  each, identical descriptions with some kinds smaller than the world (`B` 300, `E` 200) so full kinds happen; `D`'s
  remove hook sometimes spawns (from a remove hook). A library (`WhenFull::Miss`, so misses are part of the script
  without asserting) of **16 prefabs**: plain ones, a hook-only one, three variants and a variant of a variant
  (replace in place, append, tag union), hooks that write a component from `SpawnInfo`, spawn a child prefab, or
  destroy their own copy (the stale handle is compared). The **hand side** never reads the library: its 16
  definitions are written out flattened by hand in a table (kinds, values, tags, placement, hook functions in chain
  order), and its spawn checks the world's and kinds' free room first (all or nothing by hand), then calls
  `spawn`/`add<T>(e, value)`/`add_tag` and the same hook functions. **50 000 operations** chosen by FNV-1a of (seed,
  step, salt), no `rand`: spawn (0–2 overrides, a position half the time; also inside `each<A>` and from `D`'s remove
  hook), destroy (also inside `each`), add/remove of `C` and `E` on existing entities, flush, rarely `clear()` (spawns
  inside are refused on both sides). After every step both sides returned the same handle bits (else FAIL at that
  step); every 1000 steps, and after every clear, a full-state FNV-1a of both worlds (each live entity's handle, each
  kind's live slots with their owners and bytes, tags) must be equal; the running checksum (returned handles + those
  states) gives a checkpoint every 10 000 operations. Golden values recorded from pc-debug in
  `kPrefabsCheckGolden`, equal in pc-release; the unit test and the ctest `prefabs_self_check_pc` hold them; the
  console logs `prefabs: <n> sum <checksum>` and `prefabs: PASS <checksum>` / `prefabs: FAIL step <n> ...`, 2000
  operations per frame.
- **Bench** (`examples/prefabs/bench.{hpp,cpp}` pure workloads, `bench_scene` timing them with the profiler, 300
  frames each, average and worst): AC-7's four workloads exactly. Spawn: two worlds of 256 entities with the same
  description (`Body` 20 B, `Big` 64 B, `Velocity` 8 B; 2 tags per entity), one prefab; the hand-written reference
  copies the same values from `const` objects (`add<T>(e, kBig)`), and the two run on **alternate frames** (same cache
  conditions; one ratio per run). Overrides: the same with `spawn_with(world, p, at, Velocity{...})`. Map: a map built
  in code at bench start (512 point/rectangle objects of types `coin` and `walker`, an int property `value` each), the
  world cleared each frame. Find: a library of 128 prefabs `enemy_000`..`enemy_127`, 1000 finds by run-time strings in
  a fixed shuffled order. One line per workload, `prefabs bench: <name> avg X ms worst Y ms (target < Z ms) PASS|FAIL`
  (the spawn line adds `hand H ms ratio R (≤ 1.25)`), then `prefabs bench: PASS` or `prefabs bench: FAIL (<names>)`
  after every number (spec 019/020's form); a Debug build logs `not judged (Debug)`.
- **The script** `examples/prefabs/tests/check.nnhs`: SELECT → every checkpoint line and the PASS line; then the
  showcase: the room's map summary line (`map: spawned 14 skipped 1 unknown 0 misses 0`), fixed touches with their
  expected `spawned: <name> slot n gen g at x,y tags ...` lines, L/R cycles with `selected: <name> kinds ... tags ...`
  lines, touches past the walker kind's capacity until `missed: walker (misses N)`, X (restart) with the same map line,
  screenshots; then Y → the bench lines and the verdict. `fail-on "prefabs: FAIL"`, `fail-on "prefabs bench: FAIL"`,
  `fail-on "] E "`.

### The platformer (AC-4)

**What changes.** A new `level_prefabs.{hpp,cpp}` creates the level's library (3 prefabs, 6 entries, ≈ 96 value
bytes): `coin` = `Body{{}, layers::kCoin}` (placement `&Body::box`, a rectangle object's size) + `Coin{1}`, tag
`coin`, hook `value ← object's int property "value"` (1 when absent, as before); `turret` = `Body{{}, layers::kEnemy}`
(placement `&Body::box`) + `Turret{}`, tag `turret`; `spark` = `Spark{{}, 1.0f, {}}` (placement `&Spark::position`) +
`Velocity{}`, tag `fx`, hook starting the fade (the tween set captured as a pointer; the fade's setter and
`on_finish` capture `[World*, Entity]` as now). `LevelPlay::spawn_things()` = `clear()`, the player's entity by hand
(as now: the spawn point stays game code), then `prefabs_.spawn_map_objects(world_, map_)`; `coin_count_` =
`world_.storage<Coin>().count()`. `Sparkles::burst` = 4 × `spawn_with(world, spark, at, Velocity{v})`. `spawn_coin`,
`TurretBook::spawn_turret` and the `strcmp` dispatch of `spawn_things` go away; doors and the spawn point stay in
`read_objects()` (they are skipped types: `door`, `spawn`).

**Why every golden line stays the same** (`B2418A28`, all 15 lines; the x, y, coins columns too):

- *Spawn order and slots.* Before: player, then in object order each `coin` and `turret` object (others ignored).
  After: player, then `spawn_map_objects` in object order, `spawn`/`door` skipped. Same entities, same order, so the
  same lowest free entity and kind slots.
- *Component values.* Coin: `Body{bounds, kCoin}` before; after, `Body{{}, kCoin}` copied, then the `Rect` placement
  writes x, y, w, h = the rectangle object's bounds (coins are rectangles in both levels; the bonus level's are built
  by `add_object` with bounds) → the same five words; `Coin{value}` before, `Coin{1}` then `value ←
  get_int("value", 1)` after → the same int. Turret: `Body{box, kEnemy}` the same way; `Turret` was the slot's `T{}`,
  now a copy of a stored `Turret{}`: the same members (`sees` false, `cooldown` 0; the replay hashes members, not the
  padding). Spark: `Spark{at, 1, {}}` before; after, the stored `Spark{{}, 1, {}}` copied, `position ← at` (placement),
  `fade ←` the tween's handle in the hook → the same members; `Velocity{v}` by override → constructed once from `v`.
- *Tags and hooks.* One tag each, written into word 0 of a new record, as `add_tag` did. The spark's fade starts in the
  hook, after the spawn, adds and tag, exactly where `burst` started it; the `TweenSet` hands out the same handle.
- *Capacity checks.* `spawn_coin`/`spawn_turret` returned null quietly past 128 coins/4 turrets; level 1 has 47 coins
  and 2 turrets, the bonus level fewer, so the check never triggered (a full kind now asserts, which is louder).
- *Restart* = `clear()` + the same spawns → the first load's slots (FR-9).

If a byte differs anyway, the stage stops and this plan says why before any golden line is touched (spec AC-4).

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/prefabs.hpp` | new | the API above (≈ 260 lines with comments) |
| `engine/core/include/nnh/detail/prefab_values.hpp` | new | `ComponentValue`, `copy_component<T>`, `component_value<T>`, `Placement` helpers, `detail::prefab_spawn` declaration, `PrefabBlock` forward declaration |
| `engine/core/src/prefabs/library_layout.{hpp,cpp}` | new | pure byte layout of a block from a description and the flavour (header, hashes, table, records, names, entries, values) |
| `engine/core/src/prefabs/library_create.cpp` | new | `create` (validation, allocation, stamp), destructor (stored values destroyed), move, empty checks, doomed-while-spawning |
| `engine/core/src/prefabs/library_add.cpp` | new | registration: checks, commit, values copy, names (Debug/Development) |
| `engine/core/src/prefabs/prefab_flatten.cpp` | new | variants: merged entry list, tag union, inherited placement, hook-chain link |
| `engine/core/src/prefabs/library_find.cpp` | new | one-pass run-time hash, table insert and probe, `find` overloads |
| `engine/core/src/prefabs/prefab_spawn.cpp` | new | `spawn`, `spawn_with`'s out-of-line part: kinds, overrides, reservation, copies, placement, hook chain, misses |
| `engine/core/src/prefabs/prefab_map.cpp` | new | `spawn_map_objects`, `check_map_objects`, the level error message (the only file including `<nnh/tilemap.hpp>`) |
| `engine/core/src/prefabs/library_queries.cpp` | new | `count`, `valid`, `name`, `kinds`, `tags`, `has`/`value` lookup, `stats` |
| `engine/core/src/prefabs/prefab_internal.hpp` | new | block structs (`PrefabBlock` header, `PrefabRecord`, `PrefabEntry`), log channel `prefabs`, tag `prefabs` |
| `engine/core/src/entities/entities_internal.hpp` | new | `world_kind_of`, `world_reserve`, `world_component` (above) |
| `engine/core/src/entities/world_reserve.cpp` | new | the all-or-nothing reservation |
| `engine/core/src/entities/world_internal.hpp` | changed | inline `take_entity`, `attach_component` shared by spawn, add and reserve |
| `engine/core/src/entities/world_spawn.cpp`, `world_components.cpp` | changed | use the shared helpers (no behaviour change) |
| `engine/core/CMakeLists.txt` | changed | `_nnh_prefabs_sources`; `world_reserve.cpp` in `_nnh_entities_sources`; headers |
| `tests/prefabs/prefab_fixture.hpp` | new | padding-free kinds, a `Counted` kind, a world maker, helpers |
| `tests/prefabs/test_library_layout.cpp`, `test_library_create.cpp`, `test_add.cpp`, `test_names.cpp`, `test_variants.cpp`, `test_spawn.cpp`, `test_overrides.cpp`, `test_placement.cpp`, `test_spawn_hooks.cpp`, `test_all_or_nothing.cpp`, `test_map_objects.cpp`, `test_data_values.cpp`, `test_queries.cpp`, `test_prefab_misuse.cpp` | new | FR-1..FR-16, AC-1, AC-3 |
| `tests/entities/test_world_reserve.cpp` | new | the reservation against spawn/add/add_tag on twin worlds; refusals change nothing but counters |
| `tests/prefabs_example/test_self_check.cpp` | new | AC-2 golden on `examples/prefabs/self_check.cpp` |
| `tests/fuzz/fuzz_prefab_ops.cpp` | new | bytes → the self-check's operations (prefab side vs hand side), mini-fuzz in `nnh_tests`; libFuzzer with `NNH_BUILD_FUZZERS` |
| `tests/data/maps/make_fixtures.py`, `tests/data/maps/good_prefabs.tmj` | changed / new | AC-3's map: two object layers; objects by type and by `prefab`, a point and rectangles, a `value` property, a door, a spawn point, a misspelt `prefab`, an int `prefab` |
| `tests/CMakeLists.txt` | changed | the tests, the map conversion (`good_prefabs.nnhmap`), the example sources, ctest `prefabs_self_check_pc` |
| `examples/prefabs/` | new | `main.cpp`, `demos.hpp` (`PREFABS_DEMO`), `components.hpp`, `tags.hpp`, `data_types.cpp` (schema: `EnemyData`), `room.{hpp,cpp}` (world, library, registrations, map spawn, movement), `roster.{hpp,cpp}` (one prefab per enemy data asset), `room_draw.{hpp,cpp}`, `hud.{hpp,cpp}`, `showcase_scene.{hpp,cpp}`, `self_check.{hpp,cpp}`, `self_check_scene.{hpp,cpp}`, `bench.{hpp,cpp}`, `bench_scene.{hpp,cpp}`, `art/make_art.py`, `art/make_room.py`, `assets/rooms/room.tmj`, `assets/tiles/*`, `assets/enemies/*.enemy`, `CMakeLists.txt`, `tests/check.nnhs` |
| `examples/CMakeLists.txt` | changed | `add_subdirectory(prefabs)` |
| `examples/platformer/level_prefabs.{hpp,cpp}` | new | the level's library: `coin`, `turret`, `spark` |
| `examples/platformer/coins.{hpp,cpp}`, `turrets.{hpp,cpp}`, `sparkles.{hpp,cpp}`, `level_play.{hpp,cpp}`, `CMakeLists.txt` | changed | spawn functions removed, map spawn, sparkles by prefab |
| `tests/platformer/test_coins.cpp`, `test_turrets.cpp`, `test_sparkles.cpp`, `test_level_prefabs.cpp` | changed / new | the same cases over prefab spawns; the library's three prefabs |
| docs (stage 11) | new / changed | `docs/api/prefabs.md`, `docs/api/entities.md` (link from "Worlds"), `docs/api/data.md` (pattern link), `docs/guides/levels.md` ("Placing prefabs in Tiled"), `docs/architecture/decisions.md` (D36 implemented), `patterns.md` (Prototype row), `docs/REFERENCES.md`, `CLAUDE.md`, `docs/ROADMAP.md`, `specs/README.md`; `scripts/check_release_strings.sh` if the module gets a Development-only log text |

### Data and memory

| Item | Size (3DS / PC) | Notes |
|------|-----------------|-------|
| `Prefab`, `PrefabName` | 4 B | |
| `PrefabLibrary` | 4 B / 8 B + one block | tag `prefabs` or the game's |
| block header | ≈ 64 B | capacities, counts, stats, stamp, spawn depth, doomed flag, array pointers |
| per prefab | 80 B / 88 B (+ 32 B Debug/Development) | record 72 / 80 (entry range, counts, placement 4, hook link 4, `Tag[8]` 32, `SmallFunction` hook 24 / 32), hash 4, table 4 (two `u16` slots); the name |
| per entry | 16 B / 32 B | type id, value offset + owner bit, copy, destroy |
| per value | `sizeof(T)` + alignment padding | variants store only their own values |
| `PrefabOptions` | ≈ 64 B / 80 B | registration only (on the stack) |
| `ComponentValue` | 24 B / 40 B | registration and overrides only (on the stack) |

Examples (3DS, Release): the platformer's library (4 prefabs of room, 8 entries, 128 value bytes) ≈ 0.6 KB; the
example's (≈ 24 prefabs, 96 entries, 2 KB of values) ≈ 5.5 KB; the spec's 128 prefabs × 4 components of 16 bytes:
10 KB (prefabs) + 8 KB (entries) + 8 KB (values) ≈ **26 KB**. The spec's expectation ("well under 16 KB") can't hold
with 16 bytes per entry: the entries and values alone are 16 KB; the per-prefab part is dominated by the 8 inline tags
(32 B) and the hook (24 B). Both are kept: they make a spawn read one record with no indirection. Everything is in the
Heap region (CPU-only data). The world's block is unchanged (the reservation adds code, no data).

### Platform work

- **pc / 3ds:** none. Portable core code, no contract change; the 3DS runs the self-check, the showcase's touch spawns,
  the bench and the platformer (stage 10).
- **fake:** none. Tests run on the fake platform; map tests load converted fixtures from `NNH_TEST_MAPS_DIR`, data tests
  use spec 014's test game (`NNH_TEST_DATA_GAME_DIR`).

## Test strategy

All unit tests run in `nnh_tests` on the fake platform (Windows and VM 120). A `Counted` kind (constructions,
destructions, copies) checks balance in the library and in worlds; the assert hook catches misuse; the allocation
counter checks "no allocation after creation"; a block hash checks "registration refused changes nothing". The
**twin-world** pattern (one world filled by prefab spawns, a twin by hand-written calls from an independent table)
checks equivalence in unit tests (small cases) and in AC-2's script (50 000 operations).

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `test_library_create.cpp`: capacities 0/1025/−1 → `InvalidArgument`, `OutOfMemory`, block size equals `library_layout` (tag stats; Debug vs Release names), move, moved-from, destructor destroys stored values (`Counted` balance), several libraries into one world and one library into two worlds; `test_library_layout.cpp`: offsets aligned, totals by formula |
| FR-2 | `test_add.cpp`: entries in order, tags, placement, hook stored; hook-only prefab; 33 entries / 9 tags → `LimitReached`; a kind twice → `InvalidArgument`; capacities (prefabs, entries, value bytes with alignment) → `LimitReached` with the block hash unchanged; `Counted` copies at registration only; the compile-time checks in a comment-documented negative test list (non-copyable type, alignas(32), non-Vec2/Rect member) |
| FR-3 | `test_names.cpp`: `prefab_name` constexpr vs run-time (literal, sized buffer, one-pass hash on FNV vectors), `find` by all three forms gives the same handle, duplicate name and a crafted colliding hash refused, unknown → null, table full at capacity probes correctly (wrap-around), `name()` in Debug vs the Release path |
| FR-4 | `test_variants.cpp`: replace in place, append, shared values (no copy for inherited entries; the base's destroyed once), tag union with duplicates and the 8 limit, placement inherited and replaced, hooks base-then-variant, variant of a variant, a 9-hook chain → `LimitReached`, base from another library → `InvalidArgument` |
| FR-5 | `test_spawn.cpp`: twin worlds — handles, slots, component values and tags equal to `spawn`/`add<T>(e, value)`/`add_tag`; lowest free slots; spawn inside `each` (not visited), from a remove hook, into a world with quarantined slots |
| FR-6 | `test_spawn_hooks.cpp`: `SpawnInfo` (position, object, index) for code and map spawns, hooks see components and tags in place, spawn and destroy from a hook, destroying the new entity (stale handle returned), nested prefab spawns, the chain order |
| FR-7 | `test_overrides.cpp`: replace (constructed once: `Counted`), add a kind, override of the placement kind (position wins), duplicate type → compile error (documented) and twice in the array form asserts; `test_placement.cpp`: `Vec2` and `Rect` members, point and rectangle objects, keep-size, a position without placement asserts and is ignored |
| FR-8 | `test_all_or_nothing.cpp` and `tests/entities/test_world_reserve.cpp`: full world, each kind full (first, middle, override-only), unregistered kind — nothing created (counts, extents, records unchanged), miss counted in the library and the world, assert for `WhenFull::Assert`, quiet for `Miss`; inside `clear()` null without miss or assert |
| FR-9 | `test_map_objects.cpp` on `good_prefabs.nnhmap`: by type and by `prefab`, the layer filter, map order, skipped (door, spawn, empty type), unknown explicit names logged with id, name and layer (log hook) and asserting, a non-string `prefab`, `check_map_objects` equals the spawn's summary without spawning, restart after `clear()` gives the first load's slots, the type cache (same pointer, different pointers with equal text) |
| FR-10 | `test_data_values.cpp`: a prefab per `Enemy` asset of spec 014's test game (`data::name<Enemy>(i)` as names), spawned components equal `data::load`'s struct; an include check: no prefab object references a `data` symbol |
| FR-11 | `test_queries.cpp`: `count`, `valid`, `kinds`, `tags`, `has<T>`, `value<T>`, stats (each counter, including map counters) |
| FR-12 | `test_spawn.cpp`: `events::stats()` unchanged by registration and spawns; a spawned entity has only the prefab's kinds and tags |
| FR-13 | AC-2's golden on WinLibs GCC, GCC 13/14, clang 18 (VM 120) and the console |
| FR-14 | `test_prefab_misuse.cpp`: allocation counter unchanged over 100 000 spawns after creation; no log in the normal path; bench (AC-7) |
| FR-15 | `test_prefab_misuse.cpp`: every listed misuse asserts (hook) and is harmless in the Release path; a library destroyed from its own hook |
| FR-16 | include check (a TU including `<nnh/prefabs.hpp>` pulls no tilemap/data/collision/tween/gfx header); T8.4: `hello` sizes and `nm`, the code-only probe has no tilemap symbol, spec 020's tests and goldens unchanged |
| AC-2 | `tests/prefabs_example/test_self_check.cpp`, ctest `prefabs_self_check_pc`, the console's `prefabs: PASS` |
| AC-3 | `test_map_objects.cpp`, `test_data_values.cpp` |
| AC-4 | ctest `platformer_replay_matches_script` (unchanged lines), `level.nnhs --pc`, `test_coins.cpp`, `test_turrets.cpp`, `test_sparkles.cpp`, `test_level_prefabs.cpp` |
| AC-5 | PC screenshots (room, variant, override, full-kind miss) read back |
| AC-6 | `examples/prefabs/tests/check.nnhs` with `--pc`, then on the console |
| AC-7 | `prefabs bench:` lines on the console; T7.6's disassembly table; `gate.nnhs`, the replay's 60 Hz verdict and `entities bench: PASS` on the console |
| AC-8 | stage 0 references compared after stages 6 and 8; `hello` sizes; the probe; build times |

## Risks and alternatives

1. **`find` at 0.37–0.50 ms against 0.5 ms.** Mitigations in order, after the console says so: unroll the hash by four
   bytes per iteration (fewer loop instructions; the `mul` chain stays), keep the table's first probe straight-line
   (no loop for a hit), store the table as `{u16 index, u16 hash>>16}` pairs so a miss is rejected without touching
   the hash array. Then AC-7's measurement-with-margin policy.
2. **Map spawning is dominated by the tilemap's own calls** (`object()` returns a 40-byte struct through the stack;
   property lookups compare names). If it misses 4 ms: look up `"prefab"` only on objects that have properties
   (`properties.count()` first), keep the type cache, and if needed add a tilemap-internal object accessor (no public
   change) — measured first.
3. **Spawn ratio** (estimated 0.75–0.95, target ≤ 1.25): if the console disagrees, the first suspects are the
   per-entry kind lookups (cache the world's kind indices per prefab, keyed by a world serial number that spec 020's
   block would gain — internal only) and the copy calls (inline copies for trivially copyable types up to 16 bytes as
   word stores).
4. **The reservation refactor touches spec 020's hottest code** (`spawn`, `world_add`). Guard: spec 020's tests,
   `C8FB9A41`, `B2418A28`, its T7.6 instruction counts (`world_add` ≈ 92, `spawn` ≈ 65) re-checked in T1.4 and T7.6,
   and `entities bench: PASS` on the console (T10.2 run 4).
5. **Padding bytes** differ between a slot's `T{}` and a copy of a stored `T{}` for types with padding; nothing hashes
   padding (the platformer hashes members; AC-2's kinds are padding-free and `static_assert`ed). Documented.
6. **Spec 020's non-goal** ("nothing here needs to change"): the change is internal (no public line of
   `<nnh/entities.hpp>` changes). Alternative rejected: public `can_add`/`add_bytes`.
7. **Debug-only names** make a Development block 32 B per prefab bigger than Release; the layout function takes the
   flavour, and the tests run both paths (pc-debug, pc-release).
8. **A hook that destroys the world it is given** (misuse in game code): the remaining hooks get an empty world and
   assert; documented. A hook that destroys the library is handled (doomed block).
9. **Map strings and the type cache**: the cache compares pointers only within one call; a map edited by a hook during
   `spawn_map_objects` (setting properties may move strings) only causes cache misses, never a wrong prefab; the
   `MapObject` given to the hook is valid during the hook only (documented, as spec 012's views).

## Decisions taken by the plan (confirm with the plan's approval)

The spec's "Decisions to take" are approved as recommended. These smaller ones are the plan's; each is the simplest
that satisfies the spec, flagged here so none is silent:

1. **Names:** module `prefabs`, header `<nnh/prefabs.hpp>`, `PrefabLibrary`, `Prefab` (= `Handle<PrefabLibrary>`),
   `PrefabName`/`prefab_name()`, `PrefabOptions`, `Placement`/`place()`, `ComponentValue`/`component_value()`,
   `SpawnInfo`, `SpawnHook`, `WhenFull`, `spawn`, `spawn_with`, `spawn_map_objects`, `check_map_objects`,
   `MapSpawnSummary`, `PrefabStats`, `KindId`/`kind_id<T>()`.
2. **`WhenFull` (library option, default `Assert`).** FR-8 asserts on a full capacity, but AC-5 shows a full-kind miss
   on screen and AC-6 runs it in a Development build, where an assert stops the game. `WhenFull::Miss` makes a
   library's full-capacity refusals quiet misses (as spec 020's pools); the example's touch library and AC-2's script
   use it; the platformer and the default keep FR-8's assert. An unregistered kind and an unknown explicit map name
   always assert.
3. **Up to 8 hooks per prefab** (its own and its bases'): a 9th → `LimitReached` (the spec's "bounded by the library's
   capacities", made a fixed per-prefab limit like 32 entries and 8 tags).
4. **Variants share their base's stored values** for entries they don't replace (`value_bytes` counts own values
   only).
5. **`check_map_objects` logs and asserts nothing**; it reports `unknown` and `first_unknown`. `spawn_map_objects` logs
   every level error and asserts.
6. **A `prefab` property that is not a string** is a level error (loud), not a fallback to the type; an object with an
   empty type and no `prefab` property counts as skipped.
7. **A `Vec2` member from a rectangle object gets its top-left** (x, y), as Tiled stores it; a hook can centre it.
8. **Compile-time refusals:** a value type that is not copy-constructible or aligned over 16 bytes, a placement member
   that is not `Vec2`/`Rect`, an override that is a bare `nnh::Vec2` (it would read as a position), the same override
   type twice in `spawn_with`.
9. **Release keeps no names**; Debug/Development keep 31 bytes per name (messages, `name()`, the example's log lines).
10. **A miss counts in the world too** (its spawn misses, or the full kind's misses): the world's counters count what
    its capacities refused, whoever asked.
11. **Example controls** (`examples/prefabs`): L/R cycle the selected prefab, a touch on the bottom screen spawns it at
    the touched world point, A spawns it at the room's centre with a component override (the gold coin: `Coin{5}`), X
    restarts the room (`clear()` + map spawn: the first load's slots), SELECT the self-check, Y the bench, START quits.
12. **Console stage:** four scripted runs, each started with Homebrew Launcher → Y (prefabs `check.nnhs` with the
    bench, platformer `level.nnhs`, platformer `gate.nnhs`, entities `check.nnhs` for spec 020's bench after the
    reservation refactor).
13. **AC-7 targets kept as written** (the maintainer's rule).

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | Portable core; spec 020's rules plus fixed orders; copies, no recomputation; AC-2 on every compiler and the console |
| II | ✅ | One block per library (≈ 80 B per prefab, 16 B per entry, the values); one call per spawn into the world; every AC-7 workload under 16 KB of random access; disassembly checked before the console; one target at risk with AC-7's fallback. The spec's "well under 16 KB" memory example is ≈ 26 KB (explained above) |
| III | ✅ | One light header over `entities.hpp`; thin templates; measured budgets |
| IV | ✅ | No events |
| V | ✅ | No allocation after creation, no `virtual`, no RTTI, no exceptions |
| VI | ✅ | Tests first; twin worlds; AC-2 golden; the platformer's lines unchanged |
| VII | ✅ | Eight small sources + one entities source; one public header |
| VIII | ✅ | API page, levels guide, data page link, example, platformer, D36 |
| IX | ✅ | Own code, prior art cited |
| X | ✅ | Unknown explicit names are loud level errors; misuse loud in Debug, harmless in Release; no parser |
| XI | ✅ | Optional module; spec 020's public API unchanged; tilemap forward-declared and isolated in one object file; `examples/entities` keeps hand-written spawns |

No constitution exception is requested. D36 is new (see [decisions.md](../../docs/architecture/decisions.md)); D35
gets a dated note (an entities-internal reservation for prefabs, no public change); the patterns page gains the
Prototype row. One AC-7 target is flagged at risk and keeps the spec's measurement-with-margin fallback; the spec's
memory expectation is corrected above.

## Stage 0 results

Measured 2026-11-23 on the maintainer's Windows machine (WinLibs GCC, devkitARM), at `7b00e5f`, before any spec 021
change. Scripts in `build/ref-021/` (not committed).

- `hello` sizes (`size`, text / data / bss): pc-release `hello.exe` 130 572 / 22 020 / 211 696; pc-debug 198 464 /
  26 756 / 214 352; 3ds-release `hello.elf` 201 012 / 28 352 / 106 680 (`.3dsx` 246 400 bytes); 3ds-dev `hello.elf`
  217 728 / 32 872 / 109 288 (`.3dsx` 267 844). `nm -C` lists saved as `build/ref-021/hello.{pc,3ds}.nm` (2966 and
  1364 symbols). All equal spec 020's final numbers.
- `.text` of 3ds-release ELFs: `platformer.elf` 350 048, `entities.elf` 271 024.
- Build times (3 runs, `btime.sh`): edit `examples/entities/arena.cpp` → simulator (pc-debug) 2.49, 2.03, 1.97 s;
  → `.3dsx` (3ds-release) 4.78, 4.79, 4.91 s; clean `nnh_core` (pc-debug, parallel) 7.9 s.
- Header cost (`hcost.sh`, `g++ -std=c++20 -fsyntax-only` of a one-line TU, 6 runs): `<nnh/entities.hpp>` 8555
  preprocessed lines, 0.45–0.53 s.
- 3ds-dev disassembly (`fcount.py`, whole functions, slow paths included; the stage 1 guard): `World::spawn()` 109
  instructions (2 stack accesses, no call on the common path), `detail::world_add` 143 (2 stack accesses). Spec 020's
  common-path counts (`spawn` ≈ 65, `world_add` ≈ 92) are the same code. `bench.o` of `examples/entities` saved
  (`dis0/bench_entities.dis`): its inline `get`/`Storage::get`/`each`/pool paths don't include anything stage 1 touches.
- T0.2: reference screenshots in `build/ref-021/ref/`: `hello`, `sprites`, `scenes` at 30, `camera`
  (`CAMERA_DEMO=zoom1`) and `queries` (`QUERIES_DEMO=all`) at 30, `entities` (showcase) at 30, platformer
  `PLATFORMER_DEMO=bench` at 90 (top screen compared), level 1 at 90, `PLATFORMER_DEMO=replay` at 312 and at 64 (the
  first sparkles). `compare.sh` retakes them into `now/`: all identical twice.
- T0.3: ctest pc-debug 265/265 (`platformer_replay_matches_script`, `entities_self_check_pc` → `entities: PASS
  C8FB9A41`), pc-release 257/257; `level.nnhs --pc` → `nnh: PASS (79 lines, frames 1-1553)` with
  `replay: done 900 frames sum B2418A28: 60 Hz PASS`; `entities/tests/check.nnhs --pc` → `nnh: PASS (112 lines)`.
  `nnh_tests` 2199 tests; `kMaxTests` 4096: no change needed.

## Stage 1 results

- **T1.1** `take_entity(w, slot)` and `attach_component(w, kind, entity, record, slot)` are inline in
  `world_internal.hpp`; `World::spawn()` and `world_add()` call them. The 3ds-dev disassembly of `world_add` is
  byte-identical to stage 0's (143 instructions); `World::spawn()` differs by one operand order (`cmp r0, ip` for
  `cmp ip, r0`), still 109 instructions; `bench.o` of `examples/entities` identical. So spec 020's T7.6 common paths
  (`world_add` ≈ 92, `spawn` ≈ 65) and console numbers stand.
- **T1.2/T1.3** `src/entities/entities_internal.hpp` (`world_kind_of`, `ReserveRefusal`, `world_reserve`,
  `world_component`) and `world_reserve.cpp` (61 lines). `world_reserve` finds every free slot first (`find_free`
  only moves a hint past full words), then takes them with the shared helpers and writes the tags into the new record;
  extra tags past the world's `tags_per_entity` are dropped (the prefab module asserts before). 3ds-dev: 264
  instructions in all (21 stack accesses, mostly the slow paths' saves; the common path is read in T7.6).
- Tests: `tests/entities/test_world_reserve.cpp` (10): `world_kind_of` on 3 and 32 kinds (= `kind_index<T>`, empty
  world and null id → −1), `world_component`, the twin-world sequences (20 seeds × 2000 operations of spawns with
  0–3 kinds in any order and 0–2 tags, destroys, removes, flushes, clears and spawns inside `each`, on kinds of 48 /
  30 / 12 so full kinds and full worlds happen; every record word, bitmap, counter, owner and component byte compared
  after each operation), a reservation inside a remove hook, and the refusals `Empty`, `WorldFull` (spawn miss + 1),
  `KindFull` (the first full kind in call order, its misses + 1), `Clearing` (no counter), each leaving the rest of
  the world byte-identical.
- T1.4: ctest pc-debug 265/265, pc-release 257/257; 3ds-release and 3ds-dev without a warning; `nnh_tests` 2209;
  `hello` sizes and `nm` lists equal stage 0's; `level.nnhs --pc` PASS (`B2418A28`), `entities/tests/check.nnhs --pc`
  PASS (`C8FB9A41`).

## Stage 2 refinements and results

- **Files** as planned: `include/nnh/prefabs.hpp`, `include/nnh/detail/prefab_values.hpp` (`KindId`/`kind_id<T>()`,
  `ComponentValue`/`component_value<T>()`, `Placement`/`place<C, M>()`, the copy/destroy function templates,
  `distinct_types<T...>()`), `src/prefabs/prefab_internal.hpp` (`PrefabEntry`, `PrefabRecord`, `PrefabBlock`,
  `PrefabPlan`, `PrefabAccess`, log channel and memory tag `prefabs`), `library_layout.{hpp,cpp}`,
  `library_create.cpp`, `library_find.cpp`, `library_add.cpp`, `prefab_flatten.cpp`, `library_queries.cpp`.
- **Overload resolution of `add` and `spawn_with`.** The variadic forms are constrained (`requires`) to class types
  that are not `ComponentValue` (and, for the first `spawn_with` form, not `Vec2`), so `add(name, options, list, n)`
  and `spawn_with(world, p, at_ptr, list, n)` always pick the array forms. Consequence (documented): a prefab value
  must be a class type (`int` alone is not a component value; wrap it in a struct).
- **Duplicate override types** are a `static_assert` through `detail::distinct_types<T...>()`.
- **Placement checks:** a placement member past its component's end is `InvalidArgument` when the component's value
  is the registration's own (the size is known there); `place()` can't produce one anyway.
- **Records** are 72 bytes on the 3DS and 80 on 64-bit hosts, entries 16 / 32, as planned (`static_assert`-like
  checks in `test_library_layout.cpp`); the plan's example (128 prefabs × 4 values of 16 bytes) is 26 624 bytes plus
  the header on the 3DS.
- **Stamps** come from a counter in `library_create.cpp` (1..65 535, wrapping to 1).
- Tests (`tests/prefabs/`, 32): `test_library_layout` (5), `test_library_create` (6), `test_names` (6: FNV vectors,
  1000 names, the three `find` forms, `"costarring"`/`"liquid"` — a real FNV-1a 32 collision — refused, names found
  on a shared slot and across the table's wrap-around, a full library of 128), `test_add` (10: order, tags, placement,
  hook, `Counted` balance, each refusal with the block's hash unchanged, alignment, the array form), `test_queries` (5).
- T2.6: a TU including only `<nnh/prefabs.hpp>` pulls 59 headers, none of tilemap, data, collision, tween,
  animation, gfx, pool or a standard container (`build/ref-021/inccheck.sh`); 8919 preprocessed lines, 0.44–0.49 s
  (`<nnh/entities.hpp>` 8555 lines, 0.47–0.49 s in the same session). ctest pc-debug 265/265, pc-release 257/257;
  3ds-release without a warning; `nnh_tests` 2241; `hello` sizes and `nm` lists equal stage 0's.

## Stage 3–4 refinements and results

- **Variants** (`prefab_flatten.cpp`): the base's entries keep their positions (so an inherited placement's entry
  index stays valid); an own value of a base kind replaces that entry, the others are appended in call order; the
  base's tags come first, then the own ones with duplicates dropped; the hook chain links to the nearest base with a
  hook of its own (`hook_base`), its length counts every hook in it. A placement given by a variant for an inherited
  kind can't be size-checked (inherited values' sizes aren't stored); `place()` makes valid offsets anyway.
- **The reservation's interface changed inside the entities module** (still internal, no public change):
  `world_reserve` writes each reserved component's **address** (`void** components`) instead of its slot number (the
  prefab module only needs addresses: no `world_component` call per copy), and reports
  `ReserveRefusal::TagsDropped` when it kept fewer tags than asked (the prefab module asserts on it; Release keeps the
  first ones). `world_kinds_of(world, first_id, stride, count, out)` resolves a prefab's entries in one call (straight
  compares for the first four kinds, then a scan) instead of one `world_kind_of` call per entry.
- **Spawn** (`prefab_spawn.cpp`, 190 lines): checks (empty library, null or foreign prefab, empty world), the entries'
  kinds in one call, overrides matched to entries (a kind twice asserts, the second ignored; an override-only kind is
  appended after the entries), the position check, one `world_reserve`, then per entry `destroy` (types with a
  destructor) and `copy` from the stored value or the override, the override-only kinds, the placement member, the
  spawn counter, the hook chain base first. Refusals: clearing → null, nothing counted; a full capacity → a library
  miss (and the world's own counter, inside the reservation), `WhenFull::Assert` logs `spawn of prefab "<name>": ...`
  (`#<index>` in Release) and asserts; a kind the world doesn't store → a miss and an assert whatever `WhenFull` says.
- **A library destroyed inside its own hook**: the destructor and move-assignment report it before changing anything
  (Debug stops there with the block still the library's); Release marks the block doomed, the chain stops, and the
  outermost spawn frees it.
- Tests: `test_variants` (10), `test_spawn` (8: twin worlds byte-identical, lowest free and quarantined slots, only the
  prefab's kinds and tags, no event with the engine shut down, spawns inside `each` and from a remove hook, several
  libraries and worlds, `Counted`), `test_overrides` (5), `test_placement` (4), `test_spawn_hooks` (5),
  `test_all_or_nothing` (7), `test_prefab_misuse` (3: the misuse list; 100 000 spawns/destroys with no allocation and
  no log line; the library destroyed from its own nested hook). `test_world_reserve` gained the address and
  dropped-tag case and `world_kinds_of`.
- Stage ends: ctest pc-debug 265/265, pc-release 257/257 (prefab tests in both: the Release paths of every misuse),
  3ds-release without a warning; `nnh_tests` 2285; `hello` sizes and `nm` equal stage 0's; `level.nnhs --pc` PASS
  (`B2418A28`), `entities/tests/check.nnhs --pc` PASS (`C8FB9A41`). (One stage-3 run's pc-debug links failed with
  "ld: memory exhausted" while another build was running on the machine; the rerun was clean.)

## Stage 5 refinements and results

- **Fixture** `tests/data/maps/good_prefabs.tmj`, written by `make_fixtures.py` (`prefab_map()`): layer `things` with
  a `coin` rectangle (`value` 5), a `coin` point, a `walker` point, a `walker` whose `prefab` is `flying_walker`, a
  `door`, a `spawn` point, an object with an empty type, object 12 `"coin"` with `prefab` = `coinn`, object 13 with an
  int `prefab`; layer `more` with one `coin`. Converted for `nnh_tests` (`good_prefabs.nnhmap`) and checked by the
  ctest `map_convert_accepts_good_prefabs`.
- **Map objects** (`prefab_map.cpp`, 112 lines): an object with any `prefab` property is explicit (a non-string one is
  the level error `object <id> "<name>" (layer <layer>): property "prefab" is not a string`); the type → prefab
  lookup is memoised per call by the type string's pointer (`detail::TypeCache`, tested directly with equal text at
  two pointers). The level error's text is also the assert's message, so a Debug run's last line names the object.
  **Refinement:** a map object whose prefab has **no placement member** spawns without a position and without an
  assert (its hook still gets the position in `SpawnInfo`); from code, a position without a placement member still
  asserts. `check_map_objects` counts nothing in the library's stats.
- **Data values** (`test_data_values.cpp`): one prefab per `Enemy` of spec 014's test game (`enemies/bat`,
  `enemies/goblin`), plus a `Health` struct converted from it; spawned components equal `data::load`'s bytes; the
  fake platform's `fs_calls` and `asset_reads` don't move while spawning.
- **Self-check** (`examples/prefabs/check_kinds.hpp`, `self_check_defs.cpp` (the hand table), `self_check_sides.{hpp,
  cpp}` (`Side<true>` through the library, `Side<false>` by hand, the same hook code), `self_check.{hpp,cpp}`). The
  kinds' "no padding" is asserted by sizes (`std::has_unique_object_representations` is false for float members).
  The hand side decides "room" exactly as the world does: a kind has a free slot iff live + quarantined < capacity,
  and live + quarantined = (live at the last flush) + (slots taken since): both sides keep that count, so the hand
  side's all-or-nothing matches the reservation without reading engine internals. Each step compares the result
  words, the sides' running sums (every handle a spawn or hook returned, every visit) and their miss counts; every
  1000 steps and after each clear, a full-state hash (live handles and tags, each kind's live slots, owners and bytes).
  Reached: 27 419 spawns, 3152 misses (kinds B and E full), 4006 spawns from hooks, 510 copies destroyed by their own
  hook, 1567 spawns from D's remove hook, 23 392 overrides, 8801 positions, 25 clears, 180 642 visits, 75 state
  checks, 661 live at most. A one-value mistake in the hand table fails at step 1000. **Golden `0x7BB7901E`**
  (checkpoints `3EBCC2FD`, `821C8B95`, `CC194CA2`, `C8360424`, `7BB7901E`), pc-debug and pc-release.
- **Fuzz** `tests/fuzz/fuzz_prefab_ops.cpp`: every 4 bytes are one self-check operation (`PrefabsSelfCheck::apply`);
  mini-fuzz `prefab_ops_target_survives_random_inputs` (100 inputs up to 8 KiB), libFuzzer `fuzz_prefab_ops` with
  `NNH_BUILD_FUZZERS`. The self-check's sources are compiled into `nnh_fuzz_targets` (which `nnh_tests` links).
- T5.6: ctest pc-debug 266/266, pc-release 258/258; 3ds-release without a warning; `nnh_tests` 2305; `hello` sizes and
  `nm` equal stage 0's; `compare.sh` every reference identical; `level.nnhs --pc` and `entities/check.nnhs --pc` PASS;
  `linkcheck.sh` (3ds-release objects): no prefab object refers to a data symbol, only `prefab_map.o` refers to tilemap
  symbols, and no other object refers to `prefab_map`'s functions.

## Stage 6 refinements and results

- **Files**: `examples/platformer/level_prefabs.{hpp,cpp}` (`level::LevelPrefabs{library, coin, turret, spark}`,
  `create_level_prefabs(TweenSet*)`, `kSparkFadeFrames`): `coin` = `Body{{0, 0, 12, 12}, kCoin}` (placement
  `&Body::box`) + `Coin{1}`, tag `coin`, hook `value ← get_int("value", 1)`; `turret` = `Body{{0, 0, 16, 16}, kEnemy}`
  (placement `&Body::box`) + `Turret{}`, tag `turret`; `spark` = `Spark{{}, 1, {}}` (placement `&Spark::position`) +
  `Velocity{}`, tag `fx`, hook starting the fade. The stored boxes have a size (12 × 12, 16 × 16) instead of the plan's
  `{}` so code spawns (the tests) get a sensible box; every map coin and turret is a rectangle object, whose size the
  placement writes, so the bytes are unchanged.
- `LevelPlay` owns the library (a member after the tween set); `spawn_things()` = `clear()`, the player by hand,
  `spawn_map_objects(world_, map_)`, `coin_count_` from the coin storage. `spawn_coin`, `TurretBook::spawn_turret` and
  the `strcmp` dispatch are gone; `read_objects()` still reads the spawn point and the doors (skipped types).
  `Sparkles(world, prefabs)`: `burst` = 4 × `spawn_with(world, spark, at, Velocity{v})`.
- Tests: `test_level_prefabs.cpp` (3: the prefabs' kinds, tags, values; map coins and a turret equal the old
  hand-written spawns on a twin world — the turret by members, `Turret` has padding; a sparkle equals the old `burst`'s
  entity with its fade, frame by frame until it is destroyed — the two tween sets' serials normalised). `test_coins`,
  `test_turrets`, `test_sparkles` keep every case and expected result, spawning through the library (coins and turrets
  by override, as map objects place them); "coins past their capacity" now asserts in Debug/Development (the level's
  library keeps `WhenFull::Assert`) and still leaves the 128 coins alone.
- **The golden lines are unchanged**: ctest `platformer_replay_matches_script` passes and `level.nnhs --pc` →
  `nnh: PASS (79 lines, frames 1-1553)` with the unchanged `replay: done 900 frames sum B2418A28: 60 Hz PASS` (two runs
  right after heavy builds reported `60 Hz FAIL` with the same `B2418A28` — a host frame-time spike, `bonus worst 41.7
  ms, missed 1/299`; three later runs: PASS). `compare.sh`: level 1 at 90, the replay at 64 and 312 and the bench
  identical. ctest pc-debug 266/266, pc-release 258/258; 3ds-release without a warning; `nnh_tests` 2308; `hello`
  unchanged.

## Stage 7 refinements and results

- **Files** as planned except: `art/make_art.py` writes only the 4-tile `assets/tiles/room.png` (floor, wall, door, rug)
  and `art/make_room.py` the room (`assets/rooms/room.tmj`, 25 × 15 tiles, 15 objects); the self-check is split into
  `check_kinds.hpp`, `self_check_defs.cpp`, `self_check_sides.{hpp,cpp}` and `self_check.{hpp,cpp}` (stage 5); there is
  no separate `components`/`tags` source (headers only). `PREFABS_DEMO` = `showcase` (default) | `check` | `bench`.
- **The room** (`room.{hpp,cpp}`): 64 entities, 2 tags each; kinds Body (`Rect box` — the placement member — and a
  colour), Walker (16: the kind whose budget the touches exhaust), Bob (remove hook kills its tween), Coin, Stats. The
  library (`WhenFull::Miss`): `walker`, `flying_walker` (variant: + Bob, a faster Walker in place, tag `flying`, hook
  starting a yoyo tween through a handle-capturing setter), `coin` (hook: `value` from the object, gold when > 1), and
  the roster (`roster.cpp`: one prefab per `assets/enemies/*.enemy` — beetle, golem, imp, wisp — named
  `enemies/<name>`). `~Room()` clears the world before the tween set goes (members go in reverse order).
- **Log lines**: `map: spawned 14 skipped 1 unknown 0 misses 0` (logged in frame 0, before a script can read it: the
  script expects it after its restarts), `selected: <name> kinds <list> tags <list>`, `spawned: <name> slot n gen g at
  x,y tags a,b`, `missed: <name> (misses N)`. Controls as decision 11.
- **ctests**: `prefabs_self_check_pc` (`PREFABS_DEMO=check`, 60 frames → `prefabs: PASS 7BB7901E` at frame 26) and
  `prefabs_check_script_pc` (`nnh run prefabs --pc --no-build --script examples/prefabs/tests/check.nnhs`, the
  console script played in the simulator; `RESOURCE_LOCK nnh_remote_port`).
- **Screenshots read back** (the script's, pc-debug): the room (walkers, two flyers lifted over their shadows, five
  coins and the gold one, the door), the variant spawned by a touch, the beetle and the gold override at the centre,
  the walker budget's miss on the HUD (`spawns 37 misses 1 entities 23/64`), the self-check's PASS, the bench lines.
- **PC bench** (pc-dev, x86, two runs; not representative of the console): spawn 0.046–0.048 ms (hand 0.043, ratio
  1.06–1.11), overrides 0.051 ms, map 0.106–0.108 ms, find 0.028–0.029 ms; all PASS.
- T7.7: `nnh run prefabs --pc --no-build --script examples/prefabs/tests/check.nnhs` → `nnh: PASS (114 lines, frames
  1-1389)`, twice (once by hand, once as the ctest).

### Stage 7 ARM pre-check (T7.6)

`arm-none-eabi-objdump -drC` of the 3ds-dev objects (`examples/prefabs/.../bench.o`, `src/prefabs/*.o`,
`src/entities/world_reserve.o`; devkitARM GCC, `-O2`), common paths counted by hand from the listings
(`build/ref-021/dis7b/`). Cycles: ≈ 1.9 per instruction (spec 020's console calibration), cache misses aside.

| Hot path | First build | Found | Fixed | After |
|----------|-------------|-------|-------|-------|
| `prefab_spawn`, before the reservation (handle, record, empty-world check, one `world_kinds_of` call) | ≈ 65 + the kinds call (≈ 35 for 3 ids) | — | — | ≈ 100 |
| `world_reserve`, 3 kinds and 2 tags | ≈ 210 (entity `find_free` straight-line ≈ 22, per kind find ≈ 23, per kind take ≈ 25, tags, the addresses) | the plan's ≈ 140 was low: each kind's take is the shared `attach_component` (live, fresh test, owner, slot field, mask) | none (it is exactly `world_add`'s bookkeeping minus its per-call checks) | ≈ 210 |
| copies, per entry | 16 + the copy function; `b->values` reloaded per entry (the indirect calls could alias it); the override test per entry; a `memset` call clearing `by_entry[]` when overriding | the reload; the per-entry test; the `memset` | values in a local, separate loops with and without overrides, a 32-bit "overridden" mask instead of clearing an array | 14 + the copy (`Body` 8, `Big` 12 — `ldm`/`stm`, `Velocity` 4); no `memset`/`memcpy` |
| tail (spawns counter, hook test, return) | ≈ 15 | — | — | ≈ 15 |
| `find(const char*)` | the one-pass hash inlined (GCC turns `× 0x01000193` into 5 shift-adds: ≈ 8 instructions per byte, a dependent chain), then a **call** to the probe (28 instructions) | the call | the probe `always_inline`d into each `find` (49 instructions in all, no call, no stack) | ≈ 9 × 8 + 20 |
| `spawn_map_objects`, per object | `map.object(i)` out of line (a 40-byte struct through the stack), a `type("prefab")` lookup, the type cache, a `SpawnSource` on the stack, the spawn | — (the tilemap's calls are the plan's risk 2) | — | ≈ 540 with the hook's `get_int` and `get<Coin>` |

No `vmrs`, no `__aeabi_*`, no `memcpy`/`memset` on any of these paths after the fix; spec 020's `World::spawn()` (109)
and `world_add` (143) are unchanged and `examples/entities`' `bench.o` is byte-identical to stage 0's.

**Expected at the console (T10.2)** — per entity: prefab spawn ≈ 100 + 210 + 14 × 3 + 27 + 15 ≈ 410 instructions,
hand-written ≈ 65 (`spawn`) + 3 × (92 + ≈ 20 inline) + 2 × 35 (`add_tag`) ≈ 470, `destroy` ≈ 350 in both:

| AC-7 workload | Estimate | Target | Verdict |
|---------------|----------|--------|---------|
| 256 prefab spawns + destroys + flush | ≈ 1.3–1.4 ms; hand ≈ 1.45–1.55 ms; ratio ≈ 0.9 | < 2 ms, ≤ 1.25 | PASS expected |
| the same with an override and a position | ≈ 1.45–1.6 ms (+ the override match ≈ 25, the wrapper's `SpawnSource`, the Rect stores) | < 2.2 ms | PASS expected |
| 512 map objects | ≈ 2.0–2.6 ms (memory: the map's object and property arrays are walked in order) | < 4 ms | PASS expected, moderate margin |
| 1000 `find` by run-time name | ≈ 0.40–0.48 ms (≈ 9 × 7 cycles of hash chain + ≈ 40 of probe and loop) | < 0.5 ms | **at risk** (risk 1: unroll the hash, `{index, hash >> 16}` table pairs) |

No result changed: the self-check's `7BB7901E`, spec 020's `C8FB9A41` and the platformer's `B2418A28` hold.

## Stage 10: console run (prepared)

T10.1 on the PC (results in "Stage 10 checks" below): `scripts/dkp.sh cmake --build --preset 3ds-release` and the
Development builds of the three games, `scripts/dkp.sh cmake --build --preset 3ds-dev --target prefabs_3dsx
platformer_3dsx entities_3dsx`, without a warning; the T7.6 table still describes the objects; 3ds-release `hello.elf`
201 012 / 28 352 / 106 680 as in stage 0.

T10.2, on an Old 3DS, nobody touching it after Y. Four runs, each: open the Homebrew Launcher, press **Y**, then on the
PC (`nnh` builds the Development `.3dsx`, sends it and plays the script; logs and screenshots in `build/logs/`):

1. `build/pc-debug/tools/nnh/nnh run prefabs --script examples/prefabs/tests/check.nnhs` (about 40 s). Expected, as on
   the PC (pc-debug): `prefabs: self-check started`, `prefabs: 10000 sum 3EBCC2FD`, `20000 … 821C8B95`,
   `30000 … CC194CA2`, `40000 … C8360424`, `50000 … 7BB7901E`, **`prefabs: PASS 7BB7901E`**;
   `map: spawned 14 skipped 1 unknown 0 misses 0`;
   `selected: flying_walker kinds Body,Walker,Bob tags enemy,flying`;
   `spawned: flying_walker slot 14 gen 1 at 120,64 tags enemy,flying`; `selected: coin kinds Body,Coin tags coin`;
   `selected: enemies/beetle kinds Body,Walker,Stats tags enemy,roster`;
   `spawned: enemies/beetle slot 15 gen 1 at 160,100 tags enemy,roster`; `spawned: coin slot 16 gen 1 at 196,116 tags
   coin`; `selected: walker kinds Body,Walker tags enemy`; `spawned: walker slot 17 gen 1 at 70,40 tags enemy` … slots
   18–22 at x 100, 130, 160, 190, 220; `missed: walker (misses 1)`; `map: spawned 14 skipped 1 unknown 0 misses 0`
   (X); `prefabs bench: started`, then `prefabs bench: spawn avg X ms worst Y ms hand H ms ratio R (target < 2.0 ms,
   <= 1.25) PASS|FAIL`, `prefabs bench: overrides avg … (target < 2.2 ms)`, `prefabs bench: map avg … (target < 4.0
   ms)`, `prefabs bench: find avg … (target < 0.5 ms)`, and the verdict `prefabs bench: PASS` or
   `prefabs bench: FAIL (<names>)`; `nnh: PASS (124 lines, …)` (114 before the second console run: a `wait 3` now
   follows every `expect` that an input follows, because on the console the input for frame 107 once reached it on
   frame 108). **Expected from the ARM pre-check**: every logic line
   and the self-check equal the PC's; spawn ≈ 1.3–1.4 ms with a ratio ≈ 0.9, overrides ≈ 1.45–1.6 ms, map ≈ 2.0–2.6
   ms PASS; `find` ≈ 0.40–0.48 ms, at risk. A FAIL fails the run (`fail-on "prefabs bench: FAIL"`) after every number is
   logged: record them here; T10.3 then applies the plan's risk 1–3 mitigations, or the maintainer sets the budget from
   the measurement with margin (AC-7). Read the screenshots `room`, `self-check`, `variant`, `override`, `miss`, `bench`
   (top and bottom): the same pictures as a `--pc` run's.
2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` (about 40 s). Expected,
   unchanged since spec 020: `replay: 60 level1 x 121 y 242 coins 1 sum 720BE2BE`, `120 … 0330ACD0`, `180 … BA334AA2`,
   `240 … 74731BB8`, `300 … 421D79BE`, `360 … 4955DE42`, `420 … CC4B644A`, `480 … 4E9131BA`, `540 … F2718034`,
   `600 … A8292126`, `660 bonus … D76BB2E3`, `720 … 2AFED843`, `780 … FE036A4F`, `840 … FC1690D6`,
   `replay: done 900 frames sum B2418A28: 60 Hz PASS`; `nnh: PASS (79 lines, …)`. The level's coins, turrets and
   sparkles are prefabs now: a different sum here would be the swap's (run 1's self-check covers the module).
3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs` (about 2.5 min).
   Expected: `bench: PASS 4 layers + 300 sprites, …` (spec 012's gate); `nnh: PASS`.
4. `build/pc-debug/tools/nnh/nnh run entities --script examples/entities/tests/check.nnhs` (about 45 s). Expected, as
   in spec 020's third console run: every logic line, `entities: 10000 sum 723FB297` … `100000 sum C8FB9A41`,
   **`entities: PASS C8FB9A41`**, and `entities bench: PASS` with numbers near spec 020's (each ≈ 0.475, spawn ≈ 1.29,
   find ≈ 0.144, get ≈ 4.78, storage get ≈ 4.20, pool ≈ 0.441 ms): `spawn()` and `world_add()` now share inline helpers
   with the reservation, but their 3DS code is the same (T1.4).

## Stage 8 refinements and results

- **T8.2 Release strings**: `scripts/check_release_strings.sh` passes on `build/3ds-release/examples/prefabs/prefabs.elf`
  and `build/pc-release/examples/prefabs/prefabs.exe`. The module logs only errors (kept in Release by design) and
  stores no prefab names in Release; nothing to add to the script's list.
- **T8.3 Code-only games link no tilemap code**: a probe (`build/ref-021/probe/`, not committed: a world, a library,
  `add`, `spawn(at)`, `spawn_with`, `find`) built as a 3ds-release game: its `nm -C` has no `nnh::Tilemap`,
  `nnh::Properties`, `load_tilemap`, `prefab_map` or `map_objects` symbol (the only "tilemap" match is the log channel
  `nnh_log_channel_tilemap`, which `hello.elf` has too). `linkcheck.sh` holds.
- **T8.4 Opt-in and regression**: `hello` sizes and `nm` lists equal stage 0's on PC and 3DS; `compare.sh` every
  reference identical; every example's console script passes with `--pc` (actions, animation, camera, data, entities,
  foundation ×2, input, platformer `gate`, `level`, `zoom_gate`, prefabs, queries, tweens). `.text` (3ds-release):
  `platformer.elf` 350 048 → 360 904 (+10.9 KB: the prefab module's registration, spawn and map code), `prefabs.elf`
  343 236 (3ds-dev 364 400), `entities.elf` 271 024 (unchanged).
- **Build times** (3 runs, same session): an `examples/prefabs/room.cpp` edit → simulator 2.13–2.30 s, `.3dsx`
  5.04–5.13 s; `examples/prefabs/roster.cpp` 2.05–2.28 s / 4.73–4.92 s; the reference `examples/entities/arena.cpp`
  2.05–2.09 s / 4.88–4.99 s (stage 0: 1.97–2.49 s / 4.78–4.91 s). The edit is link-dominated; `room.cpp` costs ≈ 0.1–
  0.2 s more because `room.hpp` brings `<nnh/tilemap.hpp>` (gfx, collision) and `<nnh/tween.hpp>`, not because of the
  prefab header (`<nnh/prefabs.hpp>` 8919 preprocessed lines, 0.49–0.51 s against `<nnh/entities.hpp>`'s 8555 lines,
  0.44–0.48 s: within the plan's 0.05 s). **Refinement:** the registrations and their hooks moved from `room.cpp` to
  `room_prefabs.cpp` (load-time code with the tween lambdas apart from the per-frame code), which took ≈ 0.1 s off the
  `room.cpp` edit. The `.3dsx` edit sits at constitution III's 5 s line on this machine, as `examples/entities` does.

## Mutation testing

109 hand mutants (`build/ref-021/mutants.py`, applied one at a time by `build/ref-021/mutate.py`: rebuild `nnh_tests`,
run all of it, restore the file) of the spawn (31: checks, kinds, overrides and their mask, both copy loops, extra kinds,
placement, the hook chain and its order, counters, the doomed library), the reservation and the kind lookups (12), the
shared bookkeeping helpers (3), flattening (13), registration (13), `find` and the hash (5), queries (4), `record_of`,
the type cache, the layout and creation (10), map objects (10), the header's templates (4) and the platformer's
prefabs (4): **99/109 killed** on the first run (pc-debug).

- **Pre-empted before the run** (two tests written from reading the code, not from a survivor):
  `an_added_kind_replaces_its_slots_value_and_balances` (an override-only kind's slot `T{}` destroyed before the copy)
  and "misuse is not a miss" in `prefab_misuse_list_asserts_or_is_harmless`.
- **Test gaps, closed** (6): the null-id guard of `world_kinds_of` (a world of fewer than four kinds has null spare
  ids), the entry and value-byte capacities off by one entry or four bytes (the old test hit the byte limit first), an
  alignment check with a valid copy function (the old test's value also lacked one), the destructor's own doomed check
  (the test destroyed the library by move-assignment, which checks first), and the map's unknown counter before the
  assert. New tests: `world_kinds_of_maps_a_strided_list_of_ids` (extended),
  `add_refuses_one_entry_or_a_few_bytes_past_a_capacity`, `add_refuses_a_value_with_a_bad_alignment`,
  `deleting_a_library_from_its_own_hook_asserts_or_is_freed_by_the_spawn`, and a counter check in
  `an_unknown_explicit_prefab_is_a_logged_level_error_that_asserts`. All six killed in round 2.
- **Release-path** (2): the doomed block never freed (`sp1`), and a non-string `prefab` treated as an empty name
  (`ma8`): only observable where asserts are compiled out; both killed in pc-release (`mutants_rel.py`).
- **Equivalent** (2): `check_values` without its "more than 32 values" check (flattening refuses the 33rd entry before
  any array is indexed); a map object with an empty type looked up instead of skipped (no name hashes to FNV-1a's empty
  value in practice: only a cache lookup more).

Final: **107/109 killed**, 2 equivalent; every source restored (`git status` clean for `engine/` and `examples/`
after each round).

## Stage 9: Linux checks (left to the lead)

Not run in this session (VM 120 is the lead's). On `nnhge-build`, with GCC 13, GCC 14 and clang 18 (ASan/UBSan):
`nnh_tests` and ctest pass, and these tests hold their values unchanged (they are assertions):
`the_prefabs_self_check_reaches_the_golden_checksum` (`0x7BB7901E`; checkpoints `3EBCC2FD`, `821C8B95`, `CC194CA2`,
`C8360424`) and `prefab_spawns_equal_the_hand_written_calls_over_the_whole_script`, the twin-world tests
(`test_spawn`, `test_overrides`, `test_spawn_hooks`, `test_world_reserve`'s twenty seeded sequences), spec 020's
`the_entities_self_check_reaches_the_golden_checksum` (`0xC8FB9A41`) and ctest `platformer_replay_matches_script`
(`B2418A28`), `prefabs_self_check_pc` and `prefabs_check_script_pc` (`PASS 7BB7901E`). ASan/UBSan matter most for the
`Counted` balance tests (`test_add`, `test_variants`' shared values, `test_overrides`' added kind), the library
destroyed from its own nested hook (`test_prefab_misuse`, Release path under the Release-flavoured build), hooks that
destroy their own copy (`test_spawn_hooks`), and `test_data_values` (a 4 KB value block of `Enemy` structs). libFuzzer:
`fuzz_prefab_ops` (`-DNNH_BUILD_FUZZERS=ON`, clang), 10 minutes; any byte string is valid (every 4 bytes are one
self-check operation, the prefab side and the hand side compared after each).

## Stage 10 checks (T10.1, on the PC)

`scripts/dkp.sh cmake --build --preset 3ds-release` and `--preset 3ds-dev` (all targets: `prefabs`, `platformer` and
`entities` included) build without a warning. 3ds-release `hello.elf` 201 012 / 28 352 / 106 680 and 3ds-dev
`hello.elf` 217 728 / 32 872 / 109 288, as in stage 0. The final 3ds-dev objects (`prefab_spawn.o`, `library_find.o`,
`world_reserve.o`, `world_spawn.o`, `world_components.o`) disassemble identically to T7.6's: its table still holds.
The console run itself (T10.2) is the maintainer's: commands and expected lines in "Stage 10: console run (prepared)".

## Console run 1 (T10.2 run 1, Old 3DS XL, 2026-11-23)

Development build, `examples/prefabs/tests/check.nnhs` (log `build/logs/prefabs-20261123-221302.txt`). Every logic line
equals the PC's: `prefabs: PASS 7BB7901E`, the map, `selected:`, `spawned:` and `missed:` lines. Bench:

| Workload | Old 3DS avg (worst) | T7.6 estimate | Target (provisional, AC-7) | Verdict |
|----------|---------------------|---------------|----------------------------|---------|
| spawn (256 + destroys) | 1.621 ms (1.839); hand 1.498, ratio 1.08 | 1.3–1.4 ms, ratio ≈ 0.9 | < 2.0 ms, ≤ 1.25 | PASS |
| overrides | 1.918 ms (2.828) | 1.45–1.6 ms | < 2.2 ms | PASS |
| map (512 objects) | 5.572 ms (7.815) | 2.0–2.6 ms | < 4.0 ms | **FAIL** |
| find (1000, 9-character names) | 0.669 ms (0.749) | 0.40–0.48 ms | < 0.5 ms | **FAIL** |

Policy (the maintainer's): optimise first (T10.3, below), then set the budgets from the next console measurement with
≈ 25 % margin. The targets stay as written until then.

## Optimisation round (T10.3)

**Why `map` was ≈ 2× the estimate.** T7.6's ≈ 540 instructions per object were an undercount, then the usual model
error on top:

- The bench's objects **alternate** `coin` and `walker`. The one-entry type cache (`TypeCache`, keyed by the last
  type's pointer) therefore missed on every object: a full `find()` per object (≈ 71 instructions for 4–6 characters),
  not "≈ 5 for a hit".
- `Tilemap::object(i)` is 67 instructions, not ≈ 50: the 40-byte `MapObject` is built on its own stack, then copied
  to the caller (two 48-byte `ldm`/`stm` runs and the float `vldr`/`vstr`), plus the call.
- `properties.type("prefab")` is three calls (`type` → `find` → `find_property`) and a `strcmp` per property (≈ 53).
- The hook chain (`run_hooks`, `SmallFunction`'s trampoline) and the hook body (`get<Coin>`, `get_int`'s three calls
  and a `strcmp` on the hit) were ≈ 120, not ≈ 75.
- The FNV-1a hash costs ≈ 11 cycles per byte, not ≈ 5: GCC expands `× 0x01000193` into five dependent shift-adds, and
  on the ARM11 a shifted operand is needed a cycle early, so each add waits a cycle (find: 9 × 8 instructions on a
  ≈ 100-cycle dependency chain; measured 179 cycles per find, the model now gives ≈ 170–185).
- No prefetch on the map's object array (36 B per object) and property array, walked in order.

So ≈ 830 instructions per object, × ≈ 2.2 cycles (the spawn line's calibration, misses included) ≈ 1 830 cycles, plus
the map arrays' misses (≈ 1.5 lines per object) ≈ 2 000–2 200 cycles: 3.8–4.2 ms predicted against 5.57 measured. The
rest (≈ 1.3×) is the model error the spawn line shows too (1.62 ms against 1.3–1.4): out-of-line calls, two indirect
calls per entry, return-stack overflow on the hook path (map → spawn → hook → `get_int` → `strcmp` is 6 deep), and
branches that alternate with the object type.

**Changes** (no result changes; every golden holds):

- **`spawn_map_objects` reads the map in place** (`prefab_map.cpp` includes `tilemap/tilemap_internal.hpp`, the
  tilemap's internal header; still the only prefab source that sees the tilemap): object and property entries straight
  from the block, the `MapObject` built (by the new `detail::map_object_at`, which `Tilemap::object()` now uses too)
  only for an object that spawns or is a level error, `pld` four objects ahead (two lines) and eight property entries
  ahead.
- **Which property names are `prefab`** is memoised per call by string offset (16 direct-mapped entries; a map's pool
  only grows, so an offset's text never changes): no `strcmp` per property after the first sight of a name.
- **`NameMemo`** replaces `TypeCache`: 16 entries direct-mapped by the string's pointer, for types and explicit names,
  forgotten when the library grows (a hook may register a prefab mid-walk; the one-entry cache could not go stale in
  practice, a larger memo could). Equal text at two pointers is only a second lookup.
- **The run-time hash uses `mul`**: the prime goes through an empty `asm` so GCC can't see it (`fnv_prime()` in
  `library_find.cpp`); 5 instructions per byte (`eor`, `mul`, the next byte's `ldrb`/`cmp`/`bne` under the multiply)
  instead of 8, and ≈ 4–5 cycles of chain instead of ≈ 11. `find(text, size)` uses the same loop (`hash_bytes`).
- **Tilemap property lookups** (`find_property`) are inline in `map_property_table.hpp` and compare the first byte
  before calling `strcmp` on the rest: a getter is one call less and most non-matching names cost no `strcmp`. Every
  game's `get_int`/`get_string` benefits; behaviour identical.
- **`world_reserve`** asks for the record two slots ahead (`pld`, integer address arithmetic: past the block is fine
  for a hint). One instruction; spawns from a level or a burst take ascending slots, and a new record is read before it
  is written.
- Plan risk 1's `{index, hash >> 16}` table pairs were **not** applied: every find of the bench (and of a game, which
  looks up registered names) is a hit, and a hit must still confirm the full hash in the hash array, so the pair saves
  nothing there; the hash chain dominated. An unrolled or word-wise hash was not applied either: with `mul`, the
  eor → mul chain (≈ 4–5 cycles per byte) is the bound, not the instruction count.

**Tests first** (`tests/prefabs/test_map_names.cpp`, new; `test_names.cpp`): a walk over 120 objects of 24 types (half
registered), rotating property orders and explicit names answers object by object what `type("prefab")`,
`get_string("prefab")` and `find()` answer (summary, spawn order and prefab per spawn); a non-string or unknown `prefab`
among other properties is unknown; a prefab a hook registers mid-walk names the later objects of its type and the
later explicit names; the run-time hashes equal the reference FNV-1a for every length 0..79, start alignments 0..3 and
bytes 1..255 (`the_run_time_hashes_match_fnv1a_for_every_length_alignment_and_byte`; it failed to compile first: no
`hash_bytes` yet). They pass on the old code too (they pin the behaviour the memos must keep); hand mutants of the new code
(the memo not forgotten when the library grows, `strncmp(…, 4)` for `prefab`) are killed. `the_type_cache_…` became
`the_name_memo_answers_equal_text_at_different_pointers`.

**ARM evidence** (3ds-dev objects, `build/ref-021/opt/before` → `after3`, common paths counted from the listings):

| Hot path | Before | After |
|----------|--------|-------|
| `find(const char*)`, 9-character name | 103 instructions (8 per byte) | 76 (5 per byte) |
| hash chain per byte (cycles) | ≈ 11 (eor + 5 shifted adds) | ≈ 4–5 (eor + `mul`) |
| bench loop per find | 9 | 9 |
| map walk per spawned object, outside the spawn | ≈ 260 (`object()` 67, `type("prefab")` ≈ 53, `find` of the alternating type ≈ 71, loop and source ≈ 70) | ≈ 140 (name memo ≈ 16, type memo ≈ 20, `MapObject` + source ≈ 55, 3 `pld`) |
| hook's `get_int("value")` | ≈ 77 (3 calls, `strcmp`) | ≈ 70 (2 calls, first byte inline, `strcmp` of the rest) |
| `prefab_spawn` | — | identical |
| `world_reserve` | — | + 1 (`pld`) |
| map, per object in all | ≈ 830 | ≈ 700 |

`world_spawn.o`, `world_components.o` (spec 020's `spawn()` 109, `world_add` 143), `prefab_spawn.o` and both
examples' `bench.o` disassemble identically to before.

**Expected at the next console run** (honest ranges; the last map estimate was 2× low for the reasons above, so these
are scaled from the measurement rather than built up from counts): `find` ≈ 0.41–0.47 ms (from 0.669: 9 × ≈ 6 fewer
cycles per byte, ≈ 120 cycles per find); `map` ≈ 4.0–4.6 ms (from 5.57: ≈ 15 % fewer instructions, the alternating
find, the hash chain and the map arrays' misses gone) — likely still at or just over the provisional 4 ms; `spawn`
and `overrides` ≈ unchanged (the record `pld` may take a few hundredths off). The next candidates if `map` must go
lower: plan risk 3's per-prefab kind cache (≈ 35 instructions per spawn), `SpawnInfo` built once in the source
instead of copied per hook (≈ 15), then the spawn path itself, which is now ≈ 55 % of a map object.

**PC** (pc-dev, x86, noisy; not representative — x86 has hardware prefetchers and a fast multiplier): before spawn
0.045–0.048 ms (ratio 1.08–1.11), overrides 0.048–0.058, map 0.088–0.111, find 0.024–0.027; after spawn 0.048–0.055
(ratio 1.06–1.24, run-to-run noise), overrides 0.061–0.064, map 0.096–0.106, find 0.025–0.032. Before the round the
map walk alone (`check_map_objects` over the bench's map) took 0.022 ms of the 0.1: on x86 the spawns dominate.

**Checks**: `nnh_tests` 2316 (pc-debug); ctest pc-debug 268/268 and pc-release 258/258; 3ds-release and 3ds-dev
without a warning; `compare.sh` every reference identical; `--pc` scripts: `prefabs/check.nnhs` PASS
(`prefabs: PASS 7BB7901E`), `entities/check.nnhs` PASS (`C8FB9A41`), `platformer/level.nnhs` PASS
(`sum B2418A28: 60 Hz PASS`); `docs_tool.py check` 0 broken links.

## Console run 2 and budgets set (2026-11-23)

Old 3DS, Development, after the optimisation round. The first attempt stopped at line 51 (`the input for frame 107
reached the console on frame 108`): an input right after an `expect` had a one-frame lead, too short on the console
some of the time. A `wait 3` now follows every `expect` that an input follows (10 places; the script has 124 lines).
The rerun: every logic line and `prefabs: PASS 7BB7901E` as on the PC; bench spawn 1.626 ms avg (worst 1.880, hand
1.520, ratio 1.07), overrides 1.811 (2.389), map 3.562 (4.580), find 0.500 (0.581); `nnh: PASS (124 lines)`. The round
did better than its estimate for map (−36 %, estimated −17 to −28 %) and about as estimated for find (−25 %).

**Budgets** (bench policy: optimise, then set from the measurement with ~25 % margin; `bench_scene.cpp`, spec AC-7):
spawn < 2 ms and ≤ 1.25× (unchanged), overrides < 2.3 ms, map < 4.5 ms, find < 0.65 ms. The run with them: spawn
1.620, overrides 1.807, map 3.576, find 0.500 ms, `prefabs bench: PASS`, `nnh: PASS (124 lines)`. The target in the
log line is now printed with `%g` (it printed 0.65 as `0.7`).

Also on the Old 3DS (after stage 11, before the round, whose changes don't touch their paths): platformer
`level.nnhs` PASS (replay lines unchanged, `sum B2418A28: 60 Hz PASS`, worst 18.6 ms, missed 0), `gate.nnhs` PASS,
entities `check.nnhs` PASS (`C8FB9A41`; bench each 0.474, find 0.144, get 4.776, storage get 4.198, pool 0.441 ms:
spec 020's third run within 0.002 ms). The script runs now end with the console back in the Homebrew Launcher
(`nnh run --script` sends `Bye{quit}` on the console too, spec 013 plan).

## Stage 9 results (T9.1, VM 120, 2026-11-23)

GCC 13, GCC 14 and clang 18 with ASan/UBSan: no warning, every ctest passed but `prefabs_check_script_pc` (in all
three), which is the build folder's name and not the code: `nnh` takes `NNH_PRESET` only when it names a `pc-…` preset,
so in the VM's `build/g13`-style folders it looked for `build/pc-dev` (the same reason `nnh_script_pc_smoke` is left out
there; on Windows pc-debug it passes). `nnh_tests` under ASan with leaks on: 2312 tests, 0 failures; `nnh_assetc_tests`
265, 0 failures. `fuzz_prefab_ops` 600 s: 364 243 runs, cov 1176, no finding.

## Stage 11 results

- Docs: `docs/api/prefabs.md` (new); `docs/api/entities.md` (Worlds → prefabs), `docs/api/data.md` (see also: one
  prefab per data asset), `docs/api/tilemap.md` (objects name prefabs), `docs/guides/levels.md` ("Placing prefabs in
  Tiled"), `docs/architecture/patterns.md` (Prototype row → the API page), `docs/architecture/decisions.md` (D36
  implemented, with the refinements), `docs/REFERENCES.md` (Unity, Godot, flecs, Bevy, Tiled, Prototype), `CLAUDE.md`
  (module paragraph, the prefabs script command), `docs/ROADMAP.md` (Prefabs: implemented, console check pending),
  `specs/README.md`, the `nnh-architecture` skill's pattern list; spec, plan and tasks marked implemented (console
  check pending). `docs_tool.py check`: 156 files, 0 broken links. The Old 3DS numbers go into `docs/api/prefabs.md`
  ("Costs") after T10.2.
- Final: `nnh_tests` 2312 (pc-debug); ctest pc-debug 268/268, pc-release 258/258; 3ds-release and 3ds-dev without a
  warning; goldens `C8FB9A41` (entities), `B2418A28` (platformer replay), `7BB7901E` (prefabs); `compare.sh` every
  reference identical; every example script `--pc` PASS.
