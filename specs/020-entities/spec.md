# Spec 020: Entities, tags and object pools

Status: approved (2026-11-23, by the maintainer: every open question below takes its recommended answer)
Author: NNHGE maintainers · Date: 2026-11-23
Depends on: [spec 002](../002-core-foundation/spec.md) (asserts, logs, `Result`, tagged memory, profiling),
[spec 001](../001-event-bus-and-scenes/spec.md) (scenes; the event bus for the game's own events) and the scene
attachments of [spec 015](../015-tweens-and-timers/spec.md) (decision D30: an object bound to a scene is notified right
before the scene's update and right after its exit). Works with [spec 019](../019-collision-layers-and-queries/spec.md)'s
box views (queries read component storage in place), [spec 016](../016-sprite-animation/spec.md)'s animators and
[spec 015](../015-tweens-and-timers/spec.md)'s tweens (as components or targeting components) without depending on any
of them. Verified on the console with [spec 013](../013-console-remote/spec.md)'s `nnh run --script`. Implements three
ROADMAP items together: "Entities/components" (M7), "Tags and groups" and "Object pools" (Features in the spirit of
Godot and Unity). Optional for games (constitution XI, decision D25).

## Problem and goal

Every game object in NNHGE's examples lives in a hand-written fixed array: the platformer's `CoinBook` holds 128
coins and marks taken ones with "no layers", its `TurretBook` holds 4 turrets and 16 bullets with an `active` flag, a
"first inactive slot" search to fire, and a parallel `Body` array rebuilt every frame so a ray can see the player and
the turrets. Each new kind of thing repeats the same code — capacity, a free slot, an active flag, a restart, a loop
that skips dead entries — and none of it can say "this enemy I was chasing has died" except by a raw index that now
names a different enemy. Unity (GameObjects, tags, `ObjectPool`), Godot (nodes, groups, `queue_free`) and the ECS
libraries (EnTT, flecs, Bevy) all give games a way to create things, attach data to them, find them by name, and
recycle them safely. After this spec, an NNHGE game can keep **fixed-size pools** of its own structs with
**generational handles** that tell a stale reference from a live one, or a **world** of entities whose
**components** are the game's plain structs (a box with layers, a velocity, a sprite animator), stored kind by kind in
arrays with stable slots so spec 019's box views read them in place; mark entities with **tags** hashed at compile time
and find every entity with a tag; destroy things in the middle of a loop without corrupting it; and bind a world or a
pool to a scene so it is flushed and emptied with it — with fixed capacities chosen at creation, one block of tagged
memory, no allocation while playing, and the same handles, order and results on PC and 3DS. A game that uses none of
it pays nothing.

## User stories

- As a game developer, I want a pool of 64 bullets that hands me a free one in O(1)-ish time and takes it back, with
  no allocation, so that I stop writing "find the first inactive slot" for every kind of projectile and particle.
- As a game developer, I want to keep a handle to an enemy (the target of a homing missile, the boss a health bar
  follows) and ask later whether it is still that enemy, so that a recycled slot never makes my missile chase the
  wrong thing.
- As a game developer, I want to describe a thing by the data it carries — a body (box and collision layers), a
  velocity, a sprite animator, the game's own `Health` — as plain structs, and add or remove those pieces at run time,
  so that a turret and a moving platform share the code that moves bodies without sharing a base class.
- As a game developer, I want to loop over every entity that has a body and a velocity, in a tight loop over arrays,
  so that moving 500 things costs a fraction of a frame on an Old 3DS.
- As a game developer, I want to destroy the enemy I am looking at (or another one) from inside that loop, and spawn
  new ones there, without skipping, repeating or corrupting anything.
- As a game developer, I want to tag entities with names — `"enemy"`, `"coin"`, `"flying"` — compare tags as integers
  computed at compile time, and find every entity with a tag, so that "every enemy on screen freezes" or "the nearest
  coin" needs no list of my own.
- As a game developer, I want the bodies of my entities to be readable by spec 019's ray and overlap queries where they
  are, with no copy, and to turn a query's box index back into an entity.
- As a game developer, I want my world (or pool) to belong to a level scene: flushed every frame before the level
  updates, frozen while a pause menu is on top, and emptied when the level is left, without calling anything myself.
- As a game developer, I want the same spawns, destroys and loops to give the same handles and the same order on PC
  and 3DS, so that replays and console checks compare exactly.
- As a game developer who prefers my own arrays (or uses only a pool and no world), I want my game, its executable size
  and its build time to stay as they are.
- As a contributor, I want all of it tested without a console, on the fake platform.

## Functional requirements

### Handles

- **FR-1 Generational handles.** A **handle** names one slot of a pool or one entity of a world: a 32-bit trivially
  copyable value holding a **slot index** (16 bits) and a **generation** (16 bits). The **null handle** is all zero
  and generation 0 is never issued, so a value-initialised handle is null. Handles compare with `==`/`!=`, can be
  stored in the game's structs and events (4 bytes), and expose their slot and generation (for logs and checksums).
  Every time a slot is released or its entity destroyed, the slot's generation increases (wrapping from 65 535 to 1),
  so every handle issued before is **stale** from that moment. Pool handles are typed by the pool's element
  (`Handle<Bullet>`), entity handles are one type (`Entity`); names fixed by the plan.
- **FR-2 Stale use is detected, not undefined.** Looking up a stale or null handle returns "nothing" (a null pointer,
  `false`), never another object; releasing or destroying a stale or null handle returns `false` and changes nothing
  (destroying the same enemy twice in a frame — two bullets hit it — is normal). A **checked accessor** that expects a
  live handle asserts in Debug/Development and returns a value-initialised object's storage in Release (never another
  live object). The one documented limit: a handle kept while its slot is reused exactly 65 535 times reads as live
  again (generation wrap); at one reuse per frame that is 18 minutes.

### Object pools

- **FR-3 A pool.** `nnh::Pool` holds up to `capacity` objects of the game's type `T` (any default-constructible type
  whose destructor doesn't throw; it need not be trivially copyable or movable: objects never move). Two forms with
  one behaviour: capacity as a template argument with the storage **inside the pool object** (no allocation at all:
  `Pool<Bullet, 64>` as a scene member), and capacity chosen at run time, created with `Result`-returning `create`
  in **one block** from tagged memory (tag chosen by the game, default an engine tag; out of memory or a capacity
  outside 1..65 535 is an error). A pool is not copyable; the run-time form is movable when not bound to a scene.
- **FR-4 Acquire.** `acquire()` (or `acquire(value)`) takes the **lowest free slot**, constructs the object there
  (`T{}` or a copy/move of `value`) and returns its handle and a reference. A full pool returns the null handle without
  asserting (a full bullet pool is normal play) and counts a **miss** in its stats.
- **FR-5 Release.** `release(handle)` immediately makes the handle stale, destroys the object and leaves a
  value-initialised `T{}` in the slot, so any pointer the game still holds points at valid memory of the same type
  (never freed memory, never another object). The slot is **quarantined**: it is not reused until the next flush
  (FR-6), so nothing acquired in the same frame can take the place of a released object while loops and views of that
  frame are running.
- **FR-6 Flush.** `flush()` returns quarantined slots to the free set. An unbound pool is flushed by the game (once per
  frame, typically at the start of its update); a pool bound to a scene (FR-18) is flushed automatically. Flushing
  inside one of the pool's own iterations asserts and is ignored. `clear()` releases every object and flushes (a level
  restarts with the same slots it had at the first load).
- **FR-7 Iteration and raw access.** The live objects can be visited in **ascending slot order** (a range-for giving
  handle and reference, and an `each` taking a callable). Releasing any object during an iteration is safe (released
  objects are not visited afterwards); objects acquired during an iteration are **not** visited by it. The storage is
  also readable as a plain array for other modules: a pointer to slot 0, the **extent** (one past the highest slot ever
  used since the last `clear`, ≤ capacity) and the element stride; whether a slot is live and the handle in a slot.
  Every slot that is not live holds `T{}` (FR-5).
- **FR-8 Pool stats.** `count()`, `capacity()`, and stats: live, peak, misses since creation (or since `clear`).

### Entities and components

- **FR-9 A world.** `nnh::World` (name fixed by the plan) is created by a `Result`-returning `create(desc)` in **one
  block** of tagged memory sized by its description: the maximum number of entities (1..65 535), the **component
  kinds** it stores, each with its own capacity (1..maximum entities), and the number of tags per entity (0..8,
  default 4). Errors (`InvalidArgument`, out of memory) are returned, never thrown. A world is movable when not bound to
  a scene, not copyable; its destructor destroys every entity (FR-13 hooks run) and frees the block. A game may have
  several worlds.
- **FR-10 Components are the game's plain structs.** A component kind is any default-constructible type with a
  non-throwing destructor (`Body { Rect box; collision::Layers layers = kNoLayers; }`, `Velocity`, a
  `SpriteAnimator`). It is registered in the world's description with its capacity (`nnh::component<Body>(256)`, name
  fixed by the plan) and identified at compile time without RTTI (the address-per-type id the event bus already uses);
  a world holds at most 32 kinds and keeps for each one its size, alignment and construct/destroy functions (what a
  future prefab needs to add components by kind, see Non-goals). Asking a world for a kind it doesn't store asserts in
  Debug/Development and answers "nothing" in Release.
- **FR-11 Entities.** `spawn()` creates an entity in the lowest free entity slot and returns its `Entity` handle; a full
  world returns the null handle and **asserts in Debug/Development** (a world's capacity is the game's budget; pools
  are where "full" is normal). `alive(e)` tells a live handle from a stale one.
- **FR-12 Adding, reading and removing components.** `add<T>(e)` / `add<T>(e, value)` constructs a component of kind
  `T` for a live entity in the lowest free slot of that kind's storage and returns a pointer to it (null when the
  entity is stale or the kind's storage is full, the latter asserting in Debug/Development; adding a kind the entity
  already has asserts and returns the existing component unchanged). `get<T>(e)` returns the entity's component or
  null in **constant time** (no search); `has<T>(e)`. `remove<T>(e)` destroys the component at once (its slot holds
  `T{}` and is quarantined until the flush, as FR-5). A component's address never changes while it exists (stable
  slots: no packing, no swap-and-pop), so a pointer to it, an animator's `this` or a tween's pointer target stays valid
  until it is removed.
- **FR-13 Destroying entities.** `destroy(e)`, at any time including inside an iteration:
  1. runs the **remove hooks** of the entity's components (an optional function per kind, given at registration, called
     with the world, the entity and the component, in kind registration order) — the place to stop a sound, kill a
     tween or spawn an explosion where the body was;
  2. destroys every component (slots hold `T{}`, quarantined), removes the entity's tags, and makes its handle stale.
  From step 2 the entity is invisible: `alive` is false, lookups answer nothing, iterations and tag searches skip it,
  and a box view over a storage sees `T{}` in its slots (FR-17). Hooks may destroy other entities (each is processed
  completely, in call order, without unbounded recursion) and may spawn entities. `remove<T>` runs that kind's hook
  too. Entity slots are quarantined until the flush like component slots.
- **FR-14 Flush and clear.** `flush()` returns every quarantined entity and component slot to its free set (bound
  worlds: automatically, FR-18; asserts and is ignored inside an iteration). `clear()` destroys every entity (hooks run,
  in ascending slot order) and flushes, so a level rebuilt after `clear()` gets the same slots and handles' slot
  indices as the first time (generations differ).
- **FR-15 Iterating components.** `each<A, B, ...>(fn)` calls `fn(Entity, A&, B&, ...)` for every live entity that has
  **all** the listed kinds, in **ascending slot order of the first listed kind's storage** (the game lists its rarest
  kind first for speed; the order rule is documented and deterministic). Destroying or removing during the iteration is
  safe (the entity or component is not visited afterwards); entities and components created during it are not visited.
  The callable is inlined (a template parameter, no stored function, no allocation). A **storage view** per kind
  (`world.storage<T>()`, name fixed by the plan) gives what FR-7 gives for pools — range-for in slot order, pointer to
  slot 0, extent, stride, liveness — plus `owner(slot)` → `Entity` and the slot of an entity's component; a view stays
  valid for the world's life.

### Tags and groups

- **FR-16 Tags.** A **tag** is a 32-bit value: the engine's FNV-1a 32 hash of its name (the hash data paths and sprite
  names already use), made at compile time from a literal (`nnh::tag("enemy")`, usable as a `constexpr` constant and a
  `case` label) or at run time from a string the game read (a Tiled object's type, a data asset's field) with the same
  bits. Comparing tags is comparing integers. A compile-time helper checks that a list of the game's tag constants are
  pairwise distinct (`static_assert(nnh::tags_distinct(kEnemy, kCoin, kFlying))`, name fixed by the plan), the
  documented way to rule out a hash collision; the engine stores no names (Release binaries carry no tag strings).
- **FR-17 Tagging entities and finding them.** An entity carries up to the world's tags-per-entity (FR-9) tags:
  `add_tag` (false when full or already there; full asserts in Debug/Development), `remove_tag`, `has_tag`. Tags go with
  the entity when it is destroyed. `find(tag, out, capacity)` writes the live entities that have `tag` in **ascending
  entity slot order** into the caller's buffer and returns the total (which may exceed `capacity`; a null buffer with
  capacity 0 counts — spec 019's contract); `first(tag)` returns the lowest-slot one or null; `each_tagged(tag, fn)`
  visits them with FR-15's safety rules. Godot's "groups" are these tags: "call a group" is `each_tagged` (documented).
  Pools have no tags: a pool is already a group of one kind.

### Scenes, interop, determinism, cost, safety, opt-in

- **FR-18 Scene binding (D30).** A pool or a world can be **bound to a scene** (at construction or creation, or by a
  call; the plan fixes the shape) through D30's scene attachments, with no new kind of notification:
  - right **before the scene's update**, only while it is the top scene, it is flushed (so slots released during the
    previous frame's update, draw and event handlers become free; a pause menu on top freezes it);
  - right **after the scene's `on_exit()`**, it is cleared (FR-6, FR-14; hooks run, no events), so a level entered
    again starts empty. A bound pool or world must not outlive or be moved away from its scene (asserts, as bound tween
    sets do). Unbound ones are flushed and cleared by the game.
- **FR-19 No built-in lifecycle events.** The engine publishes no events for spawns, destroys or component changes:
  state is queried (constitution IV), and an engine event delivered at the next dispatch would carry an already stale
  handle. Games publish their own facts (`EnemyDefeated{Entity who, int score}`) from game code or from a remove hook;
  the documentation shows the pattern and that a handle received in an event may be stale by delivery (`alive` tells).
- **FR-20 Interop by data layout, not by includes.** The pool and world headers include neither collision, tweens,
  animation nor graphics, and those modules don't include them; they meet through the data:
  - **box views (spec 019)**: a storage's pointer, extent and stride feed `collision::box_view(...)` with member
    pointers, and a result's box index is a slot (`owner(index)` gives the entity). Because non-live slots hold `T{}`, a
    component whose layers member defaults to `kNoLayers` is invisible to every query in its free and quarantined slots
    — the documented pattern;
  - **animators (spec 016)**: a `SpriteAnimator` is a component like any other (stable address; its callback may
    capture `this` of the game object that owns the world);
  - **tweens (spec 015)**: a tween may target a component field by pointer while the component lives (its remove hook
    kills the tween), or — recommended — through a `Setter` that captures the world and the entity handle and writes
    only while the entity is alive (stale-safe); a tween set stays one per scene, not a component.
- **FR-21 Same results everywhere.** Pools and worlds use only integer state and fixed rules: lowest free slot,
  ascending slot iteration, hooks in kind registration order, quarantine until flush, generations +1 per release. Type
  ids are used only as keys, never ordered (an address differs per system); kinds are ordered by registration. The
  same sequence of calls gives the same handles, slots and visiting order bit for bit on PC (every supported host
  compiler) and on the 3DS.
- **FR-22 Bounded cost, no allocation while playing.** After creation nothing allocates, logs in the normal path or
  calls `virtual` functions (per-kind construct/destroy/hook functions are plain function pointers, called once per
  component created or destroyed, never per frame per object). Costs: handle lookup and `get<T>` constant time;
  acquire/spawn/add at most one scan of a free bitmap (capacity / 32 words); iteration proportional to the first kind's
  extent; tag search proportional to the entity extent times tags per entity. Measured on an Old 3DS (AC-7).
- **FR-23 Misuse is loud in Debug, harmless in Release.** A capacity or tag count out of range at creation is an error
  (`Result`); an unregistered kind, a null buffer with capacity > 0, a negative capacity, flush inside an iteration, a
  bound pool or world moved or destroyed while bound, using a moved-from world, the checked accessor on a stale handle
  — assert in Debug/Development and do nothing (or answer nothing) in Release; never undefined behaviour. A handle used
  with another pool or world of the same type is undetected beyond its generation (documented; no world id in 32 bits).
- **FR-24 Stats.** A world reports entities live/peak/capacity, per kind live/peak/capacity, and spawn/add misses,
  enough for a debug overlay line and for tuning capacities (the example shows them).
- **FR-25 Optional and light.** Pools and worlds are optional (XI): a game may use a pool without a world, a world
  without tags (0 tags per entity), and neither. Pools are usable from their own public header without the world's
  (two modules, names fixed by the plan), and the headers stay light (no standard containers, no `<functional>`).
  A game that includes neither links nothing new: `examples/hello`'s executable is unchanged on PC and 3DS, and no
  existing module's behaviour or results change.

## Acceptance criteria

- **AC-1 Unit tests first** (constitution VI), on the fake platform. Handles: null, slot/generation packing, wrap from
  65 535 to 1, stale after release/destroy, lookups of stale handles return nothing, double release/destroy returns
  false, checked accessor asserts. Pools (both forms): lowest free slot, full → null + miss (no assert), release resets
  the slot to `T{}` and quarantines it, flush, clear gives the first-load slots, iteration in slot order, release during
  iteration (self and others), acquire during iteration not visited, raw access (extent, stride, liveness), a type with
  a destructor (counted constructions/destructions balance), run-time form out of memory. Worlds: creation errors,
  spawn order, full world asserts, add/get/has/remove per kind, add twice, stable addresses across other spawns and
  removals, destroy runs hooks in kind order then resets components, hooks that destroy and spawn others, quarantine,
  flush, clear (hooks in ascending slot order), `each` with one, two and three kinds and the first-kind order rule,
  destroy/remove/spawn/add during `each`, storage views and `owner`. Tags: compile-time and run-time hashes equal,
  `tags_distinct` (a `static_assert` and a negative test of the helper's value), add/remove/has, per-entity limit, tags
  gone after destroy, `find` order, totals beyond capacity and counting with a null buffer, `first`, `each_tagged`.
  Misuse cases (FR-23). The Linux build VM runs them with GCC 13/14 and clang 18 and with ASan/UBSan (constructions,
  destructions and quarantine checked for leaks and use-after-destroy), as previous specs did.
- **AC-2 Golden values.** A test runs a fixed, seeded script of 100 000 operations (the engine's own integer hash, no
  `rand`) — spawns, destroys (also from hooks and inside iterations), adds and removes of three kinds, tags, flushes,
  clears, pool acquires and releases — over a world of 1024 entities and a pool of 256, and compares a checksum of
  every returned handle's bits and every visiting order with a golden value; the same test passes on every host
  compiler and in the console self-check (AC-6).
- **AC-3 Interop and scenes.** Unit tests: a `collision::box_view` over a body storage with live, free and quarantined
  slots (bodies defaulting to `kNoLayers`) — `boxes_overlapping` and `raycast` report only live bodies and `owner(index)`
  gives their entities; a `SpriteAnimator` component updated in place with a callback; a tween writing a component
  through a handle-capturing setter stops writing after `destroy` and a pointer-targeted tween killed by a remove hook.
  Scene binding on the fake platform: a bound world and a bound pool are flushed right before the scene's update and
  only while it is on top (frozen under a pushed scene), cleared right after `on_exit` with hooks run and no events
  published, empty when the scene is entered again; destroying a scene with a bound world still attached asserts.
- **AC-4 The platformer adopts entities and pools**, in two documented steps, as specs 018 and 019 did:
  1. **Same play first** — level 1 and the bonus level keep their coins, turrets and bullets in a world (or, for the
     bullets, a pool) bound to the level scene: coins are entities with a body component (layers `kCoin`, `kNoLayers`
     by default) and a value, tagged `"coin"`; turrets are entities with a body and a turret component, tagged
     `"turret"`; bullets come from lowest free slots exactly as the hand-written "first inactive bullet" did and are
     visited in slot order. Taking a coin destroys its entity; a restart is `clear()` plus the same spawns in the map's
     object order, so slots match the first load; the bullets' rays read the bodies through a box view over the body
     storage and map a hit back to an entity. The hand-written `CoinBook`/`TurretBook` arrays, active flags and the
     per-frame `Body` copy go away. The replay's checksum lines in `examples/platformer/tests/level.nnhs` and the ctest
     `platformer_replay_matches_script` pass **unchanged** (the turrets' state word hashes the same bullets in the same
     order). This proves the swap; if the plan finds a byte that must differ, it says why before changing a golden line.
  2. **Then something visible** — taking a coin bursts it into a few short-lived **sparkle** entities (a velocity, a
     tween or animator component, tag `"fx"`) that destroy themselves from their own callback when they finish
     (exercising destroy inside an update), and the HUD shows the world's live entity count. The replay sample **adds**
     the world's live count and a hash of the live entity handles to its checksum — spawn and destroy order is exactly
     what the console must reproduce — so the expected lines change: they are regenerated from the PC with the procedure
     at the top of `level.nnhs`, the ctest passes again, and the console run of `level.nnhs` must print the new PC lines.
     The commit that adds the sparkles changes the golden lines in the same commit and says why. A PC screenshot
     (`NNH_EXIT_AFTER_FRAMES` + `NNH_SCREENSHOT`) shows a coin's sparkles and the entity count in the HUD.
  The benchmark, zoom and flip demos are untouched.
- **AC-5 Example `examples/entities`** (original art or shapes only), modes chosen by `ENTITIES_DEMO`
  (`showcase` (default) | `check` | `bench`): the top screen shows a small arena with entities of several kinds —
  walkers (body + velocity, tag `"enemy"`), flyers (body + velocity + a bobbing tween, tags `"enemy"`, `"flying"`),
  coins (body, tag `"coin"`) — and a ship that fires bullets from a pool of 32 (the HUD shows live/capacity and misses
  when the player holds fire past the capacity). A **homing missile** keeps the handle of its target; when the target is
  destroyed the HUD shows "target lost (stale handle slot n gen g)" and the missile retargets with `first("enemy")`.
  Buttons cycle a tag filter that highlights every entity `find` returns for it and freezes that group (`each_tagged`).
  On the bottom screen a **touch** is turned into a world point and a spec 019 `boxes_at` query over the body storage
  picks the entity under it, which is destroyed (its remove hook spawns an explosion); the HUD lists the world's stats
  (FR-24). PC screenshots for the showcase, a tag highlight, a stale-handle retarget and a full pool, checked by
  reading the PNGs.
- **AC-6 Console script.** The `check` mode runs AC-2's script on the engine's real pools and worlds and logs
  `entities: <n> sum <checksum>` every 10 000 operations and `entities: PASS <checksum>` or `entities: FAIL ...` at the
  end, against golden values recorded from the simulator. `examples/entities/tests/check.nnhs` starts it, expects every
  line, then switches to the showcase, **touches** fixed points on the bottom screen and expects the logged answers (for
  example `destroyed: slot 5 gen 2 tags enemy,flying`), cycles the tag filter and expects the logged `find` totals, and
  takes screenshots. On an Old 3DS the maintainer only opens the Homebrew Launcher and presses Y, and
  `nnh run entities --script examples/entities/tests/check.nnhs` passes; the same script passes with `--pc`. The
  verdict, log and screenshots are in the saved log folder.
- **AC-7 Cost on the console.** The `bench` mode, measured on an Old 3DS with the profiler and logged as
  `entities bench: PASS <numbers>` (or FAIL). **Targets** (the plan confirms them before code; they predate any console
  measurement):
  - `each<Body, Velocity>` over 1024 entities (both kinds on every one), adding velocity to position: **under 0.5 ms**;
  - 256 spawns with 3 components and 2 tags plus 256 destroys and one flush: **under 1 ms**;
  - `find` of one tag over 1024 entities with 4 tags each: **under 0.1 ms**;
  - 10 000 `get<T>` lookups by handle: **under 0.5 ms**;
  - 1000 pool acquire/release pairs plus a flush: **under 0.3 ms**.
  If a target is missed, the implementation is optimised first; if it is still missed, the budget is set from the first
  console measurement with margin, written in the spec and the docs with the measured numbers (the maintainer's policy
  for benches, as in spec 019's AC-7). Per-operation costs are documented. Spec 012's tile gate and the platformer's
  60 Hz replay verdict still pass.
- **AC-8 Opt-in.** `examples/hello` builds with the same executable size as before on PC and 3DS; `examples/sprites`,
  `examples/scenes`, `examples/camera`, `examples/queries` and the platformer's benchmark keep their screenshots
  pixel-identical; every earlier spec's tests pass unchanged. Building a game `.cpp` that includes the world header
  stays within constitution III's budgets (measured before and after on `examples/entities`).
- **AC-9 Docs.** New `docs/api/` pages for pools and for entities/tags (handles and stale use, pools and their two
  forms, quarantine and flush, worlds, components and kinds, destroy and hooks, iteration rules, storage views, tags
  and `tags_distinct`, scene binding, the box-view, animator and tween patterns, determinism, costs); the scenes page's
  attachment section lists the new users; a decision row in `docs/architecture/decisions.md` (the plan's storage and
  handle decision) and the ECS sentence of D11 noted as fulfilled by an optional module; `docs/architecture/patterns.md`'s
  Object Pool row points to the module; CLAUDE.md layout and ROADMAP (three items) updated; `docs/REFERENCES.md` citing
  EnTT, flecs, Bevy, Godot, Unity, *Game Programming Patterns* and the data-oriented sources below.

## Non-goals

- **Prefabs** (a separate ROADMAP item and spec). How they fit: a prefab is a named recipe that spawns an entity and
  adds components with values. FR-10 keeps per-kind metadata (size, alignment, construct, destroy) so a prefab can add
  a component by kind from bytes without templates; values can come from a spec 014 data asset loaded into the
  component struct; a Tiled object's type or a `prefab` property, hashed with FR-16's function, names the prefab. Nothing
  here needs to change for it.
- **Scene tree and transforms** (a separate ROADMAP item and spec). How they fit: parent and child links are a
  component holding handles (`parent`, first child, next sibling), safe because handles detect stale use; local and
  world transforms are components updated by a pass in parent-first order; destroying a parent destroys its children
  from the link component's remove hook, which FR-13 allows (cascades are processed completely, without unbounded
  recursion). Stable slots keep a child's pointer valid while the parent moves.
- **Archetype storage, query caching, `without`/optional filters, change detection, observers, relationships,
  systems and schedulers, multithreading**: `each` takes "has all of these" and the game checks anything else in its
  callable (`has<T>`, `has_tag`). Follow-ups if a game needs them.
- **Growing pools or worlds** (Unity's `ObjectPool` grows to `maxSize`): capacities are fixed at creation (II, V).
- **Engine lifecycle events** (FR-19) and engine-defined components: the engine ships no `Transform`, `Sprite` or
  `Body` component; the game declares its own (the example and the platformer show small ones).
- **Saving or serialising worlds** (save data with versions is its own ROADMAP item), and an inspector of entities
  (the "Live values in the simulator" item).
- **Tags on pools, on tilemap objects or on assets**: tilemap objects keep their `type` and properties, which a game
  turns into tags with FR-16's run-time hash when it spawns entities from them.
- **A world id in handles** or cross-world misuse detection (FR-23).
- **Changing D30**: binding uses its existing notifications only (FR-18).

## Constraints

- **I** — pools and worlds are portable core code with no platform part; the same calls give the same handles and
  orders on PC and 3DS (FR-21); the example and scripts run in the simulator and on the console.
- **II** — one block per world or run-time pool (or none, for the inline form) sized at creation; the plan states the
  bytes per entity, per tag and per component slot (expected: a few bytes of bookkeeping per entity per kind plus
  `sizeof(T)` per component slot; 1024 entities with 8 kinds and 4 tags well under 100 KB besides the components);
  costs measured on an Old 3DS (AC-7).
- **III** — light headers (no standard containers or `<functional>`, the pool header without the world's), templates
  limited to what the type-safe API needs (`get<T>`, `each<A, B>`); `hello` unaffected; build times measured (AC-8).
- **IV** — state is queried; no events published by the engine (FR-19); the game's own events carry handles.
- **V** — no exceptions (`Result` at creation), no RTTI (address-per-type ids, as the event bus), no `virtual`, no
  allocation after creation, no per-frame heap.
- **VI** — pure logic, unit-tested first on the fake platform with a golden checksum (AC-1, AC-2); visible behaviour
  by screenshots and on the console (AC-4 to AC-6).
- **VII** — handles, free bitmap, pool, world block, component storage, destroy/hooks, iteration and tags in small
  separate files; one public header per module (two modules).
- **VIII** — header comments, two `docs/api/` pages, `examples/entities`, the platformer and the scenes page updated.
- **IX** — written from the published designs (EnTT's sparse sets and pointer stability, flecs' and Bevy's documentation,
  *Game Programming Patterns*); no code copied (EnTT is MIT, compatible, but its ideas are enough); credited in
  `docs/REFERENCES.md`.
- **X** — stale handles are detected and safe (FR-2); misuse is loud in Debug/Development and harmless in Release
  (FR-23); tags read from files are only hashed (no new parser).
- **XI, D25** — entirely optional; pools without worlds, worlds without tags, collision/tweens/animation without either
  (FR-20, FR-25); the platformer's benchmark keeps plain code.
- **Under pressure:** **D11** said ECS "was considered and rejected for now ... can be added later as a module": this
  spec adds it **as that optional module**, not as the engine's architecture (events and scenes stay the way modules
  talk); the plan's decision row must say so. **D30** gets two more users with no new notification kind; the plan
  must show that a bound world's clear at exit (hooks run) can't call into a scene that has left (it runs right after
  `on_exit()`, as tween sets' clear does). **FR-2's** "detects stale use" has a documented wrap limit (65 535 reuses of
  one slot), the price of 32-bit handles. **AC-4 step 2** deliberately changes the platformer's golden replay lines; the
  regeneration procedure exists and the console must reproduce the new lines. **AC-7's** targets predate any console
  measurement and fall back to the measurement-with-margin policy.

## Prior art

- **EnTT** (Michele Caini, MIT): entities as 32-bit identifiers with an entity number and a version (20 + 12 bits by
  default) recycled with an incremented version; components in sparse sets, packed by default and with **pointer
  stability** on request (`in_place_delete`, holes left as tombstones and filled on insert) —
  [crash course: entity-component system](https://skypjack.github.io/entt/md_docs_2md_2entity.html),
  [`component_traits`](https://skypjack.github.io/entt/structentt_1_1component__traits.html),
  ["ECS back and forth" part 3](https://skypjack.github.io/2019-05-06-ecs-baf-part-3/) (identifier width and recycling).
  FR-1, FR-12 and the recommended storage take the stable variant.
- **flecs** (Sander Mertens): archetype tables, entities moved between tables when components are added or removed —
  [Building an ECS #2: archetypes and vectorization](https://ajmmertens.medium.com/building-an-ecs-2-archetypes-and-vectorization-fe21690805f9).
  **Bevy**: archetype tables by default and an opt-in sparse-set storage for components added and removed often —
  [component storage](https://bevy-cheatbook.github.io/patterns/component-storage.html).
- **Godot** groups (`add_to_group`, `get_nodes_in_group`, `call_group`) and `queue_free` (deferred deletion) —
  [groups](https://docs.godotengine.org/en/stable/tutorials/scripting/groups.html),
  [`Node.queue_free`](https://docs.godotengine.org/en/stable/classes/class_node.html#class-node-method-queue-free)
  (FR-13, FR-17).
- **Unity** tags ([`CompareTag`](https://docs.unity3d.com/ScriptReference/Component.CompareTag.html),
  [`GameObject.FindGameObjectsWithTag`](https://docs.unity3d.com/ScriptReference/GameObject.FindGameObjectsWithTag.html):
  one tag per object, compared without string allocation) and
  [`ObjectPool<T>`](https://docs.unity3d.com/ScriptReference/Pool.ObjectPool_1-ctor.html) (`Get`/`Release`, a
  `collectionCheck` that catches double releases in the Editor only, growth up to `maxSize`) — FR-3 to FR-5 keep the
  get/release shape, detect double release in every build by generation, and don't grow.
- Robert Nystrom, *Game Programming Patterns*: [Component](https://gameprogrammingpatterns.com/component.html),
  [Object Pool](https://gameprogrammingpatterns.com/object-pool.html) (free list, fixed size, "reuse" bugs),
  [Data Locality](https://gameprogrammingpatterns.com/data-locality.html) (arrays of components iterated in order).
- Richard Fabian, [*Data-Oriented Design*](https://www.dataorienteddesign.com/dodbook/) (existence-based processing,
  tables per kind); Niklas Gray (Bitsquid), [Building a data-oriented entity system](http://bitsquid.blogspot.com/2014/08/building-data-oriented-entity-system.html)
  (index + generation handles over fixed arrays).

## Open questions

Every question has a recommended answer; none is decided until the maintainer approves the spec.

- [x] **One spec for the three ROADMAP items, or three?** — **recommended: one.** Pools are the storage the world is
      built from (same handles, same quarantine), tags are only useful on entities, and the platformer's adoption needs
      all three at once. Pools still ship as their own module (FR-25) so a game can take only them.
- [x] **Storage model.** Options: (a) **archetypes** (flecs, Bevy, Unity DOTS: one table per component combination);
      (b) **packed sparse sets** (EnTT's default: dense arrays per kind, swap-and-pop on remove); (c) **pools of the
      game's whole structs** only (one pool per kind of thing, no components); (d) **stable-slot storage per component
      kind** — each kind a fixed array with a free bitmap and an owner per slot, each entity a slot index per kind
      (EnTT's sparse set with pointer stability, Bevy's sparse-set storage without moves). — **recommended: (d), with
      (c) available on its own as the pool module.** (a) moves every component of an entity when a component is added
      or removed, needs per-combination tables whose sizes can't all be fixed at creation, and costs the most code;
      its strength — iterating many kinds over millions of entities — is not a 2D homebrew game's problem. (b) is the
      fastest to iterate but moves components on every removal, which breaks tween pointer targets, animator callbacks
      and any pointer held during a frame, and makes iteration order depend on the history of removals. (c) alone is
      simplest but can't attach a body to both a turret and a platform, tag across kinds, or give prefabs and the scene
      tree something to build on. (d) keeps (c)'s simplicity per kind, stable addresses, deterministic order, in-place
      box views, and O(1) lookups, at the cost of holes in iteration (bounded by lowest-free reuse and `clear`).
- [x] **Component identity without RTTI** — **recommended: the address-per-type id the event bus already uses
      (`detail::type_id<T>()`), mapped to a registration index in the world's description; never ordered by address**
      (FR-10, FR-21). Lookup cost is a scan of at most 32 ids; the storage view (FR-15) is the way to hoist it out of
      loops. A per-game "component enum" was rejected: it forces one central list and doesn't let a library add kinds.
- [x] **Handle size and split** — **recommended: 32 bits, 16-bit slot + 16-bit generation, 0 = null** (FR-1, FR-2).
      Handles go into events (≤ 32 bytes target) and game structs; 65 535 objects per pool or world is beyond any 3DS
      game. EnTT's 20 + 12 split wraps a slot's version after 4096 reuses (about a minute at one per frame); 16 bits
      gives 18 minutes. 64-bit handles were rejected: twice the size everywhere for a limit games don't reach; the wrap
      limit is documented.
- [x] **Which free slot is reused?** — **recommended: the lowest free one** (FR-4, FR-11, FR-12). It is deterministic,
      keeps the extent (what iteration and box views scan) as small as possible, and matches the platformer's "first
      inactive bullet" so AC-4 step 1 keeps its golden lines. A FIFO free list would spread generations over more slots
      but grow the extent and change the platformer's order.
- [x] **What does destroy do, and when?** Options: (a) Godot's `queue_free` — the object stays whole and visible until
      the end of the frame; (b) immediate — gone at once, slot reusable at once; (c) **immediate for the game, slot
      quarantined until the flush**. — **recommended: (c)** (FR-5, FR-13). Hooks run while the data is still there;
      after that the handle is stale and the slot holds `T{}`, so loops, tag searches and box views of the same frame
      already exclude it (with (a) a coin taken this frame would still be "there" for the next query); quarantine means
      no spawn in the same frame can reuse the slot, so a pointer held during the frame never points at another object
      and loops never visit a newcomer in an old place.
- [x] **When are pools and worlds flushed?** — **recommended: bound ones right before their scene's update through
      D30's existing `BeforeUpdate`; unbound ones by the game** (FR-6, FR-18). A new "after update" notification would
      amend D30 for no gain: flushing at the start of the next update gives the same guarantee (no reuse within a frame).
- [x] **What happens when a pool or world is full?** — **recommended: pools return null and count a miss without
      asserting; worlds return null and assert in Debug/Development** (FR-4, FR-11, FR-12). A bullet pool running dry is
      a design choice (no more than 32 bullets); a world's capacity is a budget the game sets and running out is a bug
      to see on PC first. Stats make both visible.
- [x] **How are tags stored?** Options: (a) up to K tag hashes per entity, no registration; (b) a world-wide registry of
      up to 64 tags and a 64-bit mask per entity; (c) one tag per entity (Unity). — **recommended: (a), K from 0 to 8
      per world, default 4** (FR-16, FR-17). It needs no registration step (tags from Tiled types or data files just
      work), costs 16 bytes per entity at the default, and a search is a few integer compares per entity. (b) is faster
      to test but needs every tag declared up front and a lookup from hash to bit; (c) is too few (`"enemy"` and
      `"flying"`). Layers (spec 019) already give games the bitmask model where speed matters.
- [x] **Tag hash and collisions** — **recommended: the engine's FNV-1a 32 (`detail::fnv1a32`, already shared by data
      paths and sprite names), `consteval`-capable for literals, the same function at run time; collisions ruled out by
      a compile-time `tags_distinct` check over the game's tag constants; no names stored** (FR-16). Storing names for a
      run-time check would put strings in Release binaries (the release-strings check) and cost memory per tag.
- [x] **Lifecycle events on the bus?** — **recommended: none; remove hooks plus the game's own events** (FR-13, FR-19).
      Bus events arrive at the next dispatch with stale handles and would be published for every bullet; hooks run at
      the moment with the data intact.
- [x] **Iteration API** — **recommended: an inlined `each<A, B, ...>(fn)` ordered by the first listed kind, plus
      per-kind storage views with range-for and raw arrays** (FR-15). The callable as a template parameter is free; a
      `SmallFunction` would cost an indirect call per entity. "The smallest storage leads" (EnTT) was rejected: the
      order would change with counts, which replays would see.
- [x] **What may a component be?** — **recommended: any default-constructible type with a non-throwing destructor;
      no trivially-copyable or movable requirement** (FR-10). Objects never move, so a `SpriteAnimator` (with its
      callback) works; `T{}` is the free-slot value that makes box views skip dead slots.
- [x] **One pool form or two?** — **recommended: both, one behaviour** (FR-3): inline `Pool<T, N>` (no allocation, a
      scene member, like the platformer's arrays today) and a run-time capacity in one tagged block (capacities from a
      data asset or settings).
- [x] **Module and header layout** — **recommended: two modules, a pool header and an entities header (names fixed by
      the plan); the world builds on the pool's slot machinery; tags live in the entities module** (FR-25). A game that
      only wants pools doesn't compile the world's templates (III); one header per module (VII).
- [x] **Limits of a world** — **recommended: ≤ 65 535 entities, ≤ 32 component kinds, ≤ 8 tags per entity** (FR-9,
      FR-10). They keep handles at 32 bits, the kind lookup a short scan, and tag search a few compares.
- [x] **Box-view interop** — **recommended: by data layout only (pointer, extent, stride), with `T{}` in non-live
      slots and the documented `kNoLayers` default** (FR-20). Neither module includes the other (XI); the box index is
      the slot and `owner(slot)` gives the entity.
- [x] **Tweens and animators as components** — **recommended: animators are components directly; tween sets stay one
      per scene and target components through a handle-capturing setter (recommended) or a pointer killed by a remove
      hook** (FR-20). A tween set per entity would cost a heap block each.
- [x] **Platformer: what changes, and what happens to the replay checksum?** — **recommended: two steps as specs 018
      and 019** (AC-4). Step 1 moves coins, turrets and bullets into a scene-bound world (and pool) with unchanged golden
      lines, which proves the swap; step 2 adds coin sparkles destroyed from their own callbacks and adds the world's
      live count and handle hash to the checksum, regenerating the lines in the same commit with the console required to
      match. Leaving entity state out of the checksum would hide exactly the spawn/destroy divergence the replay exists
      to catch.
- [x] **Example: new `examples/entities` or only the platformer?** — **recommended: both** (AC-4, AC-5). The platformer
      shows entities in a real game; `examples/entities` shows pools, stale handles, tags, touch-to-destroy with hooks,
      stats, and owns the console self-check and bench.
- [x] **Does this contradict D11?** — **recommended: no; D11 rejected ECS as the engine's architecture "for now" and
      foresaw it "as a module"** (Constraints). Modules and scenes still talk through APIs and the bus; the plan adds a
      decision row for the storage and handle design and a dated note on D11.
- [x] **Old 3DS cost targets** — **recommended: AC-7's five targets, confirmed by the plan, and set from the first
      console measurement with margin if they are still missed after optimisation** (the maintainer's bench policy).
