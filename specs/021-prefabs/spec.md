# Spec 021: Prefabs

Status: implemented 2026-11-23; Linux T9.1 and Old 3DS T10.2 passed (2026-11-23) · approved 2026-11-23 by the maintainer, every
"Decisions to take" item as recommended
Author: NNHGE maintainers · Date: 2026-11-23
Depends on: [spec 020](../020-entities/spec.md) (worlds, component kinds and their `ComponentDesc`, tags, handles,
quarantine; decision D35) and [spec 002](../002-core-foundation/spec.md) (asserts, logs, `Result`, tagged memory,
profiling). Works with [spec 012](../012-tilemaps-and-collision/spec.md)'s map objects and properties (a Tiled object
names a prefab) and [spec 014](../014-data-assets/spec.md)'s data assets (a prefab's values may be loaded from one)
without depending on either. Verified on the console with [spec 013](../013-console-remote/spec.md)'s
`nnh run --script`. Implements the ROADMAP item "Prefabs" (Features in the spirit of Godot and Unity). Optional for
games (constitution XI, decision D25).

## Problem and goal

Spec 020 gave games a world of entities, but every kind of thing is still made by a hand-written function that
repeats the same three steps — `spawn()`, one `add<T>(e, {...})` per component, one `add_tag` per tag — and a level
loader that compares each Tiled object's type with string literals to pick the function (`spawn_coin`,
`TurretBook::spawn_turret`, `Sparkles::burst` in the platformer; `spawn_walker`, `spawn_flyer`, `spawn_coin`,
`spawn_explosion` in `examples/entities`). Adding an enemy means writing a function and a new `strcmp` branch; a
designer can't place a new kind of thing in Tiled, and a kind of enemy whose stats live in a data asset still needs
code to turn the asset into components. Unity (prefabs, prefab variants, instance overrides), Godot (packed scenes,
inherited scenes) and flecs (prefab entities instantiated with `IsA`) all let a game declare a thing **once, by
name**, and make copies of it. After this spec an NNHGE game can register **prefabs** in a small library it owns —
each a named list of component values and tags, an optional **spawn hook** for what values can't say, optionally
**based on** another prefab — and **spawn** one into a world many times, **overriding** components or its position per
copy; a Tiled object names the prefab it stands for and one call spawns a map's objects in map order; values may come
from a spec 014 data asset loaded at registration. A spawned entity is an ordinary entity, bit for bit what the
hand-written code would have made, with no allocation while playing and the same handles on PC and 3DS. A game that
doesn't use prefabs pays nothing, and entities keep working without them.

## User stories

- As a game developer, I want to declare "a coin is a body on the coin layer, a value of 1 and the tag `coin`" once,
  under the name `"coin"`, and spawn it anywhere with one call, so that I stop writing a spawn function per kind.
- As a game developer, I want to spawn a copy with a different position or a different value for one component (a
  gold coin worth 5), without declaring another prefab, so that copies can differ where they need to.
- As a game developer, I want a "flying walker" to be a walker plus a bob component and a `flying` tag, declared as a
  variant of the walker, so that a change to the walker's values reaches every variant I register after it.
- As a game developer, I want a prefab to run my own code once per copy (start a bobbing tween, read a Tiled property,
  spawn a turret's base), so that anything the values can't express still lives with the prefab.
- As a level designer working in Tiled, I want to give an object the type `coin` (or a `prefab` property) and have the
  game spawn that prefab at the object's place, with the object's properties available to the prefab, so that I place
  new things without asking for code.
- As a game developer, I want a mistyped prefab name in a map to be reported loudly with the object's id and name, not
  ignored, so that a missing enemy is found on the PC before the console.
- As a game developer, I want an enemy's stats to come from `enemies/goblin.enemy` (spec 014) and one prefab per
  enemy data asset to be registered in a loop, so that a designer adds an enemy by adding a text file.
- As a game developer, I want a prefab spawn to be all-or-nothing — never a half-built entity when a component's
  storage is full — and to see the misses in stats.
- As a game developer, I want prefab spawns to give the same handles, slots and component bytes as my hand-written
  spawns did, on PC and 3DS, so that replays and console checks still compare exactly.
- As a game developer who writes my own spawn functions (or doesn't use entities), I want my game, its size and its
  build time to stay as they are.
- As a contributor, I want all of it tested without a console, on the fake platform.

## Functional requirements

### Libraries and prefabs

- **FR-1 A prefab library.** Prefabs live in a **library** the game creates and owns (a scene member, or a game-wide
  object), never in a hidden global registry. It is created by a `Result`-returning `create(desc)` in **one block** of
  tagged memory (tag chosen by the game, default an engine tag) sized by its description: the maximum number of
  prefabs (1..1024), of component entries over all prefabs, and the bytes of stored component values. Errors
  (`InvalidArgument`, `OutOfMemory`) are returned. A library is movable, not copyable; its destructor destroys the
  component values it stores. A game may have several libraries; one library may spawn into several worlds.
- **FR-2 A prefab.** A prefab is registered under a **name** and holds, in declaration order: up to 32 **component
  entries** (a kind and a value of the game's component struct — `Body{{0, 0, 12, 12}, kCoin}`, `Coin{1}`), up to 8
  **tags**, an optional **placement member** (FR-7) and an optional **spawn hook** (FR-6). A prefab with no components
  and only a hook is valid (the "a function" form of the ROADMAP item). Registration copies each value into the
  library block (the component type must be copy-constructible — checked at compile time where the prefab is declared;
  spec 020's components in general still need not be) and returns a **prefab handle**. Registering is a load-time
  operation: the only allocation is the library's block at creation; registration past a capacity returns
  `LimitReached` and registers nothing.
- **FR-3 Names and lookup.** A prefab's name is identified by the engine's FNV-1a 32 hash (the hash of tags, data paths
  and sprite names), computed at compile time from a literal or at run time from a string the game read (a Tiled type,
  a data asset name) with the same bits. Registering a name whose hash is already in the library is an error
  (`InvalidArgument`), so a hash collision between two prefabs of a library is always detected. `find(name)` returns the
  prefab handle or a null one; spawning takes a handle, so the per-spawn cost has no search. Release builds store no
  prefab names; Debug and Development builds may keep them for messages (the plan decides; the release-strings check
  must still pass).
- **FR-4 Variants.** A prefab can be registered **based on** an already registered prefab: it starts with the base's
  entries, tags, placement member and hook, then its own entries **replace** the base's entry of the same kind (in the
  base's position) or are **appended**, its tags are added (duplicates ignored, at most 8 in all), and its hook (if any)
  runs **after** the base's. The variant is flattened at registration (a library has no "edit" after registration, so
  there is nothing to propagate later), and spawning a variant costs the same as any prefab. Variants of variants work
  (bounded by the library's capacities).

### Spawning and overrides

- **FR-5 Spawn.** Spawning a prefab into a world creates one entity exactly as the equivalent hand-written code would:
  `spawn()` (lowest free entity slot), then for each entry in prefab order a component of that kind in its storage's
  lowest free slot, copy-constructed from the stored value (or from the copy's override, FR-7), then the placement
  (FR-7), then the tags in prefab order, then the hook (FR-6). It returns the entity's handle. The result — handles,
  slots, component bytes, tags — is identical to calling `spawn`/`add<T>(e, value)`/`add_tag` by hand in that order
  (AC-2 proves it on a golden script).
- **FR-6 Spawn hook.** A prefab's optional hook is called once per copy, after its components, placement and tags are
  in place and before spawn returns, with the world, the new entity and the copy's **spawn info**: the position given to
  the spawn (if any) and, for copies made from a map, the map object (its id, name, type, bounds and properties; FR-9).
  It is a small inline function (captures ≤ 16 bytes, trivially copyable, as spec 020's remove hooks) — the place to
  start a tween, read a Tiled property into a component, or spawn other entities. It may spawn and destroy entities
  (spec 020's rules apply), including the new one (spawn then returns its now stale handle).
- **FR-7 Overrides per copy.** One spawn call may give:
  - **component overrides**: values of component kinds that replace the prefab's value for this copy (the component is
    constructed once, from the override) or, for a kind the prefab doesn't have, add that component to this copy only;
    each kind at most once per call (twice asserts);
  - a **position**: written into the prefab's **placement member**, a `Vec2` or `Rect` member of one of its component
    kinds named at registration by member pointer (as spec 019's box views name `&Body::box`). A `Vec2` member gets the
    position; a `Rect` member gets its `x`/`y` (and, from a rectangle map object, its size unless the prefab keeps its
    own; FR-9). A position given to a prefab without a placement member asserts and is ignored.
  Overrides apply to that copy only; the prefab is unchanged.
- **FR-8 All or nothing.** Before creating anything, a spawn checks that the world has a free entity slot and a free
  slot in every kind the copy needs, and that the world stores every such kind. If not, it creates nothing, returns the
  null handle, counts a **miss** in the library's stats and, like spec 020's full world, asserts in Debug/Development
  (an unregistered kind always asserts; a full kind is the game's budget). Inside a world's `clear()` (spawns refused by
  spec 020) a spawn returns null without asserting or counting. No partial entity is ever left.

### Tiled objects and data assets

- **FR-9 Map objects name prefabs.** A map object (spec 012) names a prefab by its string property `prefab` when it has
  one, otherwise by its **type** (Tiled's class). One call spawns a map's objects into a world **in map object order**
  (optionally only one object layer), each at its bounds through the placement member (a point object gives a
  position; a rectangle object gives its bounds), with the object in the hook's spawn info. It returns a summary:
  spawned, misses (FR-8), objects whose type names no prefab (skipped: doors, spawn points and other objects the game
  reads itself are normal), and objects whose explicit `prefab` property names no prefab — the latter is a **level
  error**: logged with the map object's id, name and the unknown name (`object 12 "coin" (layer things): no prefab
  "coinn"`) and asserting in Debug/Development. A separate check call answers the same report without spawning (for a
  test or a loading screen). The same objects spawned in the same order give the same slots as the first load after a
  world `clear()` (a level restart).
- **FR-10 Values from data assets.** A prefab's component values are plain structs, so a value loaded by
  `data::load` (spec 014) into a component struct (or into a struct the game converts into components) is registered
  like any other value; the documentation shows the pattern, including one prefab per data asset of a type, named by
  the asset's name (`data::name<Enemy>(i)`), registered in a loop at load time. Spawning never reads files. The prefab
  module itself doesn't include or link the data module.

### Queries, determinism, cost, safety, opt-in

- **FR-11 Queries and stats.** A library answers: the number of prefabs, a prefab's component kinds and tags (for
  tests and debug overlays), whether a prefab has a kind, and a read-only view of a prefab's stored value of a kind.
  Stats: prefabs registered and capacity, entries and value bytes used and capacity, spawns, misses, map objects
  skipped and unknown since creation — enough for a debug overlay line (the example shows it).
- **FR-12 No link back, no events.** A spawned entity keeps no hidden link to its prefab (no engine component, no
  automatic tag); a game that wants to know "which prefab" adds a tag or a component of its own in the prefab. The
  engine publishes no event for prefab spawns (constitution IV; spec 020's FR-19); the hook and the game's own events
  cover it.
- **FR-13 Same results everywhere.** Spawning uses only spec 020's deterministic rules plus fixed orders (prefab entry
  order, variant flattening order, map object order); values are copied as constructed by the game (floats copied, not
  recomputed). The same registrations and spawns give the same handles, slots, component bytes and tags bit for bit on
  PC (every supported host compiler) and on the 3DS.
- **FR-14 Bounded cost, no allocation while playing.** After the library's creation nothing allocates; registration
  and spawning call no `virtual` functions (per-kind copy and destroy functions are plain function pointers, called
  once per component copied). A spawn costs at most what the hand-written equivalent costs plus one pass over the
  prefab's entries (the capacity check of FR-8 and a kind lookup per entry); measured on an Old 3DS (AC-7).
- **FR-15 Misuse is loud in Debug, harmless in Release.** Capacities out of range at creation are errors (`Result`);
  registration over capacity or with a duplicate name returns an error and changes nothing; a null or foreign prefab
  handle, an override of the same kind twice, a position without a placement member, a kind the world doesn't store,
  an empty or moved-from library — assert in Debug/Development and do nothing (or answer nothing) in Release; never
  undefined behaviour.
- **FR-16 Optional and light.** Prefabs are an optional module (XI) with one public header that builds on the entities
  header and includes neither tilemap, data, collision, tween nor graphics headers. A game that spawns prefabs only from
  code links no tilemap code; a game that doesn't include the header links nothing new (`examples/hello` unchanged on PC
  and 3DS). Spec 020's public API and behaviour stay as they are (anything the module needs from a world is internal or
  an addition the plan justifies), and no existing test or golden value changes.

## Acceptance criteria

- **AC-1 Unit tests first** (constitution VI), on the fake platform: library creation errors and capacities;
  registration (entries in order, tags, duplicate names and hashes, `LimitReached` leaves nothing registered, a type
  with a destructor — constructions and destructions of stored values balance); compile-time and run-time names give
  the same handle; variants (replace in place, append, tag union and limit, hook order base-then-variant, variant of a
  variant); spawn order and equivalence with hand-written calls; component overrides (replace, add a kind, twice
  asserts); placement into `Vec2` and `Rect` members (point and rectangle objects, keep-size option); spawn hooks (spawn
  info, spawning and destroying from the hook, destroying the new entity); all-or-nothing on a full world and a full
  kind (no partial entity, miss counted, assert), an unregistered kind, spawns inside `clear()`; map objects (by
  `prefab` property and by type, layer filter, map order, skipped types, unknown explicit names reported and asserting,
  the check call, restart gives the first load's slots); spawning from a remove hook and inside `each`; queries and
  stats; misuse cases (FR-15). The Linux build VM runs them with GCC 13/14 and clang 18 and with ASan/UBSan, as
  previous specs did.
- **AC-2 Golden equivalence.** A test runs a fixed, seeded script of 50 000 operations (the engine's integer hash, no
  `rand`) over a library of 16 prefabs (with variants, hooks and placement members) and two worlds of 1024 entities: one
  filled by prefab spawns with overrides, the other by the equivalent hand-written `spawn`/`add`/`add_tag` calls,
  interleaved with destroys, flushes and clears. After every step both worlds return the same handles, and a checksum
  of every handle, slot, component byte and tag is equal between them and equal to a golden value; the same test passes
  on every host compiler and in the console self-check (AC-6).
- **AC-3 Tiled and data assets.** Test maps in `tests/data/maps/` (converted by `nnh-assetc`) with objects naming
  prefabs by type and by `prefab` property, a skipped type and a misspelt `prefab` property: the spawn call's summary,
  the logged level error with the object's id and name, and the spawned entities' positions and hook-read properties
  are checked. A data asset fixture registered as a prefab value spawns components equal to `data::load`'s struct.
- **AC-4 The platformer adopts prefabs.** Level 1 and the bonus level register `coin` and `turret` prefabs (body with a
  placement member, coin value or turret state, tag) and spawn the map's coins and turrets with the map-object call (the
  coin's hook reads its `value` property); the coin sparkles are a `spark` prefab spawned with a velocity override,
  whose hook starts the fade tween that destroys it. The hand-written `strcmp` dispatch in the level loader and the
  per-kind spawn functions go away (doors and the spawn point stay game code: they are skipped types). Because FR-5
  spawns exactly what the hand-written code did, the replay's checksum lines in `examples/platformer/tests/level.nnhs`
  and the ctest `platformer_replay_matches_script` pass **unchanged** — no golden line is regenerated; if the plan finds
  a byte that must differ, it says why before changing one. The benchmark, zoom and flip demos are untouched.
- **AC-5 Example `examples/prefabs`** (original art or shapes only), modes chosen by `PREFABS_DEMO` (`showcase`
  (default) | `check` | `bench`): the top screen shows a small room made in Tiled whose objects name prefabs — walkers,
  a `flying_walker` variant, coins, a gold coin made by a property override, and a door the game reads itself (skipped)
  — and a roster of enemies registered from data assets (`assets/enemies/*.enemy`, one prefab each); the HUD shows the
  library's stats and the last map summary. On the bottom screen a **touch** spawns the selected prefab at the touched
  world point (placement member), buttons cycle the selected prefab, and a debug line lists the selected prefab's kinds
  and tags. PC screenshots for the room, a variant, an override and a full-kind miss, checked by reading the PNGs.
- **AC-6 Console script.** The `check` mode runs AC-2's script on the engine's real libraries and worlds and logs
  `prefabs: <n> sum <checksum>` every 10 000 operations and `prefabs: PASS <checksum>` or `prefabs: FAIL ...` at the end,
  against golden values recorded from the simulator. `examples/prefabs/tests/check.nnhs` starts it, expects every line,
  then switches to the showcase, expects the room's map summary line (for example `map: spawned 14 skipped 1 unknown 0
  misses 0`), **touches** fixed points on the bottom screen and expects the logged spawns (for example
  `spawned: flying_walker slot 9 gen 1 at 120,64 tags enemy,flying`), and takes screenshots. On an Old 3DS the
  maintainer only opens the Homebrew Launcher and presses Y, and
  `nnh run prefabs --script examples/prefabs/tests/check.nnhs` passes; the same script passes with `--pc`. The verdict,
  log and screenshots are in the saved log folder.
- **AC-7 Cost on the console.** The `bench` mode, measured on an Old 3DS with the profiler and logged as
  `prefabs bench: PASS <numbers>` (or FAIL). **Targets** (the plan confirms them before code; they predate any console
  measurement):
  - 256 spawns of a prefab with 3 components (one a 64-byte struct) and 2 tags, plus 256 destroys and one flush:
    **under 2 ms**, and **at most 1.25×** the same workload written by hand with spec 020's calls, measured in the same
    run (spec 020's budget for the hand-written workload is 1.6 ms);
  - the same 256 spawns with a component override and a position each: **under 2.2 ms**;
  - spawning 512 map objects (two prefabs, a hook reading one property each) into an empty world: **under 4 ms** (a
    load-time operation, measured so level loads stay inside a frame or two);
  - 1000 `find` calls by run-time name over a library of 128 prefabs: **under 0.5 ms**.
  If a target is missed, the implementation is optimised first; if it is still missed, the budget is set from the first
  console measurement with margin and written in the spec and the docs with the measured numbers (the maintainer's
  policy for benches, as in specs 019 and 020). Spec 012's tile gate, spec 020's bench and the platformer's 60 Hz
  replay verdict still pass.
  **Budgets set (2026-11-23).** The first console run missed map (5.57 ms) and find (0.67 ms); after the optimisation
  round ([plan 021](plan.md#optimisation-round-t103)) the Old 3DS measured spawn 1.626 ms (ratio 1.07), overrides
  1.811 ms, map 3.562 ms and find 0.500 ms. The budgets, from that measurement with ~25 % margin: spawn **under 2 ms**
  and ≤ 1.25× (unchanged), overrides **under 2.3 ms**, map **under 4.5 ms**, find **under 0.65 ms**. A second run
  with these budgets passed.
- **AC-8 Opt-in.** `examples/hello` builds with the same executable size as before on PC and 3DS;
  `examples/entities` (which keeps its hand-written spawns as the "without prefabs" reference), `examples/sprites`,
  `examples/scenes`, `examples/camera` and `examples/queries` keep their screenshots pixel-identical and their scripts
  passing; every earlier spec's tests pass unchanged. A game that spawns prefabs only from code links no tilemap code
  (checked on the 3DS build's map file or symbol list). Building a game `.cpp` that includes the prefab header stays
  within constitution III's budgets (measured before and after on `examples/prefabs`).
- **AC-9 Docs.** A new `docs/api/` page for prefabs (libraries and capacities, registering, names and hashes, variants,
  spawning and its equivalence with hand-written calls, overrides and placement, hooks and spawn info, all-or-nothing,
  map objects and the level error, the data-asset pattern, determinism, costs, memory); `docs/api/entities.md` links it
  from its spawning section; `docs/guides/levels.md` gains "Placing prefabs in Tiled" (types, the `prefab` property,
  what is skipped, the error message); `docs/api/data.md` links the data-asset pattern; a decision row in
  `docs/architecture/decisions.md` if the plan takes one; CLAUDE.md layout and ROADMAP updated; `docs/REFERENCES.md`
  citing Unity, Godot, flecs, Bevy and Tiled (below).

## Non-goals

- **Nested and multi-entity prefabs** (Unity nested prefabs, Godot packed scenes with child nodes): parent and child
  entities belong to the "Scene tree and transforms" ROADMAP item. Until then a prefab's hook may spawn other entities
  (a turret's base) and keep their handles in a component of its own; the scene tree spec can add "children" to a
  prefab without changing this one.
- **Shared (inherited) components** (flecs' `(OnInstantiate, Inherit)`): every copy owns its components. Data shared by
  every copy is the game's own (a `data::Ref<T>`, a pointer to a table, a constant).
- **A live link between a prefab and its copies** (Unity propagates prefab edits to instances in the editor): there is
  no editor and registration happens at load; copies are independent entities (FR-12). Hot-reloading prefabs is out.
- **Field-by-name overrides from Tiled properties** (a property `Coin.value = 5` written into the struct without code):
  properties reach components through the hook (FR-6). A follow-up could map properties onto fields of components that
  are also spec 014 data structs (whose field tables exist at run time); see the open question.
- **A prefab file format** (`*.prefab` text listing components): prefabs are declared in C++ (XI, no editor); values
  may come from spec 014 data assets (FR-10).
- **Tiled object templates** (`.tx`): the converter keeps refusing them (spec 012); objects name prefabs by type or by
  the `prefab` property instead.
- **Build-time checking that every map object's prefab exists**: prefabs are registered in code at run time, so the
  build can't know their names; the check is at load (FR-9), loud, and covered by the example's console script. See the
  open question.
- **Saving, serialising or inspecting** prefabs or spawned worlds (the "Save data with versions" and "Live values in the
  simulator" items).
- **Unregistering or editing a registered prefab**: a library only grows until it is destroyed.
- **Pools** (spec 020's `Pool<T>`): pools hold one kind of struct and are filled by `acquire(value)`, already a copy of a
  template value; prefabs are for worlds.

## Constraints

- **I** — portable core code with no platform part; the same registrations and spawns give the same entities on PC and
  3DS (FR-13); the example and scripts run in the simulator and on the console.
- **II** — one block per library sized at creation; the plan states the bytes per prefab, per entry and per stored value
  (expected: a few tens of bytes per prefab plus 16 per entry plus the values; 128 prefabs with 4 components of 16
  bytes each about 26 KB on the 3DS, as the plan computes); costs measured on an Old 3DS (AC-7).
- **III** — one light header over the entities header (no standard containers or `<functional>`), templates limited to
  registration and typed overrides; `hello` unaffected; build times measured (AC-8).
- **IV** — no engine events for spawns (FR-12); the game's events and hooks cover them; state is queried.
- **V** — no exceptions (`Result` at creation and registration), no RTTI (spec 020's address-per-type kind ids), no
  `virtual` (per-kind copy/destroy function pointers), no allocation after the library's creation, no per-frame heap.
- **VI** — pure logic, unit-tested first on the fake platform with a golden equivalence checksum (AC-1, AC-2); visible
  behaviour by screenshots and on the console (AC-4 to AC-6).
- **VII** — library block, registration and variants, spawn and overrides, map objects in small separate files; one
  public header for the module.
- **VIII** — header comments, a `docs/api/` page, the levels guide, `examples/prefabs` and the platformer updated.
- **IX** — written from the published designs (Unity's and Godot's manuals, flecs' prefab manual, Bevy's bundles); no
  code copied; credited in `docs/REFERENCES.md`.
- **X** — no new parser: map objects and properties are already parsed and checked by spec 012's converter and loader,
  data assets by spec 014's; an unknown prefab named explicitly by a map is reported loudly (FR-9); misuse is loud in
  Debug/Development and harmless in Release (FR-15).
- **XI, D25** — optional: entities without prefabs (examples/entities keeps hand-written spawns), prefabs without
  tilemaps or data assets (FR-16), maps whose objects the game reads itself (skipped types).
- **Under pressure:** **XI's "files ... checked and converted at build time"** — a Tiled object's prefab name can only be
  resolved at run time, because prefabs are declared in C++ at run time; FR-9 makes an unknown explicit name a loud level
  error at load and AC-6 covers it on the console, but it is not a build error (see the open question). **Spec 020's
  non-goal** said "nothing here needs to change" for prefabs; FR-8's all-or-nothing check and FR-5's spawn by kind need
  a free-slot answer per kind and an "add by kind from bytes" step — the plan must provide them as internals or justified
  additions without changing spec 020's public behaviour or golden values (FR-16). **FR-2's copy-constructible rule**
  narrows "any component" for prefab entries only (a `SpriteAnimator` with a callback is copyable; a type that isn't
  can still be added by the hook). **AC-7's** targets predate any console measurement and fall back to the
  measurement-with-margin policy.

## Prior art

- **Unity** prefabs: an asset that stores a GameObject with its components and values, instantiated many times;
  **instance overrides** change property values or add components per instance; **prefab variants** inherit a base and
  override some of it; nested prefabs —
  [Prefabs](https://docs.unity3d.com/Manual/Prefabs.html),
  [Instance overrides](https://docs.unity3d.com/Manual/PrefabInstanceOverrides.html),
  [Prefab variants](https://docs.unity3d.com/Manual/PrefabVariants.html). FR-4 and FR-7 keep variants and per-copy
  overrides, without the editor's live link.
- **Godot** packed scenes: a saved node tree instantiated with `PackedScene.instantiate()`, with inherited scenes as
  variants — [`PackedScene`](https://docs.godotengine.org/en/stable/classes/class_packedscene.html),
  [Creating instances](https://docs.godotengine.org/en/stable/getting_started/step_by_step/instancing.html). Children
  wait for the scene tree item (Non-goals).
- **flecs** prefabs: a prefab is an entity, instances are created with an `IsA` pair; components are copied to the
  instance by default (`OnInstantiate, Override`) or shared with `Inherit`; prefab variants inherit prefabs —
  [Prefabs manual](https://github.com/SanderMertens/flecs/blob/master/docs/PrefabsManual.md),
  [Relationships manual (IsA)](https://github.com/SanderMertens/flecs/blob/master/docs/Relationships.md). FR-5 takes
  the copy-by-default behaviour only.
- **Bevy** bundles: a typed group of components inserted together on spawn —
  [Bundles](https://bevy-cheatbook.github.io/programming/bundle.html). The code-declared, value-based shape of FR-2.
- **Tiled** object classes and custom properties, the designer-facing side of FR-9 —
  [Custom properties and classes](https://doc.mapeditor.org/en/stable/manual/custom-properties/); object templates
  (not supported, Non-goals) — [Using templates](https://doc.mapeditor.org/en/stable/manual/using-templates/).
- Robert Nystrom, *Game Programming Patterns*: [Prototype](https://gameprogrammingpatterns.com/prototype.html) (spawning
  by copying a prototype, and data-driven prototypes with a base to inherit from — FR-4, FR-10).

## Decisions to take

Every question has a recommended answer; none is decided until the maintainer approves the spec.

- [ ] **What is a prefab?** Options: (a) only a function the library calls by name; (b) only a declarative list of
      component values and tags; (c) **a declarative list plus an optional spawn hook**. — **recommended: (c)** (FR-2,
      FR-6). (a) adds little to a plain C++ function besides the name, and can't support variants, per-copy component
      overrides, introspection or the equivalence test; (b) can't start a tween or read a property. (c) gives the common
      case without code and keeps an escape hatch; a hook-only prefab still covers (a).
- [ ] **Where do prefabs live?** Options: (a) **a library object the game creates, one block with fixed capacities**;
      (b) a global registry filled by static registration macros (like `NNH_DATA_TYPE`). — **recommended: (a)** (FR-1).
      No hidden global (architecture anti-pattern), capacities known, values loadable from data assets at registration
      (static registration runs before assets exist), several libraries possible (one per level), and tests create
      their own.
- [ ] **How are prefabs named and found?** — **recommended: the FNV-1a 32 hash of the name (as tags), duplicate hashes
      refused at registration, `find(name)` returning a handle, spawns by handle, no names stored in Release** (FR-3).
      Because every prefab of a library is registered in one place, collisions are caught automatically (unlike tags);
      spawns by handle never search.
- [ ] **Variants?** Options: (a) none; (b) **flattened at registration from an already registered base**; (c) a live
      `IsA` link resolved at spawn (flecs). — **recommended: (b)** (FR-4). It gives Unity's variants and Godot's
      inherited scenes at no spawn cost; (c) needs a base lookup per spawn and buys nothing without run-time edits.
- [ ] **How are copies overridden?** Options: (a) only by writing to the entity after spawn; (b) **whole-component
      overrides at spawn, a position through a placement member, and the hook**; (c) field-by-name overrides (strings
      or member paths). — **recommended: (b)** (FR-6, FR-7). Constructing the overridden component once keeps FR-5's
      equivalence and avoids a copy then a rewrite; the placement member makes "spawn at" work for map objects and
      touches without code; (a) remains possible anyway. (c) is the follow-up below.
- [ ] **Field-by-name overrides from Tiled properties?** Options: (a) **not now: the hook reads properties**; (b) map a
      property `Kind.field` onto a component that is also a spec 014 data struct, using its run-time field table. —
      **recommended: (a)** (Non-goals). (b) couples prefabs to data assets (XI) and can't be checked at build time
      either; one line per property in a hook is cheap. Revisit when a game has many tunable properties per object.
- [ ] **What does a Tiled object use to name its prefab?** Options: (a) its type (class) only; (b) a `prefab` string
      property only; (c) **the `prefab` property when present, else the type; unknown types skipped and reported,
      unknown explicit names a loud level error**. — **recommended: (c)** (FR-9). Types are what designers already set
      (the platformer's `coin` and `turret`); doors and spawn points must be skippable; the explicit property is how a
      designer says "this must be a prefab", so a typo there is an error, not a skip.
- [ ] **Can the build check map objects' prefab names?** Options: (a) **no: a loud check at load, a check call, and the
      example's console script**; (b) declare prefab names statically in the game's `DATA_SCHEMA` sources so
      `nnh-assetc` checks maps against them. — **recommended: (a)** (FR-9, Constraints). (b) makes prefabs depend on the
      data pipeline and a second, static declaration of every prefab; a follow-up if games ship levels with typos.
- [ ] **Do prefab values come from data assets?** Options: (a) **yes, by pattern: the game loads a data asset into a
      struct and registers it as a value; one prefab per data asset in a loop**; (b) a prefab defined entirely in a data
      file. — **recommended: (a)** (FR-10). Zero coupling, no new format, no file reads while spawning; (b) needs a
      heterogeneous component file format and a schema for every component kind.
- [ ] **Placement: which member, and the size of rectangle objects?** — **recommended: one `Vec2` or `Rect` member of
      one component kind, named by member pointer at registration; a rectangle object's size wins unless the prefab
      says it keeps its own** (FR-7, FR-9). The engine never names the game's position component (spec 020 ships none);
      the platformer's objects are sized in Tiled, a touch spawn is a point.
- [ ] **What if the world is full?** — **recommended: all or nothing, null handle, a miss in stats, an assert in
      Debug/Development as spec 020's full world** (FR-8). A half-built coin (body, no value) would be found by queries
      and is worse than none.
- [ ] **Should a spawned entity remember its prefab?** Options: (a) **no link; the game adds a tag or component if it
      wants one**; (b) an automatic tag with the prefab's name; (c) an engine component. — **recommended: (a)** (FR-12).
      (b) spends one of the entity's tag slots silently, (c) adds memory to every world; both break FR-5's equivalence.
- [ ] **Nested and multi-entity prefabs?** — **recommended: not in this spec; hooks may spawn other entities, and the
      scene tree spec adds children** (Non-goals). Parent links need that spec's component and transform pass.
- [ ] **Component requirements.** — **recommended: copy-constructible for prefab entries only, checked at compile
      time where the entry is declared** (FR-2). Spec 020's components need not be copyable; prefab values must be
      copied once per spawn.
- [ ] **Module and header** — **recommended: one optional module and public header on top of the entities header,
      forward-declaring the tilemap types so code-only users link no tilemap code; spec 020's public API unchanged**
      (FR-16). Names (header, `PrefabLibrary`, `Prefab`, spawn call, map-object call) are fixed by the plan.
- [ ] **Library capacities** — **recommended: prefabs 1..1024, entries and value bytes chosen at creation, ≤ 32 entries
      and ≤ 8 tags per prefab** (FR-1, FR-2). They match spec 020's 32 kinds and 8 tags per entity.
- [ ] **Example: new `examples/prefabs`, `examples/entities` converted, or only the platformer?** — **recommended: a new
      `examples/prefabs` plus the platformer; `examples/entities` keeps its hand-written spawns** (AC-4, AC-5, AC-8).
      The new example shows Tiled, data-asset rosters, variants, overrides and touch spawns, and owns the console
      self-check and bench; the entities example stays the "without prefabs" reference (XI).
- [ ] **Platformer: what happens to the replay checksum?** — **recommended: unchanged** (AC-4). FR-5's equivalence
      means the same slots and bytes, so the golden lines prove the swap; nothing visible is added this time.
- [ ] **Old 3DS cost targets** — **recommended: AC-7's four targets, confirmed by the plan, and set from the first
      console measurement with margin if they are still missed after optimisation** (the maintainer's bench policy).
