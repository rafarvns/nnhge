# Spec 022: Scene tree and transforms

Status: implemented 2026-11-24, Linux T9.1 passed, console T10.2 pending (approved 2026-11-23, overnight pre-approval)
to review in the morning, especially decision 24, the platformer's replay lines)
Author: NNHGE maintainers · Date: 2026-11-23
Depends on: [spec 020](../020-entities/spec.md) (worlds, component kinds and their remove hooks, handles, the
destroy queue, quarantine and scene binding; decision D35) and [spec 002](../002-core-foundation/spec.md) (asserts,
logs, `Result`, tagged memory, profiling). Extends [spec 021](../021-prefabs/spec.md)'s prefabs with children (a prefab
spawns a small hierarchy all-or-nothing; decision D36). Works with [spec 004](../004-2d-graphics/spec.md)'s draw options
(rotation, scale, flips) and [spec 018](../018-camera-helpers/spec.md)'s zoom and camera rig, [spec 019](../019-collision-layers-and-queries/spec.md)'s
box views, [spec 016](../016-sprite-animation/spec.md)'s animators and [spec 015](../015-tweens-and-timers/spec.md)'s
tweens, without depending on any of them. Shares spec 015's libm-free sine (FR-3). Verified on the console with
[spec 013](../013-console-remote/spec.md)'s `nnh run --script`. Implements the ROADMAP item "Scene tree and transforms"
(Features in the spirit of Godot and Unity). Optional for games (constitution XI, decision D25).

> Naming: "scene tree" is the ROADMAP's (Godot's) word. In NNHGE a **scene** is a game mode on the scene stack
> (spec 001), so this spec says **hierarchy**, **node** and **transform**; the public names avoid "scene" (see
> Decisions to take).

## Problem and goal

Spec 020 gave games entities and spec 021 gave them prefabs, but every entity is a free-standing thing: its position is
whatever `Rect` or `Vec2` the game keeps in its own component. A gun on a ship, a hat on a head, a sword in a hand, the
wheels of a cart or a turret's barrel must be moved by hand every frame — "gun position = ship position + offset
rotated by the ship's angle and scaled by its size" — with `sin`/`cos` from the C library (different bits on the PC
and the 3DS, so replays drift), repeated for every attached thing, and with nothing that destroys the gun when the ship
explodes or keeps the hat where it is when the character drops it. Godot (`Node2D`, `Transform2D`, `reparent`), Unity
(`Transform`, `SetParent(worldPositionStays)`, destroying a parent destroys its children) and the ECS libraries (Bevy's
`Transform`/`GlobalTransform` with a propagation pass, flecs' `ChildOf` with cascading deletes, EnTT's hierarchy
patterns) all give games **parent and child** objects whose **local** position, rotation and scale follow the parent.
After this spec an NNHGE game can make an entity a **child** of another in the same world, give it a **local
transform** (position, rotation, scale relative to its parent), have the engine compute every node's **world
transform** in one pass per frame with the **same bits on PC and 3DS**, ask for a fresh world transform on demand,
**reparent** a node keeping where it is on screen, have **children destroyed with their parent** (or detach them
first), draw a node's sprite, place its collision box and point the camera at it from its world transform, and spawn a
whole small hierarchy (ship + gun + muzzle) from one **prefab** all-or-nothing. The transform math itself is a plain
value usable without any world. No allocation while playing; a game that uses none of it pays nothing.

## User stories

- As a game developer, I want to put a gun on my ship at a local offset and angle, rotate and move the ship, and see
  the gun follow — turned, scaled and mirrored with it — without writing the trigonometry myself.
- As a game developer, I want to aim the gun on its own (its local rotation) while it rides on the ship, and fire
  bullets from its muzzle's world point, so that the bullet leaves the barrel wherever the ship faces.
- As a game developer, I want a character facing left (mirrored with a negative scale) to carry its sword and hat on
  the mirrored side, with their rotations mirrored too, so that turning around is one value.
- As a game developer, I want destroying the ship to destroy its gun and muzzle — each running its own remove hooks
  where it was — so that nothing is left floating; and I want to detach a child first when I mean to keep it (a rider
  jumps off a destroyed cart).
- As a game developer, I want to pick up an item (attach it to the hand) and drop it (detach it) without it jumping on
  screen, so that reparenting keeps the world position, rotation and scale.
- As a game developer, I want world transforms computed in one pass I call once per frame, in a fixed order, and a
  fresh value on demand when I need one mid-frame (spawning a bullet right after turning the gun).
- As a game developer, I want a child's collision box to follow it (an axis-aligned box that encloses it when it is
  rotated), readable by spec 019's queries in place, and my camera to follow a child (the rider, not the cart).
- As a game developer, I want a prefab "ship" whose children are a "gun" with a "muzzle", spawned together in one call
  — all of it or none of it — from code or from a Tiled object, so that a hierarchy is declared once.
- As a game developer, I want the same calls to give the same world transforms bit for bit on PC and 3DS, so that
  replays and console checks still compare exactly.
- As a game developer who keeps my own arrays (or uses entities without hierarchies), I want the transform math on my
  own structs, and my game, its size and its build time to stay as they are when I don't use the hierarchy.
- As a contributor, I want all of it tested without a console, on the fake platform.

## Functional requirements

### Transforms (values, usable without a world)

- **FR-1 A transform value.** A **transform** is a plain, trivially copyable value: a **position** (`Vec2`, pixels),
  a **rotation** (radians, clockwise on the y-down screen — the unit and direction of `DrawOptions::rotation`) and a
  **scale** (`Vec2`, default (1, 1); a negative component mirrors on that axis). Its default value is the identity. A
  **local** transform says where a node is relative to its parent (relative to the world for a root): the node is
  scaled, then rotated, then moved, about its own origin.
- **FR-2 Composition, points and inverse.** The engine composes a parent's world transform with a child's local
  transform into the child's **world transform**, maps a local **point** (a muzzle, a box corner) and a local
  **direction** (a velocity) to world space, and maps a world point back to local space. A world transform answers:
  - its **affine map**, exact for points and directions even when a non-uniform scale on a parent meets a rotated child
    (the shear such a chain produces, Godot's `Transform2D`): positions of every descendant are always right;
  - its **position**, **rotation** and **scale** for drawing (FR-13): exact when the chain has no shear (every
    non-uniform scale sits on a node whose descendants are not rotated relative to it — the usual case: uniform scales,
    or a mirrored character); with shear, rotation and scale are the documented approximation (accumulated rotation,
    product of scales, as Unity's `lossyScale`) while points stay exact.
  These operations are free functions or members of the value (names fixed by the plan) that need no world, no
  `nnh::init()` and no allocation, so a game with its own arrays composes its own structs (constitution XI).
- **FR-3 Same bits everywhere.** Sine and cosine are computed by the engine with IEEE `+ − ×` and float↔int conversions
  only — the libm-free sine of spec 015's easings, shared without making transforms depend on the tween module — under
  core's `-ffp-contract=off`; every composition, mapping and inverse is a fixed sequence of float operations. The same
  inputs give the same bits on PC (every supported host compiler) and on the 3DS. Exactness guarantees:
  - rotation 0 and scale (1, 1) are an exact identity: a child of an unrotated, unscaled parent sits at exactly
    `parent position + local position` (one float addition per axis), and a root's world transform equals its local
    transform bit for bit;
  - whole quarter turns given with the engine's angle constants (a quarter, half and three-quarter turn) have sine and
    cosine of exactly 0 and ±1, so a child turned 90° lands on the same whole pixels as one written by hand;
  - against a `double` reference, world positions are within **1/32 pixel** for hierarchies up to the depth limit
    (FR-6), coordinates within ±4096 px and scales between 1/4 and 4 (the plan confirms the bound; AC-3 tests it).
  The engine's deterministic sine and cosine of an angle are public (a game aiming in 8 directions gets the same bits
  as the hierarchy; name fixed by the plan). Pixels drawn for a rotated sprite remain each backend's rasterisation (as
  today, spec 004): determinism covers transform values, positions and everything the game reads, not rotated texels.
- **FR-4 Mirroring.** A negative scale mirrors the node and its whole subtree: children's positions are mirrored, their
  rotations turn the other way on screen, and their drawing scale carries the flip, so a character facing left is one
  `scale.x = −1` on its root. `DrawOptions::flip_x`/`flip_y` stay per-sprite and are not inherited.

### The hierarchy

- **FR-5 Nodes are entities with the engine's node component.** An entity takes part in a hierarchy through a **node**
  component — the one component kind the engine ships (spec 020 shipped none and deferred this one here) — registered by
  the game in its world description with a capacity like any kind (a helper gives its `ComponentDesc`; the game may add
  its own remove hook to it, run before the engine's). A node holds the game-writable **local transform** (plain data:
  a tween may target it through a setter or by pointer, a prefab's placement member may name its position), the
  **world transform** last computed for it (read-only to the game), and its links (parent, children; private).
  Entities without a node, and worlds without the kind, are untouched and pay nothing.
- **FR-6 Parent and children.** `attach(child, parent, mode)` makes a node a child of another node of the **same world**
  (handles carry no world id: documented, as spec 020's FR-23), appended **after** the parent's existing children;
  `detach(child, mode)` makes it a root. `mode` is **keep world** (FR-10) or **keep local** (the local transform is
  kept, so the node jumps to its place under the new parent); the default is fixed by the plan from the decision below.
  Attaching to the current parent changes nothing. Refused — returning false, asserting in Debug/Development and
  changing nothing — are: a stale or null handle, an entity without a node, a node made its own ancestor (a cycle), a
  result deeper than the **depth limit** (16 levels, root = depth 0; the plan confirms the number), and keep-world under
  a parent whose world map can't be inverted (a zero scale). Children keep their attach order; it is the order of every
  child iteration, the pass and the cascade.
- **FR-7 Hierarchy queries.** `parent(e)` and `depth(e)` in constant time; `root(e)`; the number of children; the
  children of a node in order (a range and a first-child/next-sibling walk); `is_ancestor(a, e)`; and
  `each_descendant(e, fn)` visiting a subtree depth-first in child order (pre-order) with spec 020's iteration rules
  (destroying inside is safe; nodes created during it are not visited). Links never name a stale entity: a node is
  unlinked from its parent's children at the moment it is destroyed, detached or loses its node component.
- **FR-8 The world-transform pass.** One call computes the world transform of every node of a world: roots in
  **ascending slot order** of the node storage, each subtree **depth-first in child order**, each node exactly once from
  its parent's freshly computed world transform and its current local transform. It is a plain call the game makes
  where it wants — normally once per frame after moving things and before the camera and drawing — and is not run by
  scene binding (no new D30 notification; see Decisions to take). After it, reading a node's world transform is a load.
  Between passes a node's stored world transform is the one from the last pass, or — for a node created or attached
  since — the one computed at that moment, never uninitialised. Inside the pass the game's code is never called.
- **FR-9 Fresh values on demand.** `world_now(e)` (name fixed by the plan) computes a node's world transform from the
  current local transforms of its ancestors, without storing anything, **bit for bit** what a pass run at that moment
  would store; mapping a local point through it is how a bullet leaves a muzzle in the frame the gun turned.
- **FR-10 Reparenting that keeps the world transform.** In keep-world mode (attach, detach, FR-11's detach of children)
  the node's new local transform is computed so that its world transform is unchanged: the position within FR-3's
  bound; rotation and scale exactly when the old and new chains have no shear (otherwise the documented
  approximation, FR-2). A held item attached to a hand and later dropped stays where it was on screen at both moments.
  Its descendants keep their local transforms and therefore their world transforms.
- **FR-11 Destroying a parent destroys its children.** Destroying an entity with a node destroys every descendant
  (spec 020's destroy: each runs its own remove hooks with its components intact, including its node and last world
  transform, so "an explosion where the gun was" works), through spec 020's destroy queue — no recursion, in a fixed
  documented order (the plan fixes it; breadth-first in child order is what the queue gives). A child's hooks run after
  its parent's; by then the parent is stale and `parent(child)` answers null. To keep children, the game detaches them
  first (`detach_children(e, mode)`, keep world by default). A world's `clear()` destroys every node as spec 020 says
  (cascades merge into it; slots after a clear and the same spawns are the first load's). **Removing** the node
  component from an entity detaches it from its parent and makes its children roots that keep their world transforms
  (it does not destroy them).
- **FR-12 Scenes, flush and quarantine.** Nothing new: nodes are components of a world, so a world bound to a scene
  (spec 020's FR-18, D30) flushes and clears its nodes with it; node slots are quarantined until the flush like any
  component's. The pass is the game's call (FR-8).

### Drawing, collision, camera

- **FR-13 Drawing a node.** A helper turns a node's world transform into what `gfx::draw` takes: the position, and the
  rotation and scale in `DrawOptions` (a mirror as a negative scale), merged with options the game or an animator's
  `draw_options()` already filled (region, pivot, flips, tint, layer: kept; a world scale multiplies the given scale; a
  world rotation adds to the given rotation). Camera zoom applies as to any draw (D33). The engine ships no sprite
  component: the game draws its nodes in its own loop (`each<Node, Sprite>`).
- **FR-14 Collision boxes that follow.** A query returns the **axis-aligned bounds** of a local rectangle under a world
  transform: the mapped rectangle itself when the node is unrotated (exact, mirrored rectangles normalised to a
  positive size), the box enclosing its four mapped corners when it is rotated. The documented pattern writes it into
  the game's body component after the pass (`each<Node, Body>`), so spec 019's box views and queries read children's
  boxes in place. Oriented boxes are not added (Non-goals). A root whose body is moved by `collision::move` copies its
  box position into its local position before the pass (the documented pattern for "physics drives the root, the
  hierarchy drives the children").
- **FR-15 The camera follows a node.** A node's world position (after the pass, or `world_now`) is a point a
  `CameraRig` follows with today's API; the documented order is game update → pass → camera rig → draw. No camera
  change.

### Prefabs with children

- **FR-16 A prefab may have children.** A prefab (spec 021) may list up to 8 **child entries**, each an already
  registered prefab of the same library with an optional local transform that replaces the child prefab's node local
  for that slot in the hierarchy (a gun at (12, −4) turned 0.1 rad). The parent and every child prefab must have the
  node kind among their components (checked at registration: `InvalidArgument`). Children nest (a gun with a muzzle);
  a prefab spawns at most 32 entities in all and a hierarchy no deeper than the depth limit (`LimitReached` at
  registration). Variants inherit their base's children and may append their own (never remove them). A prefab
  without children is registered, stored and spawned exactly as today.
- **FR-17 Spawning a hierarchy, all or nothing.** Spawning a prefab with children checks up front that the world has
  an entity slot for every entity of the hierarchy and a slot in every kind they need (overrides included) and that
  attaching stays within the depth limit under the spawn's parent (FR-18); if not, nothing is created (spec 021's
  FR-8: a null handle, a miss, `WhenFull`'s assert or quiet miss). Otherwise it creates the entities **parent first,
  then children depth-first in entry order**, each exactly as spec 021's spawn creates one (components, placement,
  tags), links each child to its parent with its entry's local transform, computes their world transforms (FR-8's
  "at creation"), and then runs the spawn hooks **children before parents** (post-order, as Godot's `_ready`), so a
  ship's hook finds its gun ready. It returns the root's handle. The result — handles, slots, component bytes, tags,
  links and world transforms — is identical to the equivalent hand-written `spawn`/`add`/`add_tag`/`attach` calls in
  that order followed by the hooks in that order (AC-2 proves it). The spawn's position and component overrides apply
  to the root only.
- **FR-18 Spawning under a parent, and from maps.** A prefab (with or without children) may be spawned **as a child**
  of an existing node (the spawn's position is then local to that parent) — a weapon pickup spawned in a hand. Spawning
  a map's objects (spec 021's FR-9) spawns each object's whole hierarchy, the object's bounds placing the root; the
  children's spawn info carries the same map object as their root's. Tiled has no object parenting, so maps never
  express hierarchies themselves (Non-goals).

### Determinism, events, cost, safety, opt-in

- **FR-19 Same results everywhere.** The hierarchy uses only spec 020's deterministic rules (lowest free slot, ascending
  slots, the destroy queue) plus fixed orders (attach order, the pass's order, prefab entry order) and FR-3's float
  operations. The same calls give the same links, orders and world-transform bits on PC (every supported host
  compiler) and on the 3DS.
- **FR-20 No events.** The engine publishes no event when a node is attached, detached, reparented or moved
  (constitution IV): state is queried, and a hook or the game's own events (`ItemPickedUp{Entity item, Entity hand}`)
  cover the rest.
- **FR-21 Bounded cost, no allocation while playing.** After the world's creation nothing allocates, logs in the normal
  path or calls `virtual` functions. Costs: `parent`, `depth` and reading a world transform constant time; the pass
  linear in the number of nodes, each node's sine and cosine recomputed only when its rotation changed since the last
  pass; `world_now` linear in the depth; attach linear in the moved subtree's size (its depth check and depths);
  cascades one destroy per descendant. Memory per node stated by the plan (expected ≤ 96 bytes on the 3DS). Measured
  on an Old 3DS (AC-7).
- **FR-22 Stats and debug drawing.** Stats: nodes live, peak and capacity (the kind's), roots, deepest depth, nodes
  computed by the last pass and attach refusals since creation — a debug overlay line. A debug draw (Debug and
  Development only; nothing in Release) draws, for a screen, a line from each node to its parent and its local axes.
- **FR-23 Misuse is loud in Debug, harmless in Release.** A stale, null or foreign handle, an entity without a node, a
  cycle, too deep a hierarchy, keep-world under a non-invertible parent, a NaN or infinite transform reaching the pass,
  the pass called inside an iteration of the same world or from a hook, a prefab child without the node kind — assert
  in Debug/Development and do nothing (or answer nothing / the identity) in Release; never undefined behaviour.
- **FR-24 Optional and light.** The transform value and its math are usable from a light header that includes only
  `<nnh/math.hpp>` (no world); the hierarchy is an optional module over the entities header. Neither includes
  graphics, collision, camera, tweens, animation, tilemap or prefab headers. A game that includes neither links nothing
  new (`examples/hello` unchanged on PC and 3DS); a game that uses prefabs without children links no hierarchy code.
  Spec 020's and spec 021's public behaviour, tests and golden values (entities `C8FB9A41`, prefabs `7BB7901E`) stay as
  they are; what the prefab extension needs from the world or the library is internal or an addition the plan
  justifies.

## Acceptance criteria

- **AC-1 Unit tests first** (constitution VI), on the fake platform. Transform values: identity, composition order
  (scale, rotate, move), points, directions, inverse round trips, mirroring, shear chains (points exact, drawing values
  as documented), the deterministic sine and cosine (quarter turns exact, symmetry, against a reference table).
  Hierarchy: attach order, detach, keep-world and keep-local modes, attach to the current parent, cycles and the depth
  limit refused (nothing changed), foreign and stale handles, queries (`parent`, `depth`, `root`, children order,
  `is_ancestor`, `each_descendant` with destroy and spawn inside); the pass (order, each node once, roots equal to their
  locals bit for bit, a node created or attached between passes, rotation-unchanged nodes skipping the sine);
  `world_now` equals the pass bit for bit; cascades (every descendant destroyed, hooks in the documented order with
  their node data intact, the parent stale in a child's hook, a hook that destroys or attaches during a cascade,
  `detach_children` first, `clear()` with hierarchies and the first load's slots after it), removing the node component;
  scene binding of a world with nodes (flushed and cleared with it); the drawing helper's merge with given and animator
  options; bounds of unrotated, rotated and mirrored rectangles. Prefabs: registration of children (limits, missing
  node kind, variants appending children), all-or-nothing spawns of a hierarchy on a full world and a full kind,
  creation and hook order, spawning under a parent, map objects with children. Misuse cases (FR-23). The Linux build VM
  runs them with GCC 13/14 and clang 18 and with ASan/UBSan, as previous specs did.
- **AC-2 Golden values.** A test runs a fixed, seeded script of 50 000 operations (the engine's integer hash, no
  `rand`) over a world of 1024 entities and a library of 12 prefabs (with children, nested children and a variant):
  spawns of single nodes and hierarchies (by prefab and by hand, under a parent and as roots), local transforms set
  from seeded integers (angles including quarter turns, uniform, non-uniform and negative scales), attaches and
  detaches in both modes, refused attaches, destroys (cascading) and removals, passes every 64 operations, flushes and
  clears. It checks after every pass that `world_now` equals the stored value for every node, and that the prefab-built
  world and a hand-built twin hold the same handles, links and bits; and it compares a checksum of every live node's
  world-transform bits, links and order with a golden value. The same test passes on every host compiler and in the
  console self-check (AC-6).
- **AC-3 Accuracy and exactness.** Unit tests compare world positions of seeded hierarchies (depth up to the limit,
  coordinates within ±4096 px, scales 1/4–4) with a `double` reference: within FR-3's bound. Exactness: roots bit for
  bit, unrotated unscaled chains equal to hand-added positions, quarter turns on whole pixels, keep-world round trips
  (attach then detach) returning the original world transform within the bound and exactly when no shear.
- **AC-4 The platformer adopts the hierarchy.** Each turret gets a **barrel** child (a node and a small sprite
  component) spawned by the turret prefab's child entry; the barrel's local rotation turns toward the player in the 8
  directions the turret already aims (`TurretBook::aim`), and bullets leave from the barrel's **muzzle point** mapped to
  world space. Turrets and barrels are drawn through FR-13's helper; the HUD's entity count includes the barrels. Because
  barrels are entities, the replay sample's live count and handle hash change, and bullets start from the muzzle: the
  expected lines in `examples/platformer/tests/level.nnhs` are regenerated from the PC with the procedure at the top of
  that file, in the same commit that adds the barrels, which says why; the ctest `platformer_replay_matches_script`
  passes again and the console run of `level.nnhs` must print the new PC lines. (The alternative that keeps the lines —
  the platformer untouched — is a decision below.) A PC screenshot shows a turret's barrel aimed at the player. The
  benchmark, zoom and flip demos are untouched.
- **AC-5 Example `examples/hierarchy`** (name fixed by the plan; original art or shapes only), modes chosen by an
  environment variable (`showcase` (default) | `check` | `bench`): the top screen shows a **ship** (moved and turned
  with the Circle Pad and L/R) carrying a **gun** that aims on its own (C-Stick or D-Pad) with a **muzzle** where
  bullets start; a **walker** with a hat that bobs (a tween on the hat's local position), mirrored with a negative scale
  when it turns; a **cart** with a rider the camera follows (a `CameraRig` on a child); a non-uniformly scaled
  "squash" parent with a rotated child (shear: points exact). The bottom screen: a **touch** picks the node under it
  (FR-14's bounds through a spec 019 query), a button attaches the picked item to the ship's hand or drops it (keep
  world: it doesn't jump), another destroys the ship (its children go with it; the HUD shows the count) or detaches its
  children first; the HUD shows FR-22's stats; FR-22's debug draw is toggled by a button. PC screenshots for the
  showcase, a mirrored walker, a pick-up and drop, a cascade and the debug draw, checked by reading the PNGs.
- **AC-6 Console script.** The `check` mode runs AC-2's script on the engine's real worlds and libraries and logs
  `hierarchy: <n> sum <checksum>` every 10 000 operations and `hierarchy: PASS <checksum>` or `hierarchy: FAIL ...` at
  the end, against golden values recorded from the simulator. `examples/hierarchy/tests/check.nnhs` starts it, expects
  every line, then switches to the showcase, presses fixed inputs and **touches** fixed points, and expects logged
  answers (for example `picked: hat slot 7 gen 1 depth 2 at 132.5,40.25`, `dropped: hat at 132.5,40.25` — the same
  world position before and after — and `destroyed: ship + 3 children`), and takes screenshots. On an Old 3DS the
  maintainer only opens the Homebrew Launcher and presses Y, and `nnh run hierarchy --script
  examples/hierarchy/tests/check.nnhs` passes; the same script passes with `--pc`. The verdict, log and screenshots are
  in the saved log folder.
- **AC-7 Cost on the console.** The `bench` mode, measured on an Old 3DS with the profiler and logged as
  `hierarchy bench: PASS <numbers>` (or FAIL). **Targets** (the plan confirms them before code; they predate any
  console measurement, and are estimated from the Old 3DS's 268 MHz ARM11, its 16 KB data cache with 32-byte lines, no
  L2, no hardware prefetcher and ~110 cycles per miss: a node record of ~96 bytes is about three lines):
  - the pass over **1024 nodes** in 256 hierarchies of 4 (a root, two children, a grandchild), every root moved and
    turned and one child turned each frame: **under 1.5 ms**;
  - the same pass when only positions changed (rotations and scales as at the last pass — the platformer's case):
    **under 1.0 ms**;
  - **256 `world_now`** of depth-3 nodes in random hierarchies: **under 1.5 ms** (memory-bound: each walks four
    records);
  - **64 prefab hierarchies of 4 entities** (a node and two other components each, one tag) spawned, then destroyed by
    destroying their roots (cascades), plus one flush: **under 2.2 ms**, and **at most 1.3×** the same hierarchies built
    by hand with spec 020's calls and `attach`, measured in the same run (spec 021's budget for 256 single spawns is
    2 ms);
  - **256 keep-world reparents** between rotated and scaled parents plus **256 keep-local** ones: **under 1.0 ms**.
  If a target is missed, the implementation is optimised first; if it is still missed, the budget is set from the first
  console measurement with margin and written in the spec and the docs with the measured numbers (the maintainer's
  policy for benches, as in specs 019 to 021). Spec 012's tile gate, spec 020's and 021's benches and the platformer's
  60 Hz replay verdict still pass.
- **AC-8 Opt-in.** `examples/hello` builds with the same executable size as before on PC and 3DS; `examples/entities`,
  `examples/prefabs` (goldens `C8FB9A41` and `7BB7901E`), `examples/sprites`, `examples/scenes`, `examples/camera` and
  `examples/queries` keep their screenshots pixel-identical and their scripts passing; every earlier spec's tests pass
  unchanged. A game that spawns prefabs without children links no hierarchy code, and a game using only the transform
  value links no entities code (checked on the 3DS build's map file or symbol list). Building a game `.cpp` that
  includes the hierarchy header stays within constitution III's budgets (measured before and after on the example).
- **AC-9 Docs.** New `docs/api/` page(s) for transforms and the hierarchy (the transform value and its math, units and
  mirroring, determinism and exactness, shear, nodes and the node kind, attach/detach and modes, queries, the pass and
  where it goes in a frame, `world_now`, reparenting, cascades and `detach_children`, drawing, bounds and the body
  pattern, following a node with the camera, costs, memory, misuse); `docs/api/math.md` links the transform value;
  `docs/api/entities.md` (the engine's one component kind, cascades), `docs/api/prefabs.md` (children), `docs/api/gfx.md`
  (drawing a node) and `docs/api/camera.md` (following a child) link it; `docs/guides/levels.md` notes how map objects
  spawn hierarchies; a decision row in `docs/architecture/decisions.md`; CLAUDE.md layout and ROADMAP updated;
  `docs/REFERENCES.md` citing the prior art below.

## Non-goals

- **Hierarchies across worlds**, or of things that are not entities (pool objects, tilemap objects, UI): a parent and
  its children live in one world. Pools and the game's own arrays use the transform value directly (FR-2).
- **Oriented (rotated) collision boxes**, polygons and rotated tile collision: spec 019's queries stay axis-aligned;
  a rotated child's box is its enclosing bounds (FR-14).
- **Inherited visibility, enabled state, draw layer or tint** (Godot's `visible`/`z_index` inheritance, Unity's
  `SetActive` on a subtree): the game checks the parent itself or tags the subtree. A follow-up if games need it.
- **An automatic propagation pass** tied to scenes or the frame loop, or change events; and **dirty-flag lazy world
  transforms** (Godot's): see Decisions to take.
- **Tiled hierarchies**: Tiled has no object parenting, group layers are refused by spec 012's converter, and so are
  rotated objects; a map object's prefab brings its own children (FR-18). Reading Tiled's object rotation into a
  local rotation is a follow-up.
- **Skeletal animation**, bones, inverse kinematics, constraints (look-at, follow with lag) and physics joints: a
  game turns its own nodes; tweens and animators drive local values.
- **Reordering children** (Godot's `move_child`) and attaching at an index: children keep attach order; detach and
  attach again to move a child to the end. A follow-up if UI needs it.
- **3D transforms** (matrices for M8's 3D): the 2D value keeps its name, 3D types come later beside it (as `math.hpp`
  says of `Vec2`).
- **Saving, serialising or inspecting** hierarchies (the "Save data with versions" and "Live values in the simulator"
  items).
- **Changing spec 020's or spec 021's existing behaviour**: a world without the node kind and a prefab without
  children behave and cost exactly as before (FR-24).

## Constraints

- **I** — portable core code with no platform part and no platform `#ifdef`; the same calls give the same transforms
  on PC and 3DS (FR-3, FR-19); the example and scripts run in the simulator and on the console.
- **II** — node records live in the world's one block, sized by the node kind's capacity; the plan states the bytes per
  node (expected ≤ 96 on the 3DS: about 96 KB for 1024 nodes) and the stack the pass and cascades need for the depth
  limit (fixed, no allocation); costs measured on an Old 3DS (AC-7).
- **III** — a light transform header (only `<nnh/math.hpp>`), a hierarchy header over the entities header (no standard
  containers or `<functional>`), templates limited to typed helpers; `hello` unaffected; build times measured (AC-8).
- **IV** — state is queried (parent, children, world transforms); no events for attaches, reparents or moves (FR-20).
- **V** — no exceptions (`Result`/`bool` and asserts), no RTTI (spec 020's kind ids), no `virtual`, no allocation after
  the world's creation, no per-frame heap; the pass calls no game code.
- **VI** — pure logic, unit-tested first on the fake platform with a golden checksum and a `double` reference (AC-1 to
  AC-3); visible behaviour by screenshots and on the console (AC-4 to AC-6).
- **VII** — transform math, the deterministic sine (shared with tweens), node links, the pass, reparenting, cascades,
  bounds and drawing helpers, prefab children in small separate files; one public header per module.
- **VIII** — header comments, the `docs/api/` page(s), linked pages, the example and the platformer updated.
- **IX** — written from the published designs (Godot's and Unity's manuals, Bevy's and flecs' documentation, EnTT's
  articles, *Game Programming Patterns*); no code copied; credited in `docs/REFERENCES.md`.
- **X** — no new parser or file format; misuse is loud in Debug/Development and harmless in Release (FR-23); a NaN or
  infinite local transform is caught at the pass instead of spreading through a subtree silently.
- **XI, D25** — optional: transforms without entities (FR-2), entities without hierarchies, prefabs without children,
  hierarchies without prefabs, drawing, collision or cameras (FR-24).
- **Under pressure:**
  - **Spec 020's non-goal "the engine ships no `Transform`, `Sprite` or `Body` component"**: this spec ships exactly one
    engine-defined component kind (the node), registered by the game like any other, because links and cascades need
    fields the engine knows. Spec 020's own non-goal foresaw it ("parent and child links are a component holding
    handles ... updated by a pass in parent-first order"). The decision row must say so.
  - **Spec 021's "Nested and multi-entity prefabs" non-goal** is lifted here: FR-16/17 extend D36's single-entity
    reservation to a whole hierarchy, without changing single-entity spawns' bytes or the prefabs golden (FR-24).
  - **"The same bits on PC and 3DS"** now covers sine and cosine: FR-3 relies on the engine's polynomial sine and on
    `-ffp-contract=off`; any host compiler or flag that contracts or reorders float operations breaks AC-2 (that is what
    it is for). Rotated texels stay backend rasterisation (spec 004), so screenshots of rotated sprites may differ by a
    pixel between PC and 3DS — scripts compare logged values, not those pixels.
  - **The maintainer's preference for unchanged replay lines** conflicts with AC-4 as recommended: any child entity in
    the platformer's world changes the replay's live count and handle hash. AC-4 regenerates the lines deliberately (as
    spec 020's AC-4 step 2 did); keeping them means leaving the platformer untouched (Decisions to take).
  - **D30 is not amended**: the pass is an explicit call (FR-8). An automatic "after update" pass would need a new
    scene notification and would race the camera rig, which must run after the pass.
  - **AC-7's** targets predate any console measurement and fall back to the measurement-with-margin policy.

## Prior art

- **Godot**: `Node2D` (`position`, `rotation`, `scale`, `skew`, `global_transform`), `Transform2D` (a 2×3 affine
  matrix, so shear is representable), `Node.reparent(new_parent, keep_global_transform = true)`, `queue_free` freeing
  children, and `_ready` called on children before their parent —
  [`Node2D`](https://docs.godotengine.org/en/stable/classes/class_node2d.html),
  [`Transform2D`](https://docs.godotengine.org/en/stable/classes/class_transform2d.html),
  [`Node`](https://docs.godotengine.org/en/stable/classes/class_node.html) (`reparent`, `_ready`, `queue_free`),
  [Using SceneTree](https://docs.godotengine.org/en/stable/tutorials/scripting/scene_tree.html). FR-2's affine world
  map, FR-10's keep-world default and FR-17's post-order hooks follow it.
- **Unity**: `Transform` with `localPosition`/`localRotation`/`localScale`, `SetParent(parent, worldPositionStays)`,
  `lossyScale` (the approximate world scale under shear), destroying a GameObject destroys its children —
  [`Transform`](https://docs.unity3d.com/ScriptReference/Transform.html),
  [`Transform.SetParent`](https://docs.unity3d.com/ScriptReference/Transform.SetParent.html),
  [`Transform.lossyScale`](https://docs.unity3d.com/ScriptReference/Transform-lossyScale.html),
  [`Object.Destroy`](https://docs.unity3d.com/ScriptReference/Object.Destroy.html). FR-2's drawing values under shear
  and FR-11's cascade follow it.
- **Bevy**: `Transform` (local) and `GlobalTransform` (computed) kept in sync by a propagation system in `PostUpdate`;
  changes after it lag a frame; `TransformHelper` computes one entity's global transform on demand —
  [Transforms](https://bevy-cheatbook.github.io/fundamentals/transforms.html),
  [Parent/Child hierarchies](https://bevy-cheatbook.github.io/fundamentals/hierarchy.html),
  [`Transform`](https://docs.rs/bevy/latest/bevy/prelude/struct.Transform.html). FR-8 (one pass, its place in the
  frame) and FR-9 (`world_now`) take this shape.
- **flecs**: `ChildOf` relationship pairs, children deleted with their parent by the relationship's cleanup policy
  (depth-first, children before their parent; FR-11 lets the plan fix its own documented order), `cascade` queries
  that visit parents before children —
  [Relationships manual](https://github.com/SanderMertens/flecs/blob/master/docs/Relationships.md),
  [Hierarchies manual](https://github.com/SanderMertens/flecs/blob/master/docs/HierarchiesManual.md).
- **EnTT** (Michele Caini): hierarchies as a component of links (parent, first child, previous/next sibling) on sparse
  sets, and walking them without jumping around memory —
  ["ECS back and forth" part 4: hierarchies](https://skypjack.github.io/2019-06-25-ecs-baf-part-4/) and
  [part 4 insights](https://skypjack.github.io/2019-08-20-ecs-baf-part-4-insights/). FR-5/FR-7's link component.
- Robert Nystrom, *Game Programming Patterns*: [Dirty Flag](https://gameprogrammingpatterns.com/dirty-flag.html)
  (scene-graph world transforms, the case for and against lazy recomputation); Bitsquid,
  [Building a data-oriented entity system, part 3: the transform component](http://bitsquid.blogspot.com/2014/10/building-data-oriented-entity-system.html)
  (transforms as a component with parent/child indices and a world-matrix array).

## Decisions to take

Every question has a recommended answer; none is decided until the maintainer approves the spec.

- [ ] **One spec for hierarchy and transforms, and transforms usable without it?** — **recommended: one spec, two
      modules**: a transform value with its math (only `<nnh/math.hpp>`) and the hierarchy over entities (FR-2, FR-24).
      A game with its own arrays gets deterministic composition; the hierarchy builds on the same value (as pools
      and worlds, spec 020).
- [ ] **Name.** "Scene tree" collides with NNHGE's scenes (spec 001). Options: (a) **"hierarchy" / "node" /
      "transform"** in headers, types and the example; (b) Godot's "scene tree". — **recommended: (a)**; the ROADMAP
      item keeps its title with a note. Exact names (`Transform2D`, `Node`, header names) fixed by the plan.
- [ ] **Where do nodes live?** Options: (a) **an engine-provided node component kind in the game's world**, registered
      by a helper, holding local and world transforms and links; (b) a companion tree object beside a world, indexed by
      entity slot (it would still need a component's remove hook to see destroys); (c) the game's own struct, read by
      member pointers (as box views). — **recommended: (a)** (FR-5). Links and cascades need fields the engine owns; (a)
      reuses spec 020's storage, quarantine, binding and hooks; (c) can't hold links safely; (b) duplicates bookkeeping.
- [ ] **How are transforms represented?** Options: (a) **local as position/rotation/scale; world as an affine map plus
      accumulated rotation and scale for drawing**; (b) position/rotation/scale for both (Unity's lossy model: points
      wrong under shear); (c) matrices only (drawing would need `atan2`/`sqrt` to get an angle back). —
      **recommended: (a)** (FR-1, FR-2). Points are always exact, drawing needs no inverse trigonometry, and the usual
      chains (uniform scale, mirrors) are exact everywhere.
- [ ] **Angle unit.** Options: (a) **radians, clockwise, as `DrawOptions::rotation`, with engine constants for quarter
      turns that are exact**; (b) turns (1 = full turn); (c) degrees. — **recommended: (a)** (FR-1, FR-3). One unit
      across gfx, tweens and transforms; exact quarter turns come from the constants.
- [ ] **Sine and cosine.** Options: (a) **the engine's libm-free polynomial sine (spec 015), moved where both modules
      share it, plus a public deterministic `sin`/`cos`**; (b) `std::sin`/`std::cos` (different bits per libm); (c)
      lookup tables (memory, coarse angles). — **recommended: (a)** (FR-3). Same bits by construction; already
      measured accurate to 2·10⁻⁶; public so games aim with the same numbers.
- [ ] **When are world transforms computed?** Options: (a) **an explicit pass the game calls once per frame, plus
      `world_now` on demand**; (b) dirty flags and lazy recomputation on read (Godot: always fresh, but locals only
      through setters, so a tween by pointer or a prefab placement member would bypass it); (c) an automatic pass from
      scene binding (needs a new D30 "after update" notification and would run before the game's camera code
      can follow the result). — **recommended: (a)** (FR-8, FR-9). Locals stay plain data, the order in the frame is
      the game's (update → pass → camera → draw), and `world_now` covers mid-frame needs with the same bits.
- [ ] **Order of the pass.** — **recommended: roots in ascending node-slot order, each subtree depth-first in child
      order, each node once** (FR-8). Deterministic, independent of counts, parent always before child.
- [ ] **Destroying a parent.** Options: (a) **children destroyed (cascade), with `detach_children` to keep them**; (b)
      children detached as roots; (c) a per-node choice. — **recommended: (a)** (FR-11). Godot, Unity and flecs' default;
      a gun left floating is the worse surprise; keeping is one explicit call.
- [ ] **Cascade order and hooks.** — **recommended: spec 020's destroy queue (breadth-first in child order), each
      descendant's hooks with its data intact, its parent already stale** (FR-11). No recursion, nothing new in spec 020.
- [ ] **Removing the node component.** Options: (a) **detach the node and make its children roots keeping their world
      transforms**; (b) destroy the children; (c) refuse. — **recommended: (a)** (FR-11). Removing data shouldn't
      destroy other entities.
- [ ] **Reparenting mode default.** Options: (a) **keep world by default (Godot's `reparent`, Unity's `SetParent`),
      keep local on request**; (b) keep local by default (Godot's `add_child`); (c) no default: always explicit. —
      **recommended: (a)** (FR-6, FR-10). Picking up and dropping things is the common case; prefab children use their
      entry's local transform.
- [ ] **Cycles and depth limit.** — **recommended: cycles refused; a fixed limit of 16 levels** (FR-6). Fixed stacks
      for the pass and cascades with no allocation; 2D games nest a few levels (ship → gun → muzzle).
- [ ] **Child order.** — **recommended: attach order, appended at the end; no reordering in this spec** (FR-6,
      Non-goals).
- [ ] **Mirroring.** — **recommended: negative scale mirrors the subtree; `flip_x`/`flip_y` stay per-sprite and are not
      inherited** (FR-4, FR-13). One value turns a character around with everything it carries.
- [ ] **Drawing.** Options: (a) **a helper that fills position, rotation and scale into `DrawOptions`, merged with the
      game's and an animator's options; the game draws in its own loop**; (b) an engine sprite component drawn by the
      engine. — **recommended: (a)** (FR-13). The engine still ships no sprite component (spec 020); (b) would decide
      layers and draw order for the game.
- [ ] **Collision boxes of children.** Options: (a) **an axis-aligned bounds query (enclosing box when rotated) and a
      documented `each<Node, Body>` pattern after the pass**; (b) an automatic sync into a member of the game's
      component (a member pointer per world); (c) oriented boxes in spec 019. — **recommended: (a)** (FR-14). (b) is a
      follow-up if the pattern repeats; (c) is a collision spec of its own.
- [ ] **Camera following a node.** — **recommended: no camera change; the rig follows the node's world position after
      the pass** (FR-15).
- [ ] **Prefabs with children.** Options: (a) **child entries (a registered prefab + an optional local transform),
      spawned all-or-nothing as one hierarchy, hooks children first, the root's handle returned**; (b) hooks spawn and
      attach children themselves (possible today, but not all-or-nothing and no equivalence test); (c) not now. —
      **recommended: (a)** (FR-16, FR-17), limits 8 child entries per prefab and 32 entities per spawn. It lifts spec
      021's non-goal the way that spec anticipated.
- [ ] **Hook order in a hierarchy spawn.** Options: (a) **children before parents (post-order), after every entity is
      created and linked**; (b) parents first. — **recommended: (a)** (FR-17): a ship's hook finds its gun ready
      (Godot's `_ready`).
- [ ] **Tiled.** — **recommended: no map-side hierarchies (Tiled has none; group layers and rotated objects stay
      refused); a map object's prefab brings its children** (FR-18, Non-goals).
- [ ] **Events.** — **recommended: none** (FR-20), as specs 020 and 021.
- [ ] **Inherited visibility, layer or tint.** — **recommended: not now** (Non-goals).
- [ ] **Platformer adoption and its replay lines.** Options: (a) **turret barrels as child entities aimed in the 8
      directions, bullets from the muzzle; the replay lines regenerated in the same commit, the console required to
      match them** (AC-4); (b) barrels drawn with the transform value only (no entities: lines unchanged, but the
      hierarchy is not exercised in a real game); (c) the platformer untouched. — **recommended: (a)**. Any child
      entity changes the sample's live count and handle hash, so "unchanged lines" and "children in the platformer"
      can't both hold; putting the muzzle's world point into the bullets puts FR-3's bits under the replay, which is
      what it exists to check. If the maintainer prefers unchanged lines, (c) with the new example carrying everything.
- [ ] **Example.** — **recommended: a new `examples/hierarchy` that owns the console self-check and bench; the
      entities and prefabs examples unchanged** (AC-5, AC-8).
- [ ] **Old 3DS cost targets** — **recommended: AC-7's five targets, confirmed by the plan, and set from the first
      console measurement with margin if they are still missed after optimisation** (the maintainer's bench policy).

## Open questions

- [ ] The exact accuracy bound of FR-3 (1/32 pixel proposed) — the plan measures it against the `double` reference
      before fixing it.
- [ ] The depth limit (16 proposed) and the per-node memory (≤ 96 bytes proposed) — the plan confirms both with its
      record layout.
- [ ] Whether caching each node's sine and cosine (FR-21: recomputed only when its rotation changed) is worth its 12
      bytes per node on the Old 3DS — the plan decides from AC-7's first measurement.
- [x] Every item of "Decisions to take" above (as recommended, overnight pre-approval 2026-11-23).
