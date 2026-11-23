# Plan 020: Entities, tags and object pools

Status: approved (2026-11-23, by the maintainer)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decision D35 ([decisions.md](../../docs/architecture/decisions.md))
Uses spec 002 (asserts, logs, `Result`, tagged memory, profiler), spec 001 (scenes; the bus for the game's own
events), spec 015 (D30 scene attachments, `TweenSet`, `Setter`), spec 016 (`SpriteAnimator` and its inline
callback), spec 019 (`collision::box_view`, `boxes_at`, `raycast`; the platformer's coins, turrets and two-step
adoption), spec 013 (scripts), spec 014/016 (`detail::fnv1a32`)

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | Portable core only, integer state only (no float in the module); type ids are keys, never ordered; kinds ordered by registration; golden checksum on every compiler and on the console (AC-2, AC-6) |
| II 3DS constraint | ⚠️→✅ | Pressure point: AC-7's five targets predate any measurement and the last two specs needed console optimisation rounds. Answered by an ARM11 cost plan (below): hot paths inline in the game's TU, no per-element call across TUs, no VFP in the module, no struct copies through the stack, a disassembly check before the first console run (T7.6). Two targets (`get<T>`, pool pairs) are estimated over or at their limit; AC-7's rule applies (optimise, then budget from the measurement with margin). Memory: one block per world, ≈ 63 KB for 1024 entities × 8 kinds × 4 tags besides the components |
| III Build speed | ⚠️→✅ | Pressure point: templates (`get<T>`, `each<A, B>`) in a header games include. Two light headers (no standard containers, no `<functional>`, no `<tuple>`), the pool header without the world's; templates are thin typed wrappers over integer code; `each` is one fold expression. Measured before/after (T0.1, T11.4) against `events.hpp`/`tween.hpp` |
| IV Event-driven | ✅ | Nothing published by the engine (FR-19); state is queried; the game publishes its own facts from hooks |
| V No hidden costs | ✅ | No exceptions (`Result` at creation), no RTTI (`detail::type_id<T>()`), no `virtual`, no allocation after creation; plain function pointers per kind (construct, destroy) and an inline `SmallFunction` hook, called once per component created or removed |
| VI Spec + test first | ✅ | Pure logic first on the fake platform; a reference model checks the world operation by operation over seeded sequences; the platformer swaps first with unchanged golden lines (stage 6) |
| VII Small files | ✅ | Two modules (`pool`, `entities`), one public header each; ≈ 20 source files of one concept each, all planned ≤ 200 lines |
| VIII Documented API | ✅ | `docs/api/pool.md`, `docs/api/entities.md`, header comments, `examples/entities`, the platformer, the scenes and patterns pages, D35 |
| IX Clean and legal | ✅ | Own code from published designs (EnTT's pointer-stable sparse sets, Bitsquid's index + generation handles, *Game Programming Patterns*); cited in `docs/REFERENCES.md`, nothing copied |
| X Safe by default | ✅ | Stale handles detected (FR-2), misuse asserts in Debug/Development and is harmless in Release (FR-23); no parser (tags from files are only hashed) |
| XI Opt-in, never in the way | ⚠️→✅ | Pressure point: D11 called ECS "later, as a module". It arrives as exactly that optional module (D35): events and scenes stay how modules talk, `hello` links nothing new, collision/tweens/animation don't include it (FR-20, FR-25) |

## Design

### Overview

```
game structs (its own)            <nnh/pool.hpp>                         <nnh/entities.hpp>
  Bullet, Body, Velocity, ...      Pool<Bullet, 32>  (inline storage)     World (one block: entity records, kinds)
                                   Pool<Bullet>::create(n) (one block)      kind k: T[cap + 1], owner u16[cap], 4 bitmaps
                                   Handle<Bullet>: slot 16 | gen 16        entity record: meta, kinds mask, tags, slot per kind
                                                                           Entity = Handle<World>
  shared (public detail headers):  detail/handle.hpp, detail/slot_bits.hpp (lowest free, quarantine, fresh)
  scene binding (D30):             BeforeUpdate → flush()     AfterExit → clear()   (no new notification)
  interop by layout:               world.storage<Body>() → data(), extent(), stride() → collision::box_view(...)
```

### Decision D35: stable slots per kind, 32-bit generational handles, quarantine until flush

**Storage model (spec option (d), with (c) on its own).** A pool is a fixed array of `T` plus three bitmaps; a world is
one block holding an **entity record** per entity and, per component kind, a fixed array of `T`, an owner per slot and
four bitmaps. Nothing ever moves: a component's address is fixed from `add` to `remove`. A free or quarantined slot
holds a constructed `T{}` (constructed at creation, re-constructed after each removal), so raw views (box views) see
only `T{}` there.

**Handles.** `nnh::Handle<T>` is one `std::uint32_t`: bits 0–15 the slot, bits 16–31 the generation. 0 is null.
Each slot keeps a **meta word**: its current generation in bits 0–15 and a live flag in bit 16 (and, for entities, a
dying flag in bit 17). A handle is live exactly when `meta[slot] == (generation | kLive)` — one load and one compare,
which also refuses a forged handle naming a free slot with its next generation. Generations start at 1 and step
`65535 → 1` on every release/destroy.

**Quarantine.** Releasing makes the handle stale at once and moves the slot from the live bitmap to the quarantine
bitmap; the lowest-free search reads `~(live | quarantine)`, so a quarantined slot is not reused until `flush()`
clears the quarantine bitmap (only the words below the extent, only for kinds marked dirty).

**Fresh slots.** While an iteration runs (`iterating_ > 0`), a slot taken by `acquire`/`spawn`/`add` also gets a bit
in a **fresh** bitmap and a world/pool flag `any_fresh_` is set; iterations skip fresh slots (one branch on the flag
when nothing was added) and the outermost iteration's end clears the fresh words it dirtied. This is what makes
"created during an iteration is not visited" hold for newcomers landing in a hole above the cursor.

**The world block** (one `mem::alloc`, tag `entities` or the game's; every array aligned to its type; computed by a
pure layout function shared by `create` and the tests):

| Part | Bytes | Notes |
|------|-------|-------|
| `detail::WorldHot` header | ≈ 64 | what inline code reads: record base and stride, entity extent, kind count, `ids` and `kinds` pointers, iteration depth, flags |
| kind table | `n × ≈ 48` (3DS) | `KindHot` (data, owner, bitmaps, size, capacity, extent, live, peak, misses) + `KindCold` (construct, destroy, hook `SmallFunction`, record offset of its slot field) |
| type ids | `n × 4` | contiguous, scanned by `get<T>` (≤ 32) |
| entity records | `E × (8 + 4·tags + 2·n)`, rounded to 4 | `{meta u32, kinds u32 (bit k = has kind k), tags[tags_per_entity] u32, slot[n] u16 (0xFFFF none)}` |
| entity bitmaps | `3 × ⌈E/32⌉ × 4` | live, quarantine, fresh |
| destroy queue | `E × 2` | `next` link per entity (FR-13: hooks' destroys, FIFO) |
| per kind | `(cap + 1) × sizeof(T)` + `cap × 2` + `4 × ⌈cap/32⌉ × 4` | data (+1 scratch slot for the checked accessor in Release), owner (entity slot), live, quarantine, fresh, removing |

Entity-major records put an entity's meta, kinds, tags and component slots in one or two cache lines: `get<T>(e)` and
the second kind of `each<A, B>` touch one record.

**Rejected:** archetypes (move components, per-combination tables), packed sparse sets with swap-and-pop (moves
components, order depends on removal history), kind-major `u16 sparse[n][E]` arrays (two cache lines per `get`), a
FIFO free list (grows the extent, changes the platformer's bullet order), immediate slot reuse (a pointer held during
a frame could name a newcomer), 64-bit handles, bus events for lifecycle, a per-game component enum.

### Public API (sketch)

```cpp
// <nnh/detail/handle.hpp> (included by both public headers)
namespace nnh {
/// A 32-bit generational handle: slot (16 bits) and generation (16 bits); 0 = null. Trivially copyable: store it
/// in your structs and events. A handle says nothing about liveness by itself: ask its pool or world.
template <typename T>
class Handle {
public:
    constexpr Handle() = default;
    constexpr int slot() const { return static_cast<int>(bits_ & 0xFFFFu); }
    constexpr int generation() const { return static_cast<int>(bits_ >> 16); }
    constexpr std::uint32_t bits() const { return bits_; }
    static constexpr Handle from_bits(std::uint32_t bits) { Handle h; h.bits_ = bits; return h; }
    constexpr explicit operator bool() const { return bits_ != 0; } ///< not null (it may still be stale)
    constexpr bool operator==(const Handle&) const = default;
private:
    std::uint32_t bits_ = 0;
};
} // namespace nnh
```

```cpp
// <nnh/pool.hpp>: <cstddef>, <cstdint>, <new>, nnh/result.hpp, detail/handle.hpp, detail/slot_bits.hpp, detail/pool_core.hpp
namespace nnh {
class Scene;  struct MemTag;
inline constexpr int kMaxPoolCapacity = 65535;
inline constexpr int kRuntimeCapacity = 0;                    ///< Pool<T>: capacity chosen by create()
struct PoolStats { int live = 0, peak = 0, capacity = 0, misses = 0; };
template <typename T> struct Acquired {                       ///< `if (auto [h, b] = pool.acquire(); h) ...`
    Handle<T> handle{};  T* object = nullptr;
};

/// Up to N objects of T in the pool object itself (no allocation): a scene member. Not copyable, not movable.
template <typename T, int N = kRuntimeCapacity>
class Pool {
public:
    Pool();                     ///< unbound: call flush() once per frame, clear() to restart
    explicit Pool(Scene& scene); ///< bound (D30): flushed before the scene's update, cleared after its on_exit()
    ~Pool();                    ///< destroys every object (T's destructor), detaches from the scene
    Acquired<T> acquire();                        ///< lowest free slot (holds T{}); full: null handle + a miss
    Acquired<T> acquire(const T& value);  Acquired<T> acquire(T&& value);
    bool release(Handle<T> h);                    ///< false (nothing changes) for a stale or null handle
    T* get(Handle<T> h);  const T* get(Handle<T> h) const;   ///< null when stale or null
    bool alive(Handle<T> h) const;
    T& at(Handle<T> h);                           ///< checked: asserts; Release: a scratch T{} (never a live object)
    void flush();                                 ///< quarantined slots become free (asserts inside an iteration)
    void clear();                                 ///< release all (ascending), flush: next acquires start at slot 0
    void bind(Scene& scene);  bool bound() const;
    detail::PoolRange<T> items();                 ///< `for (auto [h, b] : pool.items())`, ascending slots
    template <typename F> void each(F&& fn);      ///< fn(Handle<T>, T&), ascending, inlined
    T* data();  int extent() const;  static constexpr int stride() { return sizeof(T); }
    bool is_live(int slot) const;  Handle<T> handle_at(int slot) const;  ///< null handle for a non-live slot
    int count() const;  int capacity() const;  PoolStats stats() const;
};
/// The same pool with its capacity chosen at run time: one block of tagged memory. Movable while unbound.
template <typename T>
class Pool<T, kRuntimeCapacity> {
public:
    Pool() = default;  ///< empty: every call asserts and answers nothing
    static Result<Pool> create(int capacity, MemTag* tag = nullptr); ///< 1..65535; tag default `pool`
    Pool(Pool&&) noexcept;  Pool& operator=(Pool&&) noexcept;  ///< asserts (and moves nothing) while bound
    // ... the members above, same behaviour
};
} // namespace nnh
```

```cpp
// <nnh/entities.hpp>: <cstddef>, <cstdint>, <type_traits>, <new>, nnh/result.hpp, detail/handle.hpp,
//                      detail/slot_bits.hpp, detail/type_id.hpp, detail/name_hash.hpp, detail/small_function.hpp,
//                      detail/world_layout.hpp — not <nnh/pool.hpp>
namespace nnh {
class Scene;  struct MemTag;  class World;
using Entity = Handle<World>;

// ---- Tags (FR-16) ----
enum class Tag : std::uint32_t { None = 0 };
constexpr Tag tag(const char* name) { return Tag{detail::fnv1a32(name)}; }   ///< `case nnh::tag("enemy"):`
constexpr Tag tag(const char* text, std::size_t size) { return Tag{detail::fnv1a32(text, size)}; }
/// True when every tag differs from the others and from Tag::None: `static_assert(nnh::tags_distinct(kEnemy, kCoin))`.
template <typename... Tags> constexpr bool tags_distinct(Tags... tags);

// ---- Component kinds (FR-10) ----
struct ComponentDesc { /* type id, size, align, capacity, construct, destroy, erased hook: filled by component<T> */ };
template <typename T> ComponentDesc component(int capacity);
/// With a remove hook: `nnh::component<Body>(256, [this](nnh::World& w, nnh::Entity e, Body& b) { ... })`
/// (captures ≤ 16 bytes, trivially copyable, as event handlers).
template <typename T, typename F> ComponentDesc component(int capacity, F&& on_remove);

struct WorldDesc {
    int max_entities = 0;      ///< 1..65535
    int tags_per_entity = 4;   ///< 0..8
    MemTag* memory_tag = nullptr; ///< default `entities`
};
struct WorldStats { int entities = 0, peak = 0, capacity = 0, spawn_misses = 0, add_misses = 0; };
struct KindStats { int live = 0, peak = 0, capacity = 0, misses = 0; };

/// A view of one kind's storage (FR-15): valid for the world's life.
template <typename T>
class Storage {
public:
    T* data();  int extent() const;  static constexpr int stride() { return sizeof(T); }
    bool is_live(int slot) const;  Entity owner(int slot) const;  ///< null for a non-live slot
    int slot_of(Entity e) const;   ///< -1 when e is stale or lacks T
    T* get(Entity e);              ///< as World::get<T>, with the kind lookup hoisted
    detail::StorageRange<T> items(); ///< `for (auto [e, body] : bodies.items())`, ascending slots
    int count() const;  int capacity() const;  KindStats stats() const;
};

class World {
public:
    World() = default;                         ///< empty: every call asserts and answers nothing
    template <int K>
    static Result<World> create(const WorldDesc& desc, const ComponentDesc (&kinds)[K]);
    static Result<World> create(const WorldDesc& desc, const ComponentDesc* kinds, int count);
    ~World();                                  ///< clear() (hooks run), frees the block
    World(World&&) noexcept;  World& operator=(World&&) noexcept; ///< assert (nothing moves) while bound
    void bind(Scene& scene);  bool bound() const;

    Entity spawn();                            ///< lowest free slot; full: null + asserts (Debug/Development)
    bool destroy(Entity e);                    ///< hooks, then components and tags gone; false when stale
    bool alive(Entity e) const;
    template <typename T> T* add(Entity e);                    ///< the slot's T{}; null when stale or full
    template <typename T> T* add(Entity e, const T& value);
    template <typename T> T* get(Entity e);                    ///< constant time; null when stale or absent
    template <typename T> const T* get(Entity e) const;
    template <typename T> bool has(Entity e) const;
    template <typename T> bool remove(Entity e);               ///< runs T's hook, then T{} and quarantine
    template <typename T> T& at(Entity e);                     ///< checked accessor (FR-2)
    template <typename T> Storage<T> storage();
    /// fn(Entity, A&, B&, ...) for every live entity with all kinds, ascending slots of A's storage (inlined).
    template <typename A, typename... B, typename F> void each(F&& fn);
    detail::EntityRange entities();            ///< every live entity, ascending slots (the platformer's hash)

    bool add_tag(Entity e, Tag t);  bool remove_tag(Entity e, Tag t);  bool has_tag(Entity e, Tag t) const;
    int find(Tag t, Entity* out, int capacity) const;  ///< ascending; returns the total; null + 0 counts
    Entity first(Tag t) const;
    template <typename F> void each_tagged(Tag t, F&& fn);    ///< fn(Entity), FR-15's safety rules

    void flush();  void clear();  bool clearing() const;     ///< clearing(): inside clear() (hooks may skip effects)
    WorldStats stats() const;  int count() const;  int capacity() const;
};
} // namespace nnh
```

Exact names may still change while building; stage notes in this file record any change.

### Events introduced

None (FR-19, constitution IV). The documentation shows a game publishing `EnemyDefeated{Entity who, int score}` from
a remove hook and checking `world.alive(e.who)` on delivery.

### Semantics fixed by the plan

**Pools.** `acquire()` takes the lowest slot with `live | quarantine` clear (a hint word skips full words), sets the
live bit and `meta = gen | kLive`, raises the extent, returns `{handle(slot, gen), &slot}`; the slot already holds
`T{}`. `acquire(value)` destroys that `T{}` and copy/move-constructs `value` in place. A full pool returns
`{null, nullptr}` and counts a miss. `release(h)`: stale → false; else `~T()`, `new T{}`, live bit → quarantine bit,
`meta = next(gen)` (not live). `flush()` clears quarantine words below the extent. `clear()` releases every live slot
in ascending order, flushes, and sets the extent to 0. Stats: live, peak, misses (reset by `clear`).

**World operations.**

- `spawn()`: during `clear()` → null without assert (nothing may survive a clear, see the D30 proof); otherwise the
  lowest free entity slot, record zeroed (kinds 0, tags 0, slots 0xFFFF), live. Full → null, a miss, an assert.
- `add<T>(e)`: kind of `T` (scan of the type ids; unregistered → assert, null); `e` live (meta compare) and not dying
  (assert, null); already has `T` → assert, the existing component; kind full → assert, a miss, null; else the
  lowest free slot of the kind: live (+ fresh while iterating), owner = e's slot, record slot = it, kinds bit set.
  The slot holds `T{}`; `add(e, value)` destroys it and copy-constructs `value`.
- `get<T>(e)`: meta compare, record slot ≠ 0xFFFF, `static_cast<T*>(data) + slot`. Inline, no call.
- `remove<T>(e)`: stale, absent, a dying entity (its components go in step 2 of its destroy) or a component whose
  hook is running (its `removing` bit) → false. Else: `removing` bit, hook depth +1, the kind's hook
  `(world, e, component)` (the component is still attached and readable), depth −1, then detach (`T{}`, live →
  quarantine, record slot 0xFFFF, kinds bit cleared, `removing` cleared), then drain the destroy queue if depth is 0.
- `destroy(e)`: stale or dying → false. If a hook is running (depth > 0) → mark dying, append to the FIFO queue,
  return true (processed before the outermost operation returns; `alive(e)` stays true until then). Otherwise process
  `e` and then the queue: **step 1** dying flag; for each kind bit of `e` in registration order (the mask re-read
  each step), its hook runs with the component attached; **step 2** every remaining component detached (`T{}`,
  quarantine), tags zeroed, `meta = next(gen)` (stale), live → quarantine, live count −1. No recursion: hooks' destroys
  only enqueue; each entity is enqueued at most once (dying flag).
- `flush()`: inside an iteration → assert, ignored. Quarantine words of the entity table and of every dirty kind below
  their extents cleared.
- `clear()`: `clearing = true`; destroy every live entity in ascending slot order (queue drained after each); flush;
  extents 0; `clearing = false`; stats' misses kept (they are since creation).

**Iteration.** `each<A, B...>(fn)`: `iterating_ + 1`; for each 32-bit word of A's live bitmap below A's extent
(snapshotted at the start: slots above it are newcomers), take `bits = live & ~fresh` (the `& ~fresh` only when
`any_fresh_`), then lowest bit first (`31 − clz(bits & −bits)`): `e = owner[slot]`, record `r`; for each `B`:
`s = r.slot[kB]`, skip on 0xFFFF or a fresh bit; call `fn(Entity{slot(e), gen(r.meta)}, A[slot], B[s]...)`; then
`bits &= live[word]` reloaded (anything the callback destroyed or removed is not visited). `−1` at the end; the
outermost end clears dirty fresh words. `entities()`, `items()`, `each_tagged` use the same loop over the entity or
kind bitmaps; range objects hold the iteration guard (constructor +1, destructor −1), so `break` is safe. A
callback that calls `flush()` asserts.

**Tags.** A record holds `tags_per_entity` words, 0 = empty. `add_tag(e, t)`: stale → false; `t == Tag::None` →
assert, false; already there → false; full → assert, false. `remove_tag` writes 0. A destroyed entity's tags are
zeroed in step 2, so `find` scans the records below the entity extent without a liveness test (dead and free records
hold no tag) and counts every match; it writes the first `capacity` into `out`. `first` stops at the first match.
`each_tagged` iterates live, non-fresh entities and tests their tag words.

**Checked accessors.** `at(h)` on a stale handle: assert in Debug/Development; Release re-constructs the kind's (or
pool's) scratch slot `T{}` (slot `capacity`, never live, outside every extent) and returns it.

**Misuse (FR-23)** — assert in Debug/Development, Release answers nothing: unregistered kind; null buffer with
capacity > 0; negative capacity; `flush()` inside an iteration; moving a bound pool or world (nothing moves); using an
empty or moved-from world/pool; `at` on a stale handle; adding to a dying entity; `add_tag(Tag::None)`; `bind` twice
or while iterating/clearing; destroying a world or run-time pool from inside its own iteration or hook (the block is
freed by the outermost operation, as `TweenSet` does). Creation errors (`InvalidArgument`: capacities, tag count, 0 or
more than 32 kinds, a kind listed twice, a kind capacity above `max_entities`; `OutOfMemory`) are `Result`s.
"Destroyed while bound" (FR-23) is read as: the scene destroyed with the object still attached (the `Scene`
destructor's check, AC-3) — destroying a bound world or pool detaches it (its destructor), as a `TweenSet` does,
which is how a scene member ends.

### Scene binding and the D30 proof (FR-18)

`bind(scene)` attaches a `detail::SceneAttachment` kept in the block (world, run-time pool) or in node bytes inside the
pool object (inline pool: `alignas(void*) unsigned char node_[3 * sizeof(void*)]`, checked by a `static_assert` in
`pool_scene.cpp` against the internal struct, constructed there). Its notify function: `BeforeUpdate` → `flush()`;
`AfterExit` → `clear()`. Nothing else in D30 changes.

**Claim: a bound world's clear at scene exit, hooks included, cannot call into a scene that has left.**

1. *When.* `AfterExit` is sent only from `SceneStack::do_pop()`, right after `scene->on_exit()` and before
   `do_pop()` returns (`scene_stack.cpp`). For `replace`, `do_push(next)` runs after `do_pop()` returns; nothing else
   (no frame boundary, no other scene's `on_enter`/`update`, no event dispatch) runs in between. So the hooks run as the
   last step of that scene's stay — exactly where code at the end of its `on_exit()` would run.
2. *Alive.* The scene object exists during the pass: the game owns it, and `~Scene()` requires `!on_stack_ &&
   attachments_ == nullptr`. A world that is a member is destroyed (and detached) before the `Scene` base destructor
   runs, so no notification can reach a destroyed scene; a world that outlives its scene trips that check (AC-3).
3. *Complete.* After the pass the world holds nothing: `clear()` destroys every entity and, because `spawn()` refuses
   while `clearing()`, no hook can leave a newcomer behind; `add` to a dying entity is refused; the flush frees every
   slot. The engine queues nothing (FR-19), so no later delivery carries the world's data, and no hook can run again
   until the scene is entered again and the game spawns.
4. *No re-entry into the stack.* The pass runs with `g_scene_pass.running`; a hook that calls `scenes::push/pop/
   replace` only queues (applied in the next round of `apply_pending()`, after this pop); a hook that called
   `scenes::update_and_draw()` would assert ("notified from inside a notification"); attaching during the pass waits
   for the next pass (epoch) and detaching moves the cursor (`scene_attachments.cpp`), so a hook that destroys a bound
   tween set or pool is safe.
5. *Other attachments.* Nodes run in attach order. A world bound before the scene's `TweenSet` runs its hooks first
   (they may kill tweens; the set is then cleared silently); bound after, its hooks see a cleared set where `kill` on
   a stale handle does nothing. The one game-side hazard — a hook **starting** a tween in a set already cleared in this
   pass, which would survive into the next stay — is closed by the documented rule "bind the world before the tween
   sets its hooks use" and by `clearing()`: effect hooks start with `if (w.clearing()) return;` (the example and the
   platformer do both, and AC-3's test checks the order).

`BeforeUpdate` runs before the top scene's `update()`, outside any game iteration, so its flush never meets the
"flush inside an iteration" assert; a pushed pause scene stops it (the notify goes to the top scene only).

### Generation wrap (FR-2)

A slot's generation steps once per release. Because a released slot stays quarantined until the next flush, one slot
is released at most once per flush; with one flush per frame (any bound pool or world), a stale handle reads as live
again only if it was kept while **its** slot was reused on 65 535 consecutive flushes: ≥ 65 535 frames = 18 min 12 s
at 60 Hz. A game that flushes an unbound pool several times per frame divides that. Documented with the 32-bit
rationale; tests cover the step from 65 535 to 1 and the exact wrap (a handle kept across 65 535 reuses of its slot
reads live: the documented limit, asserted as such so a change is noticed).

### Header and build cost (FR-25, III)

- Headers: `<nnh/pool.hpp>` and `<nnh/entities.hpp>` include only `<cstddef>`, `<cstdint>`, `<new>`,
  `<type_traits>` and engine detail headers already used by `events.hpp`/`tween.hpp` (`result.hpp`, `type_id.hpp`,
  `small_function.hpp`, `name_hash.hpp`) plus three small new ones (`handle.hpp`, `slot_bits.hpp`, `world_layout.hpp`
  or `pool_core.hpp`). No `<tuple>`, `<utility>` beyond what `result.hpp` already pulls, no `<functional>`, no
  containers. Neither header includes the other, `collision.hpp`, `tween.hpp`, `animation.hpp` or `gfx.hpp`.
- Templates: `get/has/add/remove/at/storage<T>` are 3–10-line wrappers (kind lookup + typed pointer arithmetic);
  `each<A, B...>` is one function template whose per-kind checks are a fold expression over a fixed-size array of
  record offsets — no recursion, no tuple, one instantiation per distinct kind list per TU. `component<T>` instantiates
  three tiny functions (construct, destroy, hook adapter) per kind, once per program.
- Budgets (measured in T0.1 and T11.4, 3 runs): `-fsyntax-only` of a one-line TU including `<nnh/entities.hpp>` no
  slower than one including `<nnh/tween.hpp>`; a TU with 10 distinct `each<...>` lists and 40 `get<T>` calls compiles
  within 0.3 s of the same TU without them (pc-debug); editing `examples/entities/arena.cpp` → simulator < 2 s,
  `.3dsx` < 5 s (constitution III).

### ARM11 cost plan (AC-7)

Calibration from spec 019's console rounds (Old 3DS, `-O2`, GCC 16): simple integer loops with loads ran at ≈ 2–3
cycles per instruction (`boxes_at`'s 11–17-instruction reject path: 0.118 µs = 31 cycles per box); every
`vcmp`+`vmrs`, out-of-line call, `memcpy`/`memset` call and struct copied through the stack was visible in the
numbers. 1 ms = 268 000 cycles. The rules this plan builds in from the start:

1. **No per-element call across translation units.** `get`, `alive`, `has`, `each`, the ranges and pool
   `acquire`/`release`/`get` are inline in the headers (bitmap scans in `slot_bits.hpp`). Out-of-line calls are per
   operation (`spawn`, `add`'s slot bookkeeping, `destroy`, `flush`, `find`), never per visited element; per-kind
   construct/destroy pointers and hooks run once per component created or removed.
2. **No VFP in the module.** Everything is integer; the only float work in a bench is the game's callback.
3. **No struct through the stack.** Handles are one `uint32_t` (passed and returned in `r0`); out-of-line functions
   return an `int` slot (≤ 4 bytes, AAPCS `r0`), and `Acquired`/pointers are composed inline. No out-of-line function
   returns `Acquired` or `ComponentDesc` on a hot path.
4. **No division, no `__aeabi_*`.** Capacities are rounded words (`>> 5`, `& 31`), record strides multiply (`mla`),
   typed data indexes by `sizeof(T)` known at compile time.
5. **No `memset`/`memcpy` calls per element.** Records are zeroed with word stores (≤ 8 + 2·32 + 4·8 bytes); `T{}`
   reset uses placement new (inline for pools, the per-kind function for worlds).
6. **Bit scans:** ARMv6K has `clz` but no `rbit`: lowest bit = `31 − clz(x & −x)` (3 instructions), checked in the
   disassembly.

Estimates (instructions on the hot path × ≈ 2.5 cycles):

| AC-7 workload | Hot path | Estimate | Target | Verdict |
|---------------|----------|----------|--------|---------|
| `each<Body, Velocity>` × 1024 (both kinds on all), `pos += vel` | ≈ 25 instructions per entity (bit scan 4, owner + record 3, B slot + checks 4, handle 2, 2 `vldr`/`vadd`/`vstr` pairs 6, reload/and 2, loop 4) | ≈ 0.24 ms | < 0.5 ms | confirmed |
| 256 × (spawn, 3 `add`, 2 tags) + 256 `destroy` + flush | spawn ≈ 40, add ≈ 45 each, tag ≈ 20 each, destroy ≈ 140 (3 kinds: record walk, 3 resets through the kind's function, bitmaps, tags) → ≈ 355 per pair | ≈ 0.85 ms | < 1 ms | confirmed, little margin (risk 2) |
| `find` over 1024 entities × 4 tags | ≈ 8 instructions per record (`ldm` of 4 words, 4 conditional compares, stride add, loop) | ≈ 0.08 ms | < 0.1 ms | confirmed, little margin |
| 10 000 `get<T>` by handle | handle load, decode 2, bound 2, meta load + compare 3, kind scan (hoisted by GCC if it proves no store; else ≈ 3 per id), record slot load + none 3, address 2 → 15–25 instructions with three dependent loads | ≈ 1.0–1.9 ms | < 0.5 ms | **at risk** (risk 1) |
| 1000 pool acquire/release pairs + flush (`Pool<Item, 1024>`) | acquire ≈ 20, release ≈ 15 | ≈ 0.33 ms | < 0.3 ms | **at risk** (risk 1) |

As AC-7 says, the targets stay as written; T7.6 checks the disassembly against this table before the first console
run, the console measures (T10.2), and a missed target gets the risk-1 mitigations; if still missed, the budget is set
from the measurement with margin, written in the spec and the docs with the numbers.

### The platformer (AC-4)

**Step 1 — same play, unchanged golden lines.** Each word of a replay sample (`checksum_add`: player box x/y, camera
x/y, touching sides, coins collected, the turrets' state word) must be the same bits. Why each is:

- *The player and camera* don't read the world: the player still moves through the same `collision_grid(kPlayerMask)`.
- *Coins collected*: `collect` was one `boxes_overlapping` over the coin array with the player's mask; it becomes one
  over the body storage with the same mask. The player's body (`kPlayer`) and the turrets' (`kEnemy`) don't pass
  `kPlayerMask = kWorld | kCoin`; free and quarantined slots hold `Body{}` (no layers, no area). So the same coins
  overlap, and the count is order-free anyway. `CoinCollected` events go out in ascending body slot order = map
  object order among coins = the old index order (events aren't in the checksum; the HUD and sounds see the same).
- *The turrets' word* hashes `hits`, `flash`, each turret's `sees`/`cooldown` in turret order, then each active
  bullet's position bits and `stuck` in bullet slot order. Turrets: `each<Turret>` visits ascending `Turret` slots =
  spawn order = map object order among turrets = the old index order. Bullets: the old `shoot` took the first
  inactive slot; bullets are released only in `move_bullets` (stuck time over, or the player hit), which runs after
  `look` (where turrets fire) in the same update, and the pool is flushed at the start of the next update (before
  `look`). So every slot the old code could reuse is free again when the next shot is fired, and no slot it couldn't
  reuse is: the lowest free slot equals the first inactive one, every frame. A restart (`TurretBook::restart` set every
  bullet inactive) becomes `clear()`, after which the lowest free slot is 0 again.
- *Rays*: the bullets' `raycast(grid, bodies, ...)` gets a view of the whole body storage instead of a rebuilt
  player + turrets array; with `kBulletMask = kWorld | kPlayer` only the player's box passes, as before, and the slab
  test is per box, so the nearest hit and its fraction/point are the same bits; `hit.box` maps to the player through
  `owner(hit.box)`.
- *Restart* = `world.clear()` + spawns in the same order (player, then the map's coin and turret objects in object
  order), so slots equal the first load's.

If a byte differs anyway, the stage stops and plan.md says why before any golden line is touched (spec AC-4).

**Step 2 — sparkles, regenerated lines.** Taking a coin spawns 4 sparkle entities (decision 13). The sample gains
`entities` (live count) and `handles` (FNV-1a over the live handles' bits in `entities()` order); `checksum_add`
hashes them after the turrets' word, so **every** `sum` changes (bonus level included). The log line's x, y and coins
columns must not change (sparkles never touch the player): the independent check. The lines are regenerated with the
procedure at the top of `level.nnhs` (`PLATFORMER_DEMO=replay NNH_EXIT_AFTER_FRAMES=920 ...platformer.exe`) in the same
commit, whose message says why; the ctest passes again; the console must print the new PC lines (T10.2).

### Self-check and bench (AC-2, AC-6, AC-7)

- **Self-check** (`examples/entities/self_check.{hpp,cpp}`, pure): a world of 1024 entities (kinds `A` 16 B, `B`
  8 B with a remove hook that sometimes destroys another entity or spawns one, `C` 4 B; 4 tags per entity) and a
  run-time `Pool<Item>` of 256; 100 000 operations chosen by the engine's integer hash of a seed and the step (no
  `rand`): spawn, destroy (also inside `each` and `each_tagged`), add/remove of the three kinds, add/remove tag,
  `find` with a small buffer, `each<A>`, `each<B, A>`, `each<A, B, C>`, flush, rarely `clear`, pool
  acquire/release/flush/`items()`. FNV-1a over every returned handle's bits, every visit (handle bits in order),
  `find` totals and outputs, and stats; a checkpoint every 10 000 operations. Golden values recorded from the PC in
  `kEntitiesCheckGolden`; the unit test and ctest `entities_self_check_pc` hold them; the console logs
  `entities: <n> sum <checksum>` and `entities: PASS <checksum>` / `entities: FAIL ...`, 2000 operations per frame.
- **Bench** (`examples/entities/bench.{hpp,cpp}` pure workloads, `bench_scene` timing them with the profiler, 300
  frames each, average and worst): the five AC-7 workloads exactly (a world of 1024 entities with `Body`
  (`Rect` + layers) and `Velocity` on every one; the spawn workload on a separate world with 3 kinds and 2 tags; 1024
  entities × 4 tags for `find`; 10 000 seeded handles for `get<Body>`, also timed through `Storage<Body>::get` for
  information; `Pool<Item, 1024>` for the pairs). Logs one line per workload, `entities bench: <name> avg X ms worst Y
  ms (target < Z ms) PASS|FAIL`, then one verdict line `entities bench: PASS` or `entities bench: FAIL (<names>)`
  after every number (spec 019's round-2 form, so one console run reports everything); `check.nnhs` has
  `fail-on "entities bench: FAIL"`.
- **The script** `examples/entities/tests/check.nnhs`: SELECT → every checkpoint line and the PASS line; back in the
  showcase, fixed touches with their expected `destroyed: slot n gen g tags ...` lines, X cycles with the expected
  `filter: enemy found N` lines, a fire hold past the pool's capacity (`pool: full, misses N`), the homing missile's
  `target lost (stale handle slot n gen g)` line, screenshots; then Y → the bench lines and the verdict.

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/detail/handle.hpp` | new | `Handle<T>` |
| `engine/core/include/nnh/detail/slot_bits.hpp` | new | inline bitmap ops: lowest free (with hint), take, release to quarantine, fresh, lowest-bit scan; `next_generation` |
| `engine/core/include/nnh/detail/pool_core.hpp` | new | `PoolCore` (standard layout: counts, extent, iteration depth, flags, stats, pointers, node bytes, a type-erased `reset_all` for the scene's clear), `PoolRange<T>` |
| `engine/core/include/nnh/pool.hpp` | new | the API above (≈ 220 lines with comments) |
| `engine/core/include/nnh/detail/world_layout.hpp` | new | `WorldHot`, `KindHot`, record field offsets, inline kind lookup and record access, `StorageRange`, `EntityRange` |
| `engine/core/include/nnh/entities.hpp` | new | the API above (≈ 300 lines with comments, as `camera.hpp`) |
| `engine/core/src/pool/pool_core.cpp` | new | init, flush, clear driver, stats, checked-accessor failure, misuse logs |
| `engine/core/src/pool/pool_runtime.cpp` | new | run-time form: block layout and `create`, move, destroy |
| `engine/core/src/pool/pool_scene.cpp` | new | `bind`, the notify function (flush / clear), node size check |
| `engine/core/src/pool/pool_internal.hpp` | new | shared internals (tag `pool`) |
| `engine/core/src/entities/block_layout.{hpp,cpp}` | new | pure byte layout of a world from its description (offsets, alignment, total) |
| `engine/core/src/entities/world_create.cpp` | new | validation (`Result` errors), block allocation, kind table, `T{}` construction |
| `engine/core/src/entities/world_lifetime.cpp` | new | destructor, move, empty/moved-from checks, busy (destroyed from its own hook) |
| `engine/core/src/entities/world_spawn.cpp` | new | `spawn`, entity record init, misses |
| `engine/core/src/entities/world_components.cpp` | new | `add` slot bookkeeping, `remove` (hook, detach), checked-accessor failure |
| `engine/core/src/entities/world_destroy.cpp` | new | `destroy`, dying flag, FIFO queue, hooks in kind order, step 2 |
| `engine/core/src/entities/world_flush.cpp` | new | `flush`, `clear`, `clearing`, end of the outermost iteration (fresh words) |
| `engine/core/src/entities/world_tags.cpp` | new | `add_tag`, `remove_tag`, `has_tag` |
| `engine/core/src/entities/tags_find.cpp` | new | `find`, `first`: record scans specialised for 1..8 tags per entity |
| `engine/core/src/entities/world_scene.cpp` | new | `bind`, notify (flush / clear) |
| `engine/core/src/entities/world_stats.cpp` | new | `stats()`, kind stats |
| `engine/core/src/entities/world_internal.hpp` | new | `WorldCold`, `KindCold`, block access, tag `entities` |
| `engine/core/include/nnh/scene.hpp` | changed (message only) | the destructor's check names "a bound TweenSet, World or Pool" (not in `hello`: it has no scene) |
| `engine/core/CMakeLists.txt` | changed | two source lists (`_nnh_pool_sources`, `_nnh_entities_sources`), headers |
| `tests/pool/test_handle.cpp`, `test_slot_bits.cpp`, `test_pool.cpp`, `test_pool_iteration.cpp`, `test_pool_runtime.cpp`, `test_pool_scene.cpp` | new | FR-1..FR-8, FR-18 for pools |
| `tests/entities/world_model.hpp`, `test_world_model.cpp` | new | a naive reference model (arrays and linear searches) compared operation by operation on seeded sequences |
| `tests/entities/test_world_create.cpp`, `test_block_layout.cpp`, `test_spawn.cpp`, `test_components.cpp`, `test_destroy_hooks.cpp`, `test_flush_clear.cpp`, `test_each.cpp`, `test_storage.cpp`, `test_tags.cpp`, `test_find.cpp`, `test_world_scene.cpp`, `test_world_misuse.cpp` | new | FR-9..FR-18, FR-23, FR-24 |
| `tests/entities/test_interop.cpp` | new | AC-3: box views, an animator component, tweens through a setter and a hook |
| `tests/entities_example/test_self_check.cpp` | new | AC-2 golden on `examples/entities/self_check.cpp` |
| `tests/fuzz/fuzz_world_ops.cpp` | new | bytes → operations against the model (mini-fuzz in `nnh_tests`; libFuzzer with `NNH_BUILD_FUZZERS`) |
| `tests/CMakeLists.txt` | changed | the tests, the example sources, ctest `entities_self_check_pc` |
| `examples/entities/` | new | `main.cpp`, `demos.hpp` (`ENTITIES_DEMO`), `components.hpp` (Body, Velocity, Bob, Coin, Missile, Explosion), `tags.hpp`, `arena.{hpp,cpp}` (world desc, spawns, movement, hooks), `arena_draw.{hpp,cpp}`, `hud.{hpp,cpp}`, `showcase_scene.{hpp,cpp}`, `self_check.{hpp,cpp}`, `self_check_scene.{hpp,cpp}`, `bench.{hpp,cpp}` (pure workloads), `bench_scene.{hpp,cpp}`, `CMakeLists.txt`, `tests/check.nnhs` |
| `examples/CMakeLists.txt` | changed | `add_subdirectory(entities)` |
| `examples/platformer/level_world.{hpp,cpp}` | new (step 1) | the platformer's components (`Body`, `Coin`, `Turret`), tags, the world's description and `static_assert(tags_distinct(...))` |
| `examples/platformer/coins.{hpp,cpp}` | new (step 1), replaces `coin_book.*` | spawn a coin; collect = `boxes_overlapping` over the body storage with the player's mask, `owner` → destroy |
| `examples/platformer/turrets.{hpp,cpp}` | changed (step 1) | turrets are entities; bullets in `Pool<Bullet, 16>`; bodies read through a box view of the body storage |
| `examples/platformer/level_play.{hpp,cpp}`, `level_scene.cpp` | changed | the world bound to the level scene; restart = `clear()` + spawns in map order; the player's body updated once per frame; step 2: sparkles, the bound `TweenSet`, the HUD count |
| `examples/platformer/sparkles.{hpp,cpp}` | new (step 2) | spawn 4 sparkles per coin, fade by a handle-capturing `Setter`, destroy on finish; remove hook kills the tween |
| `examples/platformer/replay_run.{hpp,cpp}`, `hud.{hpp,cpp}` | changed (step 2) | `ReplaySample::entities`, `::handles` in the checksum; the live count on the HUD |
| `examples/platformer/tests/level.nnhs` | changed (step 2) | regenerated golden lines |
| `tests/platformer/test_coins.cpp` (was `test_coin_book.cpp`), `test_turrets.cpp`, `test_replay_run.cpp`, `test_sparkles.cpp` | changed / new | the same cases over the world; step 2's words |
| docs (stage 11) | new / changed | `docs/api/pool.md`, `docs/api/entities.md`, `docs/api/scene.md` (attachments list), `docs/architecture/scenes.md`, `patterns.md`, `decisions.md` (D35 implemented), `docs/REFERENCES.md`, `CLAUDE.md`, `docs/ROADMAP.md`, `specs/README.md` |

### Data and memory

| Item | Size (3DS / PC) | Notes |
|------|-----------------|-------|
| `Handle<T>`, `Entity`, `Tag` | 4 B | |
| `Acquired<T>` | 8 B / 16 B | inline only |
| `ComponentDesc` | ≈ 48 B / 64 B | creation only (holds the hook `SmallFunction`) |
| `Pool<T, N>` | ≈ 56 B + 4·N + 12·⌈N/32⌉ + (N + 1)·sizeof(T) | inline; `Pool<Bullet(32 B), 16>` ≈ 680 B |
| `Pool<T>` | 4 B / 8 B + one block of the same terms | tag `pool` or the game's |
| `World` | 4 B / 8 B + one block | tag `entities` or the game's |
| world header + kinds | ≈ 64 + 52·n (+ 4·n ids) B | |
| per entity | 8 + 4·tags + 2·n (rounded to 4) + 2 + 0.375 B | default 4 tags, 8 kinds: 42.4 B |
| per component slot | sizeof(T) + 2 + 0.5 B (+ one scratch slot per kind) | |

Examples: 1024 entities, 8 kinds × 1024 slots, 4 tags → 43.4 KB (entities) + 20.5 KB (slot overhead) + 0.5 KB ≈
**64 KB besides the components** (spec: "well under 100 KB"). The platformer (256 entities, 5 kinds, 2 tags, ≈ 450
slots) ≈ 11 KB with its components. `examples/entities` ≈ 30 KB. Everything in the Heap region (CPU-only data).

### Platform work

- **pc / 3ds:** none. Portable core code with no contract change; the 3DS runs the self-check, the touch queries, the
  bench and the platformer (stage 10).
- **fake:** none. Tests run on the fake platform (memory, scenes through `nnh::init`); scene-binding tests push and pop
  scenes through `nnh::frame()` as spec 015's tests do.

## Test strategy

All unit tests run in `nnh_tests` on the fake platform (Windows and VM 120). A test-only counting type
(`Counted`: constructions, destructions, live objects) checks balance; the assert hook catches misuse; the allocation
counter checks "no allocation after creation". A **reference model** (`world_model.hpp`: plain arrays, linear
searches, the same rules written the obvious way) is compared with the real world after every operation of seeded
sequences (handles, liveness, components' values, tags, `find` results, iteration orders, stats).

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `tests/pool/test_handle.cpp`: null, packing, `from_bits`, comparison, `sizeof == 4`, trivially copyable; `test_slot_bits.cpp`: `next_generation` 65 535 → 1, never 0 |
| FR-2 | `test_pool.cpp`, `test_spawn.cpp`: stale after release/destroy, forged next-generation handle refused, double release/destroy false, `at` asserts (hook) and returns scratch `T{}` in the Release path, the wrap limit pinned (a handle across 65 535 reuses reads live) |
| FR-3 | `test_pool.cpp` (inline: no allocation counter change), `test_pool_runtime.cpp` (`create` capacity 0/65 536/−1 → `InvalidArgument`, an allocation failure → `OutOfMemory` through a tiny budget/fake, move, moved-from), `Counted` balance |
| FR-4, FR-5 | `test_pool.cpp`: lowest free, `acquire(value)` copy/move, full → null + miss without assert, release resets to `T{}`, quarantine (acquire after release in the same frame takes another slot) |
| FR-6 | `test_pool.cpp`: flush frees, clear restarts at slot 0 with the first-load slots, flush inside an iteration asserts |
| FR-7 | `test_pool_iteration.cpp`: ascending order, release self/others during `items()` and `each`, acquire during iteration not visited (also into a hole above the cursor), `break` keeps the guard balanced, raw access (`data`, `extent`, `stride`, `is_live`, `handle_at`) |
| FR-8 | `test_pool.cpp`: count, capacity, live/peak/misses, reset by clear |
| FR-9 | `test_world_create.cpp`: every `InvalidArgument` case, `OutOfMemory`, tags 0..8, the block size equals `world_layout` (tag stats), move, empty/moved-from asserts; `test_block_layout.cpp`: offsets aligned (an `alignas(32)` kind), totals by formula |
| FR-10 | `test_world_create.cpp`: 32 kinds accepted, 33 refused; unregistered kind asserts / null; a type with a destructor and a non-movable type (an animator) as kinds |
| FR-11 | `test_spawn.cpp`: lowest free, full asserts and counts, `alive` |
| FR-12 | `test_components.cpp`: add/get/has/remove per kind, add twice (assert, existing unchanged), kind full (assert, miss), stable addresses across other spawns/adds/removes, quarantine of component slots |
| FR-13 | `test_destroy_hooks.cpp`: hooks in registration order with data intact (`get` of other kinds works), components `T{}` after, tags gone, handle stale; hooks that destroy others (FIFO, each once, a 1000-long chain without recursion: stack depth constant), hooks that spawn, `remove` from its own hook (false), `destroy` of the hooked entity from a `remove` hook (processed after), add to a dying entity (assert) |
| FR-14 | `test_flush_clear.cpp`: flush, flush in `each` asserts, clear runs hooks in ascending slot order, `spawn` refused while `clearing()`, the first-load slots after clear (generations differ) |
| FR-15 | `test_each.cpp`: one/two/three kinds, the first-kind order rule (A rarer than B, B rarer than A), destroy/remove/spawn/add inside (self, earlier, later, into holes), nested `each`; `test_storage.cpp`: views, `owner`, `slot_of`, `items()`, extent/stride, valid after spawns |
| FR-16 | `test_tags.cpp`: `static_assert(nnh::tag("enemy") == ...)` (FNV-1a vector), `case` labels, run-time hash of a non-terminated buffer equals the literal's, `tags_distinct` true/false/with `Tag::None` (as values, and one `static_assert`) |
| FR-17 | `test_tags.cpp`, `test_find.cpp`: add/remove/has, per-entity limit (assert), tags gone after destroy, `find` order, totals beyond capacity, null + 0 counts, null + capacity > 0 asserts, `first`, `each_tagged` with destroys and spawns inside, `tags_per_entity` 0 and 1..8 (each specialised loop) |
| FR-18 | `test_pool_scene.cpp`, `test_world_scene.cpp`: flushed right before the bound scene's update and only on top (frozen under a pushed scene), cleared right after `on_exit` with hooks run and no events published (the bus stats), empty on re-entry, attach-order with a bound `TweenSet` (hooks before its clear), a hook that requests a scene change (queued), destroying a scene with a world still attached asserts, moving a bound world/pool asserts and moves nothing |
| FR-19 | `test_world_scene.cpp`, `test_world_model.cpp`: `events::stats()` unchanged by every operation |
| FR-20 | `test_interop.cpp`: AC-3's cases; an include check (T1.5, T11.4): `pool.hpp`/`entities.hpp` preprocessed without `collision`, `tween`, `animation`, `gfx` |
| FR-21 | `test_world_model.cpp` (model equality), `tests/entities_example/test_self_check.cpp` (AC-2 golden) on WinLibs GCC, GCC 13/14, clang 18 and the console |
| FR-22 | `test_world_misuse.cpp`: the allocation counter unchanged over 100 000 operations after creation; no log in the normal path (log hook count); bench numbers (AC-7) |
| FR-23 | `test_world_misuse.cpp`, `test_pool.cpp`: every listed misuse asserts (hook) and is harmless in the Release path |
| FR-24 | `test_world_create.cpp`, `test_components.cpp`: entity and kind stats |
| FR-25 | T0.1/T5.5/T8.4: `hello` sizes and `nm` (no pool/world symbol) on PC and 3DS; header include check; build times |
| AC-4 | ctest `platformer_replay_matches_script` (step 1 unchanged, step 2 regenerated), `level.nnhs --pc`, `test_coins.cpp`, `test_turrets.cpp`, `test_sparkles.cpp`, screenshot |
| AC-5 | PC screenshots `ENTITIES_DEMO=showcase` and the script's shots (tag highlight, stale retarget, full pool), read back |
| AC-6 | `examples/entities/tests/check.nnhs` with `--pc`, then on the console |
| AC-7 | `entities bench:` lines on the console; T7.6 disassembly table; `gate.nnhs` and the replay's 60 Hz verdict on the console |
| AC-8 | stage 0 reference screenshots compared byte for byte after stages 5, 6 and 8; `hello` sizes |

## Risks and alternatives

1. **`get<T>` and pool targets (AC-7).** Estimated 1.0–1.9 ms and ≈ 0.33 ms against 0.5 and 0.3. Mitigations in
   order, after the console says so: hoist the kind lookup explicitly in the hot inline path (cache the last kind
   index per `Storage`), put a kind's record slot next to the meta word for the first registered kinds, make the
   bench's handle array access sequential-friendly only if the workload allows it (it must stay "by handle"),
   `ldrd` meta + kinds in one load, and a hint word for the pool's lowest free. Then AC-7's measurement-with-margin
   policy.
2. **Spawn/destroy target close** (≈ 0.85 ms of 1 ms): skip the per-kind destroy call for trivially destructible kinds
   (`destroy == nullptr`) and reset trivially copyable kinds from a `T{}` prototype with word copies.
3. **Hooks that start work during a clear** (D30 proof, point 5): documented rule and `clearing()`; a test pins the
   order. A hook that publishes a game event during a clear is the game's choice (delivered next frame; the scene's
   subscriptions were reset in its `on_exit()`).
4. **Unbound world destroyed with hooks touching destroyed members**: an unbound world's destructor runs hooks; a hook
   that uses a member declared after the world (destroyed first) is the game's bug. Documented ("declare the world
   after what its hooks use"); bound worlds are already empty at destruction (cleared at exit).
5. **Destroy from a hook is deferred**: inside a hook, `alive(x)` stays true for an entity the hook just destroyed
   until the outermost operation finishes it. Documented with the reason (no recursion, every hook sees its data).
6. **Quarantine changes "release then acquire in the same frame"** for code that did it by hand; the platformer only
   releases bullets after it fires (look, then move), so its order is unchanged (stage 6 proves it). Its unit tests
   that fire directly between updates may see another slot; they are adapted, not weakened.
7. **Kind lookup by scan** (≤ 32 ids): fine for typical counts (3–8); `Storage<T>` hoists it. A hash was rejected as
   premature.
8. **`kMaxTests`**: 4096; the spec adds ≈ 250 tests (T0.4 checks the current count).
9. **Template bloat on the 3DS**: `each` instantiations per call site; measured as `.text` of `examples/entities` and the
   platformer before/after (T8.4).

## Decisions taken (pre-approved)

Small decisions the plan would normally ask about; each takes the recommended option.

1. **Names:** modules `pool` and `entities`; `<nnh/pool.hpp>`: `Pool<T, N>`, `Pool<T>` (run-time, `kRuntimeCapacity`),
   `Handle<T>`, `Acquired<T>`, `PoolStats`; `<nnh/entities.hpp>`: `World`, `Entity` (= `Handle<World>`), `WorldDesc`,
   `ComponentDesc`, `component<T>(capacity[, hook])`, `Storage<T>` (`world.storage<T>()`), `WorldStats`,
   `KindStats`, `Tag`, `tag()`, `tags_distinct()`; `at` is the checked accessor; `items()`/`entities()` are the
   ranges.
2. **Binding shape:** `bind(scene)` on all three; the inline pool also takes the scene in a constructor (as
   `TweenSet`), since it can't be moved after construction. A world or run-time pool is created by `create()` (a
   `Result`, moved into its member), then bound.
3. **Components registered by an array** (`World::create(desc, kinds)`; the `K` overload takes a C array), not an
   `initializer_list` member.
4. **Hooks are `SmallFunction`s** (captures ≤ 16 bytes, `[this]`), typed for the game (`World&, Entity, T&`), stored
   type-erased; one per kind.
5. **A free slot holds a constructed `T{}`**: `acquire()`/`add<T>()` hand it out as is; with a value, `T{}` is
   destroyed and the value copy/move-constructed. Constructions and destructions balance.
6. **Destroys from hooks are queued (FIFO)** and processed before the outermost operation returns; `remove<T>` on a
   dying entity and on a component whose hook is running returns false.
7. **`spawn()` during `clear()` returns null without assert**; hooks test `clearing()` to skip effects.
8. **`entities()`** (every live entity, ascending) is added for the platformer's handle hash and debugging; it obeys
   FR-15's rules.
9. **`Tag` is `enum class Tag : std::uint32_t`**, `Tag::None` (0) is refused by `add_tag`; a name hashing to 0 is
   caught by `tags_distinct`.
10. **"Destroyed while bound"** (FR-23) is the scene destroyed with the object attached; destroying a bound object
    detaches it (as tween sets).
11. **Iteration snapshots the first kind's extent**; newcomers are excluded by the fresh bitmap.
12. **Platformer step 1:** the player is an entity too (its body is the one per-frame write, from `Player::box()`);
    coins (`Body{bounds, kCoin}` + `Coin{value}`, tag `"coin"`), turrets (`Body{box, kEnemy}` + `Turret`, tag
    `"turret"`); spawn order on restart: player, then the map's objects in order; bullets in an inline `Pool<Bullet,
    16>` inside `Turrets`, bound to the level scene by `LevelPlay`, also flushed at the start of `Turrets::update()`
    (a no-op when bound; the unit tests run it unbound). Flushing a bound pool or world by hand is allowed and
    harmless.
13. **Platformer step 2:** 4 sparkles per coin (`Spark{position, alpha, fade}` + `Velocity`, tag `"fx"`), faded over
    24 frames by the level scene's bound `TweenSet` through a `Setter` capturing `[this, e]` that writes only while
    `alive(e)`; `on_finish` destroys the sparkle; `Spark`'s remove hook kills its tween (so a restart leaves no tween).
    The world is bound before the tween set. The replay sample gains two words (live entities, FNV-1a of the live
    handles' bits in `entities()` order).
14. **Example controls** (`examples/entities`): Circle Pad/D-Pad moves the ship, A fires (holding past 32 bullets shows
    misses), X cycles the tag filter (none → enemy → flying → coin), L/R freeze/unfreeze the filtered group, SELECT runs
    the self-check, Y runs the bench, a touch on the bottom screen destroys the entity under it; START quits.
15. **AC-7 targets kept as written** (the maintainer's rule); risk 1 lists what happens if the console misses them.
16. **Console stage:** three scripted runs, each started with Homebrew Launcher → Y (entities `check.nnhs` including
    the bench, platformer `level.nnhs`, platformer `gate.nnhs`); nothing else is asked of the maintainer.

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | Integer-only portable core; fixed rules (lowest free, ascending iteration, registration order, quarantine, +1 generations); model equality and the golden on every compiler and the console |
| II | ✅ | One block per world/run-time pool, ≈ 64 KB for 1024 × 8 × 4 besides components; ARM11 rules built in, disassembly checked before the console; AC-7 targets kept with their fallback |
| III | ✅ | Two light headers, thin templates, measured budgets |
| IV | ✅ | No engine events; hooks + the game's events |
| V | ✅ | No allocation after creation, no `virtual`, no RTTI, no exceptions |
| VI | ✅ | Tests first, a reference model, the platformer swap with unchanged lines before anything visible |
| VII | ✅ | Small files of one concept; one public header per module |
| VIII | ✅ | Two API pages, example, platformer, scenes and patterns pages, D35 |
| IX | ✅ | Own code, prior art cited |
| X | ✅ | Stale use detected, misuse loud in Debug and harmless in Release, no parser |
| XI | ✅ | D35 is D11's "later, as a module"; nothing else includes it; `hello` unchanged |

No constitution exception is requested. D35 is new and D11 gets a dated note (see
[decisions.md](../../docs/architecture/decisions.md)); two AC-7 targets are flagged at risk and keep the spec's
measurement-with-margin fallback.

## Stage 0 results

Measured 2026-11-23 on the maintainer's Windows machine (WinLibs GCC, devkitARM), at `aca6e3e`, before any spec 020
change.

- `hello` sizes (`size`, text / data / bss): pc-release `hello.exe` 130 572 / 22 020 / 211 696; pc-debug 198 464 /
  26 756 / 214 352; 3ds-release `hello.elf` 201 012 / 28 352 / 106 680 (`.3dsx` 246 400 bytes); 3ds-dev `hello.elf`
  217 728 / 32 872 / 109 288. `nm -C` symbol lists saved as `build/ref-020/hello.{pc,3ds}.nm`.
- `.text` of other 3ds-release ELFs: `platformer.elf` 316 876, `queries.elf` 275 860.
- Build times: edit `examples/hello/main.cpp` → simulator (pc-debug) 1.77, 1.81 s (a first run 3.3 s); → `.3dsx`
  (3ds-release) 3.12, 3.06 s; clean `nnh_core` (pc-debug, `ninja -t clean nnh_core`, parallel) 7.4 s.
- Header cost (`g++ -std=c++20 -fsyntax-only` of a one-line TU, 3 runs; `build/ref-020/hcost.sh`): `<nnh/events.hpp>`
  7636 preprocessed lines, 0.42–0.45 s; `<nnh/tween.hpp>` 8413 lines, 0.44–0.48 s; `<nnh/collision.hpp>` 691 lines,
  0.30–0.32 s.
- T0.2: reference screenshots in `build/ref-020/ref/` (not committed): `hello`, `sprites`, `scenes` at frame 30,
  `camera` (`CAMERA_DEMO=zoom1`) and `queries` (`QUERIES_DEMO=all`) at 30, platformer `PLATFORMER_DEMO=bench` at 90
  (top screen only: the bottom shows frame times), level 1 at 90 and `PLATFORMER_DEMO=replay` at 312.
  `build/ref-020/compare.sh` retakes them into `now/` and compares (bytes; the bench's top screen by pixels).
- T0.3: ctest `platformer_replay_matches_script` passes; the replay's last line is
  `replay: done 900 frames sum 96FE8943: 60 Hz PASS`.
- T0.4: `kMaxTests` 4096; `nnh_tests` 2022 tests: no change needed.

## Stage 1 refinements and results

- **`detail::SlotTable`** (`slot_bits.hpp`) holds a table's bitmap pointers and counters (capacity, extent, count, peak,
  misses, hint) and its inline operations (`find_free`, `take`, `retire`, `flush`, `end_iteration`, `reset`); pools and,
  in stage 2, each component kind embed one. Hint invariant: every word below it has no free slot; `flush()` lowers
  it to the first word it frees. A capacity that is not a multiple of 32 needs no padding bits: the lowest free bit at
  or past `capacity` means the table is full.
- **`PoolApi<T, D>`** (CRTP over the form's `core()`) is the one implementation of both forms; `PoolOps<T>` holds the
  typed inline operations on a `PoolCore`. The inline form's core points into its own arrays (`pool_init_inline`),
  the run-time form's core is the start of its block.
- **Empty pools**: commands (`acquire`, `items`, `each`, `at`, `flush`, `clear`, `bind`) assert (Release: log, answer
  nothing); queries (`get`, `alive`, `release`, `count`, `capacity`, `stats`, `extent`, `data`, `is_live`,
  `handle_at`) answer "nothing" without asserting, so a moved-from pool can still be destroyed and inspected.
- **`clear()` inside an iteration** asserts and is ignored, like `flush()`; `clear()` also resets peak and misses.
- Tests: `tests/pool/test_handle.cpp`, `test_slot_bits.cpp` (word boundaries, capacity 33, the naive reference on
  10 000 seeded operations), `test_pool.cpp`, `test_pool_iteration.cpp`, `test_pool_runtime.cpp`; behaviour cases run
  on both forms through `POOL_CASE` (`pool_fixture.hpp`). `nnh_tests` 2080 tests.
- T1.6: a TU including only `<nnh/pool.hpp>` pulls 52 headers, none of collision, tween, animation, gfx, entities or a
  standard container (`build/ref-020/inccheck.sh`); 8199 preprocessed lines, 0.43–0.47 s (`<nnh/result.hpp>`'s
  `<utility>` is most of it, as for `tween.hpp`). ctest pc-debug 264/264, pc-release 257/257, 3ds-release without a
  warning; `hello` sizes and `nm` lists equal stage 0's on pc-release and 3ds-release.

## Stage 2–4 refinements and results

- **Files** as planned (`detail/world_layout.hpp`, `entities.hpp`, `src/entities/block_layout.{hpp,cpp}`,
  `world_create.cpp`, `world_lifetime.cpp`, `world_spawn.cpp`, `world_components.cpp`, `world_destroy.cpp`,
  `world_flush.cpp`, `world_tags.cpp`, `tags_find.cpp`, `world_scene.cpp`, `world_stats.cpp`, `world_internal.hpp`).
  Each kind's `KindHot` embeds the pools' `SlotTable` (one implementation of lowest free, quarantine and fresh).
- **The dying flag is a fourth entity bitmap** (in `WorldCold`), not bit 17 of the meta word: the meta compare of
  `get`/`alive`/`live_record` stays one load and one compare with no mask, and a dying entity still reads as live
  (its hooks see their data; a queued entity stays `alive` until processed, risk 5). Per entity this is 4 bitmap bits
  (0.5 B) instead of 3.
- **The record's slot field** is a `u16` array right after the tags; a record is `2 + tags + ⌈kinds / 2⌉` words.
- **`add()` returns `kAddedExisting | slot`** from the out-of-line part when the entity already has the kind, so
  `add(e, value)` leaves the existing component unchanged and the kind is looked up once per call.
- **A world destroyed inside its own iteration or hook** (misuse): Debug/Development assert before anything changes;
  Release marks the block doomed (`kPendingDoomed`), runs no more hooks, and the outermost operation or iteration end
  frees it (every object destroyed). Move-assigning over a busy world reports the same misuse first.
- **Empty worlds**: commands assert; `get`/`has`/`alive`/`destroy`/`remove`/`has_tag`/stats answer "nothing".
  `get<T>` of an unregistered kind asserts (FR-10) and answers null.
- **`clear()` inside a hook** asserts and is ignored, like inside an iteration (it would destroy entities whose hooks
  are running); `clear()` keeps the stats (peak and misses are since creation).
- **Reference model** (`tests/entities/world_model.hpp`, driver `model_driver.{hpp,cpp}`): three kinds whose hooks
  destroy (queued), remove another kind or spawn with an add; operations: spawn, destroy, add, remove, flush, clear,
  `each<MA>` with destroys inside, `each<MB, MA>` with removes and spawns inside, `each<MA, MB, MC>`, tags,
  `find`/`first`, `each_tagged` with destroys, `entities()` with spawns. The whole state (liveness and generation per
  slot, every component slot's liveness, owner and value, extents, tags) is compared after every operation:
  `world_equals_the_reference_model_on_50_seeded_sequences` (50 × 5000 operations, tags 1..8), one run without tags,
  and `world_operations_publish_no_events` (2000 operations with the engine shut down: any publish would assert).
  The fuzz target `fuzz_world_ops` runs the same driver (mini-fuzz: 200 random inputs of up to 4 KiB; libFuzzer
  executable `fuzz_world_ops` with `NNH_BUILD_FUZZERS`).
- Tests: `test_block_layout`, `test_world_create`, `test_spawn`, `test_components`, `test_storage`,
  `test_destroy_hooks` (including the 1000-long chain), `test_flush_clear`, `test_each`, `test_world_misuse` (100 000
  operations with no allocation and no log line; the doomed world), `test_world_model`, `test_tags`, `test_find`,
  `test_fuzz_world_ops`. `nnh_tests` 2155 (pc-debug), 2115 (pc-release); ctest pc-debug 264/264, pc-release 257/257;
  3ds-release without a warning; `hello` sizes and `nm` equal stage 0's.

## Stage 5 refinements and results

- **Pool binding** (`pool_scene.cpp`): `PoolCore::node` holds the `SceneAttachment` (checked by `static_assert`s);
  `BeforeUpdate` flushes, `AfterExit` calls the typed `clear_fn`. **World binding** (`world_scene.cpp`): the node is
  in `WorldCold`; `bind()` records the World object (`owner`), which can't move while bound, and the notification
  calls `flush()`/`clear()` on it, so hooks get the game's own World.
- `scene.hpp`'s destructor check now names "a bound TweenSet, World or Pool".
- **D30 proof as tests** (`test_world_scene.cpp`): `XhhE` call order (on_exit, the two remaining hooks of the exit
  clear, then the next scene's on_enter); spawns refused in the exit clear; a hook's `scenes::replace` during the exit
  clear is queued (the next scene enters, then the request replaces it); a world bound before a `TweenSet` sees the
  tween still running in its hooks, one bound after sees it already cleared (kill of a stale handle is harmless); a
  scene destroyed with an outliving world attached stops the game (NNH_VERIFY, every flavor); a bound world doesn't
  move. `test_pool_scene.cpp`: the same for pools (flush right before update, frozen under a pushed scene, cleared
  after on_exit, empty on re-entry, a bound run-time pool doesn't move and detaches when destroyed).
- **Interop** (`test_interop.cpp`): `boxes_overlapping`, `boxes_at` and `raycast` over `storage<Body>()` with live,
  quarantined and free slots report only live bodies and `owner()` maps the index back; an animator component whose
  notice callback destroys its own entity; a handle-capturing `Setter` stops writing after destroy; a pointer tween
  killed by the kind's remove hook. **Found while writing it:** a callback stored *inside* the component it destroys
  (an animator's `on_notice` lambda) loses its captures once `destroy()` returns (the slot now holds a fresh `T{}`):
  copy what it needs first and destroy last, or destroy after `update()` from its returned notices. Documented in
  `docs/api/entities.md` (pitfalls).
- **Self-check** (`examples/entities/self_check.{hpp,cpp}`): kinds A (16 B), B (8 B, remove hook destroying by value or
  spawning with an A), C (4 B); 4 tags per entity, six tags; a run-time `Pool<Item>` of 256. 32 operation kinds by
  FNV-1a of (seed, step, salt): spawns with up to 3 kinds and 2 tags (live limit 700), destroys, adds/removes of B and
  C, `each<A>` with destroys inside, `each<B, A>` with removes and spawns inside, `each<A, B, C>`, tags,
  `find`/`first`, `each_tagged` with destroys, pool acquire/release/`items()` with releases inside/flush, world
  flushes, rarely `clear()`; a flush of both every 50 operations keeps room (a full world asserts). Reached: 17 825
  spawns, 2879 destroys, 988 destroys and 297 spawns from hooks, 50 clears, 830 889 visits, 3125 finds, 9242 pool
  acquires, 133 pool misses, 700 live at most. Golden `0xC8FB9A41` (checkpoints `723FB297`, `3BB094CB`, `58530BE8`,
  `21AD333D`, `DC95F672`, `C5CFA4B7`, `284AFBEE`, `F1272C58`, `45824FCC`, `C8FB9A41`), pc-debug and pc-release.
- T5.5: ctest pc-debug 264/264, pc-release 257/257, 3ds-release without a warning, `nnh_tests` 2175; `hello` sizes and
  `nm` equal stage 0's; `compare.sh`: every reference screenshot identical.

## Stage 6 refinements and results

- **Files**: `level_world.{hpp,cpp}` (`level::Body`, `Coin`, `Turret`; tags `"player"`, `"coin"`, `"turret"` with
  `static_assert(tags_distinct(...))`; `create_world()`: 256 entities, Body 256, Coin 128, Turret 4, 2 tags);
  `coins.{hpp,cpp}` (was `coin_book.*`: `spawn_coin`, `collect_coins` → `TakenCoin{entity, value}` in ascending body
  slot); `turrets.{hpp,cpp}` keeps the name `TurretBook` but owns only the bullets (`Pool<Bullet, 16>`, no `active`
  flag), hits and flash: turrets are entities (`spawn_turret`), `update(grid, player_entity)` reads the player's Body,
  flushes the pool first (a no-op when bound) and maps a ray's `hit.box` to the player through
  `storage<Body>().owner()`; `state()` hashes `each<Turret>` then the pool's `items()`.
- `LevelPlay(art, map, scene)` creates the world and binds it and the bullet pool to the level scene. The map's coins
  and turrets are no longer stored at construction: `restart()` = `spawn_things()` (`clear()`, the player's entity
  with its Body on `kPlayer`, then coins and turrets in object order) + the turrets' restart. The player's Body is
  written once per frame after it moves (the value the old book copied into its body array). `draw()` and `sample()`
  lost their `const` (iterations hold a guard in the world).
- `test_coin_book.cpp` → `test_coins.cpp`: the eleven cases over the world, plus "other bodies in the storage are
  ignored". `test_turrets.cpp`: every case kept on a world with a player entity; two new: bullets pass coins, and a
  released bullet's slot is reused by the next frame's shot (the old "first inactive bullet").
- **T6.3: the golden lines are byte-identical.** ctest `platformer_replay_matches_script` passes and `nnh run platformer
  --pc --no-build --script examples/platformer/tests/level.nnhs` → `nnh: PASS (76 lines, frames 1-1553)` with the
  **unchanged** `level.nnhs` (last line `replay: done 900 frames sum 96FE8943: 60 Hz PASS`); `compare.sh`: level 1 at
  frame 90, the replay at 312 and the bench identical. ctest pc-debug 264/264, pc-release 257/257, 3ds-release without
  a warning; `nnh_tests` 2178.
