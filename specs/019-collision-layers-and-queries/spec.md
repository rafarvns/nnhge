# Spec 019: Collision layers and queries

Status: approved (2026-11-22, maintainer pre-approved recommendations: every open question below is answered with the
recommended answer); **implemented 2026-11-23**, console check (T10.2) and Linux runs (T9.1) pending
Author: NNHGE maintainers · Date: 2026-11-22
Depends on: [spec 012](../012-tilemaps-and-collision/spec.md) (`<nnh/collision.hpp>`: boxes, `Grid`, `Cell`,
`move`, `cells_touching`; `<nnh/tilemap.hpp>`: collision layers, solid/one-way tiles, `collision_grid()`, properties,
the Tiled converter), [spec 002](../002-core-foundation/spec.md) (asserts, logs, profiling). Works with
[spec 018](../018-camera-helpers/spec.md)'s screen ↔ world conversions (a touch becomes a world point to query)
without depending on the camera module. Verified on the console with [spec 013](../013-console-remote/spec.md)'s
`nnh run --script`. Implements two ROADMAP items of "Features in the spirit of Godot and Unity" together: "Collision
layers and masks" and "Raycasts and shape queries". Optional for games (constitution XI, decision D25).

## Problem and goal

Spec 012 gave games boxes, grids and `collision::move`, and nothing to say **which things may touch which** or to
**ask the world a question**. Today a game that wants bullets to ignore the ship that fired them, coins that only the
player picks up, or a water layer that stops boats but not birds writes its own `if` chains in every loop; a turret
that should fire only when it **sees** the player, a laser that stops at the first wall, a cursor that tells you
"what's under this point", or a bullet so fast it would skip a thin wall between two frames all need a ray against
tiles and boxes, and the first hand-written version of that tunnels through corners or slips between two diagonal
tiles. Godot (physics layers and masks, `RayCast2D`, `intersect_point`), Unity (layers, `Physics2D.Raycast`,
`OverlapPoint`) and Box2D (category/mask bits, ray casts with a fraction) all solve it on top of a physics world.
NNHGE has no physics world and keeps it that way: the game owns its arrays of boxes and its grids; after this spec
it tags them with **layer bits**, filters with **masks**, and asks pure, allocation-free questions — rays (first hit
or all hits) against tiles with an exact grid traversal and against boxes, box casts, point and box overlap queries —
that give the same bits on PC and 3DS. A game that uses none of it pays nothing.

## User stories

- As a game developer, I want to put each of my boxes on one or more named layers (player, enemy, coin, player
  bullet, world) and give each a mask of the layers it detects, so that "coins only touch the player" and "bullets
  ignore their shooter" are data, not `if` chains.
- As a game developer, I want my tilemap's collision to carry layers too (a water layer that stops boats but not
  fish, one-way planks that only the player stands on), set in code or in Tiled, so that one map serves different
  kinds of movers.
- As a game developer, I want to cast a ray from a turret to the player and know whether a wall is in between (line
  of sight), against tiles and boxes, filtered by layers.
- As a game developer, I want the first thing a ray hits — where, which side (normal), how far, which cell or which
  box — and optionally every thing it passes through (a piercing laser), into a buffer I own.
- As a game developer, I want a fast bullet to hit a thin wall or a small enemy even when it moves many pixels per
  frame, without tunnelling.
- As a game developer, I want to ask "what is under this point" (a touch, a cursor) and "what overlaps this box" (an
  explosion, a sword swing), over my own arrays, filtered by layers.
- As a game developer, I want these queries to keep my structs as they are: I point the query at my array of enemies
  (box and layers are fields) instead of copying boxes into an engine container.
- As a game developer, I want the results to be identical on PC and 3DS frame by frame, so that replays and console
  checks compare exactly.
- As a game developer who doesn't use layers or queries, I want my game, its executable and spec 012's collision to
  stay exactly as they are.
- As a contributor, I want all of it tested without a console, on the fake platform.

## Functional requirements

### Layers and masks

- **FR-1 Layer bits.** `<nnh/collision.hpp>` gains a **layer set** type: 32 independent layer bits held in one
  32-bit unsigned value, with named constants for **no layers** (0), **all layers** and the **default layer** (bit
  0), and a `constexpr` way to make the bit of layer `n` (0..31; out of range asserts in a constant expression or in
  Debug/Development). The engine names no layers: the game declares its own (an enum of bit constants, documented
  pattern). Layer sets combine with the usual bit operators. Everything that had no layers before this spec is on the
  **default layer**.
- **FR-2 Detection rule.** One rule, Godot's: **A detects B when A's mask shares a bit with B's layers.** A helper
  answers it (`detects(mask, layers)`, name fixed by the plan). It is one-directional — the side that reacts asks
  (the player collects a coin if the player's mask has the coin layer) — and a game that wants Box2D's symmetric
  rule writes `detects(a.mask, b.layers) && detects(b.mask, a.layers)` (documented). A box whose layers are **no
  layers** is detected by nothing (a cheap way to disable an entry in an array, e.g. a taken coin); a mask of no
  layers detects nothing.
- **FR-3 Box views.** Queries over boxes take a **box view** of the game's own memory, never a copy: a pointer to the
  first box, a count and a **byte stride**, plus an optional pointer and stride to each box's layer set (absent: every
  box is on all layers, so an unfiltered array works as before). Helpers make a view from a plain `Rect` array, a
  `Rect` array with a parallel layer array, and an array of the game's structs through member pointers (`enemies`,
  `&Enemy::box`, `&Enemy::layers`). A view owns nothing, is a few words, and is valid while the game's array is;
  results report boxes by **index** in the view.
- **FR-4 Query filter.** Every query takes an optional **filter**: a mask (default: all layers), one index to
  **ignore** (default none: a bullet's query can skip its shooter even when they share a layer), and, for rays and
  casts, whether shapes that already contain the start are reported (**hit from inside**, default off, FR-10, FR-12).
  The filter is a small value with defaults, so the shortest call has none.

### Layers on grids and tilemaps

- **FR-5 Grids stay as they are.** `collision::Grid`, `move`, `contacts` and `cells_touching` keep their behaviour and
  their results bit for bit. A grid the game describes (an array or a callback) has no layers: a game that wants
  layered cells describes one grid per purpose (or reads its own layer data in its callback) — the documented
  pattern. Layer filtering of tiles happens where the layers are known: in the tilemap (FR-6), which hands out a grid
  already filtered.
- **FR-6 Tilemap collision layers.** A tilemap's collision gains layer sets, each defaulting to the default layer:
  - every **tile layer** has a **collision layer set** used when its `collision` flag is on (all its non-empty tiles
    are solid on those layers);
  - every **tile id** marked solid or one-way has a **collision layer set** (a water tile solid only on a "boat"
    layer, a plank one-way only on a "player" layer);
  - both are set in code (build or load time, names fixed by the plan) and read from **Tiled**: an integer property
    `collision_layers` on a tile layer or on a tile of a tileset — a Tiled custom enum with "allow multiple values"
    saved as a number gives a checkbox per layer and stores exactly these bits (Tiled supports bits 0..30);
  - a query returns the union of a cell's layer sets (`collision_layers_at(x, y)`, name fixed by the plan);
  - `collision_grid(mask)` returns a grid in which a cell is **Solid** when any solid contribution of that cell
    (a collision tile layer or a solid tile) has a layer in `mask`, else **OneWay** when a one-way tile there has one,
    else **Empty**; `collision_grid()` with no argument means all layers and returns exactly what it returns today.
  The filtered grid reads the map live like today's (edits are seen at once) and costs at most one extra mask test
  per contribution looked up.
- **FR-7 Tiled checks.** The converter (`nnh-assetc`) refuses a `collision_layers` property that is not an integer
  or is 0 (a collision layer on no layers collides with nothing: the message says to untick `collision` instead),
  with its usual `file: reason`; one fixture per refusal. Maps converted before this spec load unchanged and behave
  as before (everything on the default layer); the plan states whether the `.nnhmap` format changes (recommended:
  no, the values travel as the properties they already are).

### Rays

- **FR-8 A ray is a segment.** A ray goes **from** a point **to** a point (Godot's `target_position`): no direction
  normalisation, no infinite rays; a "long" ray is a far end point. A ray of zero length hits nothing (FR-14 answers
  point questions). A **hit** reports:
  - the **fraction** along the segment in [0, 1] (Box2D) and the **distance** (fraction × segment length);
  - the **point** where the segment enters the shape, with the coordinate of the face it crosses **exactly** equal
    to that face (not recomputed through the fraction), so a hit point is flush with the wall it reports;
  - the **normal** of that face, a unit axis vector pointing out of the shape (left face: (−1, 0)), or (0, 0) for a
    hit from inside;
  - **what** was hit: a cell (its x, y and `Cell` kind, `x`/`y` possibly outside the grid when `outside` blocks) or a
    box (its index in the view);
  - whether it was a hit from inside.
- **FR-9 Ray against a grid: exact traversal.** A ray against a `Grid` visits the cells the segment passes through
  **in order, every one of them, from the start to the end** (Amanatides & Woo's traversal), so no wall is skipped
  however long the ray is (10 000 px is fine) and a thin wall between two frames of a fast bullet is always found.
  Rules (each covered by tests):
  - **Touching is not hitting**, as everywhere in spec 012: a segment that only runs along a cell's edge, or ends
    exactly on a cell's face, does not hit that cell; a segment that enters a cell's area does.
  - **Corners never leak**: a segment passing exactly through a grid corner treats both cells beside the corner as
    crossed, so line of sight never squeezes between two diagonal solid cells; the reported face of such a hit is the
    x face (ties prefer x, as `move` and `separation` do).
  - **One-way cells** block only a ray entering through their **top face while going down** (normal (0, −1)), the
    same rule as `move`; an option ignores them entirely (line of sight usually does).
  - **Start inside a blocking cell**: that cell is ignored unless the filter asks for hits from inside (then it is
    the hit, at fraction 0, normal (0, 0)).
  - **Cost**: one cell lookup per cell visited; when `outside` is Empty the segment is first clipped to the grid, so a
    ray from far outside costs only the cells inside; runs of non-blocking `outside` cells are skipped, as in `move`.
- **FR-10 Ray against boxes.** A ray against a box view tests every box of the view that passes the filter (FR-2,
  FR-4) with the slab method. A box is hit when the segment passes through its **interior** (touching is not
  hitting); the hit is the entry point on its boundary with that face's normal; a ray entering exactly through a box
  corner reports the x face. Boxes with no width or height, boxes on no layers and the ignored index are never hit.
  Boxes that contain the start are ignored unless the filter asks for hits from inside (Godot's `hit_from_inside`,
  Box2D's rule of ignoring shapes that contain the origin).
- **FR-11 First hit and all hits.** Each ray query exists in two forms:
  - **first hit**: the nearest hit, or none; ties at the same fraction go to the lowest index (boxes) or the first
    cell in traversal order (grids);
  - **all hits**: writes up to `capacity` hits into the caller's buffer **sorted by fraction** (ties as above) and
    returns how many there are in total (which may exceed `capacity`; when it does, the buffer holds the nearest
    `capacity`); a null buffer with capacity 0 counts. Against a grid, "all" means every blocking cell along the whole
    segment (a piercing laser); against boxes, every box whose interior the segment crosses.
  Plus **cells crossed**: every cell the segment visits against a grid, blocking or not, in order, with the fraction
  where the segment enters it — the traversal itself, for fog of war, lasers drawn per tile and debug drawing — with
  the same buffer/total contract.
- **FR-12 Grid and boxes together; line of sight.** A first-hit query takes a grid and a box view at once (either may
  be absent) and returns the nearest hit of both; when a cell and a box are hit at the same fraction, the **cell**
  wins (a wall in front of an enemy standing flush against it hides it). A **line of sight** helper answers "is
  nothing in the way from A to B" as a bool over the same inputs, ignoring what contains A (the viewer's own box is
  excluded with the filter's ignore index, or by its layers).
- **FR-13 Box casts against boxes.** A **box cast** moves a box along a segment (its top-left corner from → to)
  against a box view and reports the first contact or all contacts with FR-8's fields: the fraction, the box's
  position at contact (flush with the face it touches), the face's normal and the index. Rules as FR-10: touching is
  not hitting, boxes already overlapped at the start are ignored unless hits from inside are asked for, same filter.
  A cast with the relative motion of two moving boxes answers "did these two meet this frame". Box casts against
  **grids** are a non-goal (`collision::move` moves boxes through grids; thin projectiles use rays).

### Point and box queries

- **FR-14 What is under a point.** Over a box view: every box that **contains** the point (half-open edges, as
  `contains`), filtered, written in **ascending index** into the caller's buffer with the total returned (FR-11's
  contract). Over a grid: the cell under the point (x, y and kind, including `outside` cells beyond the edges; the
  cell's top-left half-open rule of spec 012). A game combines them with a tilemap's `tile_at` and properties to tell
  "what's under this touch".
- **FR-15 What overlaps a box.** Over a box view: every box that **overlaps** a given box (shared area > 0), filtered,
  in ascending index, same buffer contract; the given box may be one of the view's (the filter's ignore index skips
  it). Over a grid: today's `cells_touching` (unchanged), which with FR-6's filtered grid is layered too.

### Determinism, cost, safety, opt-in

- **FR-16 Same bits everywhere.** Every query uses only `+ − × ÷`, comparisons, floor/ceil on `float` and, for the
  distance alone, the IEEE square root (correctly rounded by definition, so equal on every target; compiled without
  `errno` handling), with no FP contraction and a documented order of operations, as spec 012's collision. The same
  inputs give the same hits — fractions, points, normals, order — bit for bit on PC (every supported host compiler)
  and on the 3DS. Cell indices saturate at ±2^30 as in spec 012.
- **FR-17 Bounded cost, no state.** Queries keep no state, need no `nnh::init()`, never allocate, never log in the
  normal path and use no `virtual`: the cost is one lookup per grid cell visited and one test per box in the view
  (no broadphase: the game's arrays are small; non-goals). Their documented costs are measured on an Old 3DS (AC-7).
  Misuse — NaN or infinite points, a negative count or capacity, a null buffer with capacity > 0, a stride smaller than
  a `Rect` (or than a layer set) when a pointer is given, a grid with cell sizes ≤ 0 — asserts in Debug/Development
  and returns no hits (total 0) in Release, never undefined behaviour.
- **FR-18 Optional and unchanged.** Layers and queries are optional (XI). The collision header keeps depending only on
  `<nnh/math.hpp>` (plus the fixed-width integer header for the layer type) and needs no tilemap; the tilemap does not
  need the queries. A game that uses neither links no query code (checked on executable size); `overlaps`, `move`,
  `contacts`, `cells_touching` and `Tilemap::collision_grid()` give the same results as before, so spec 012's tests,
  the tile gate and the platformer's replay checksums are unchanged by the engine change alone (AC-4 step 1).

## Acceptance criteria

- **AC-1 Unit tests first** (constitution VI), on the fake platform: layer bits, `detects`, disabled entries (no
  layers), box views over plain arrays, parallel arrays and structs with a stride, absent layer pointers; filters
  (mask, ignore index, hit from inside); ray vs grid — each direction (8 octants and the 4 axis-aligned cases), rays
  starting and ending on cell edges, along an edge (no hit), ending on a face (no hit), exactly through corners
  (both neighbours checked; no slipping between diagonal cells; x-face tie), start inside a solid cell with and
  without hit from inside, one-way cells from above/below/the side and ignored, outside Solid and Empty, a ray from far
  outside clipped to the grid, 10 000-px rays, cell sizes that are not square, negative and fractional origins;
  cells crossed matching a brute-force reference (sampling) on thousands of seeded rays, and first hit equal to the
  first blocking cell crossed; ray vs boxes (slab entry, each face's normal, grazing edges, corner tie, zero-size,
  inside start); first hit vs all hits, sorting, ties, capacity overflow keeping the nearest, counting with a null
  buffer; grid + boxes nearest-of-both with the cell-wins tie; line of sight; box casts (each face, flush contact
  position, starting overlap, relative motion of two movers); point and box overlap queries (half-open edges,
  ascending order, ignore index); misuse cases (FR-17). The Linux build VM runs them with GCC 13/14 and clang 18 and
  with ASan/UBSan, as previous specs did.
- **AC-2 Golden values.** A test casts a fixed, seeded set of 4096 rays (the engine's own integer hash, no `rand`)
  and 512 box casts against a fixed grid with solid and one-way cells and 64 layered boxes, and compares a checksum of
  every result's bits (fractions, points, normals, indices, cells, order) with a golden value; the same test passes on
  every host compiler and in the console self-check (AC-6).
- **AC-3 Tilemap layers.** Unit tests: default layer sets; tile-layer and tile-id layer sets set in code; the union
  query; `collision_grid(mask)` for masks that keep, drop or turn cells into one-way; `collision_grid()` equal to
  before cell by cell (a regression test written before the change); live edits seen by a filtered grid; `move`
  through a filtered grid. Converter: a `.tmj` fixture with a `collision_layers` flag enum on a layer and on tiles
  loads with the expected sets; one fixture per refusal (FR-7) prints `file: reason`; maps converted before this spec
  load and give the same grid. The existing tilemap fuzz targets still run clean.
- **AC-4 Platformer adopts layers and queries**, in two documented steps, as spec 018 did:
  1. **Same play first** — the player, coins and level geometry get layer sets (player, coin, world) and coins are
     picked up through a box-overlap query over the coin book's array (a struct view; taken coins have no layers)
     filtered by the player's mask, instead of the hand-written loop: the replay's checksum lines in
     `examples/platformer/tests/level.nnhs` and the ctest `platformer_replay_matches_script` pass **unchanged**. This
     proves the swap.
  2. **Then something visible** — level 1 gets one or two **turrets** (Tiled objects of a new type) that cast a line of
     sight ray to the player each frame against the level's grid and fire slow bullets (enemy-bullet layer, mask
     player + world) when they see the player; bullets move by a ray against the grid and the player's box each frame,
     so they stop at the first wall even at speed (no tunnelling), and the player's own coins and turrets ignore them
     through masks. Bullets are **harmless to the player's movement** (a hit flashes the player and counts in the HUD)
     so the replay script's inputs still play the same run. The replay sample **adds** the turrets' state (sight,
     bullets, hit count) to its checksum — rays are exactly what the console must reproduce — so the expected lines
     change: they are regenerated from the PC with the procedure at the top of `level.nnhs`, the ctest passes again,
     and the console run of `level.nnhs` must print the new PC lines. The commit that adds the turrets changes the
     golden lines in the same commit and says why. A PC screenshot (`NNH_EXIT_AFTER_FRAMES` + `NNH_SCREENSHOT`) shows
     a turret with its sight line drawn to the player and a bullet stopped flush at a wall.
  The benchmark and flip demos are untouched.
- **AC-5 Example `examples/queries`** (original art or shapes only): the top screen shows a small tile room (a
  tilemap with a solid layer, a water layer on its own collision layer and one-way planks) and coloured boxes on
  three layers; a ray from a fixed emitter to a point the player moves (Circle Pad / D-Pad) is drawn with the cells
  it crosses shaded, every hit marked with its normal, and the first hit highlighted; buttons toggle the query mask's
  layer bits (shown in the HUD), switch first hit / all hits / box cast, and toggle "ignore one-way". On the bottom
  screen a **touch** is converted to a world point (through a spec 018 camera rig or the `Camera2D`, whichever the
  plan picks; the example shows the conversion) and the HUD lists **what is under it**: the cell, its kind and layer
  set, the tile id and its properties, and the boxes containing the point; the point is marked on the top screen.
  PC screenshots for first hit, all hits with the cells crossed, a box cast, a mask that lets the ray pass the water,
  and a touch query, checked by reading the PNGs.
- **AC-6 Console script.** `examples/queries` has a self-check mode that runs AC-2's script on the engine's real
  queries and logs `queries: <n> sum <checksum>` every 1024 rays and `queries: PASS <checksum>` or `queries: FAIL ...`
  at the end, against golden values recorded from the simulator. `examples/queries/tests/check.nnhs` starts it,
  expects every line, then **touches** fixed points on the bottom screen and expects the logged answers (for example
  `under: cell 5,7 solid layers 0x1 tile 3 boxes 2`), moves the ray with the stick and expects the logged first hit,
  and takes screenshots of each mode. On an Old 3DS the maintainer only opens the Homebrew Launcher and presses Y, and
  `nnh run queries --script examples/queries/tests/check.nnhs` passes; the same script passes with `--pc`. The
  verdict, log and screenshots are in the saved log folder.
- **AC-7 Cost on the console.** A bench mode of `examples/queries`, measured on an Old 3DS with the profiler and
  logged as `queries bench: PASS <numbers>` (or FAIL): 128 rays of up to 400 px per frame against a tilemap's
  filtered grid (16×16 tiles) plus 64 boxes, first hit, costs **under 2 ms per frame** (target; the plan confirms it),
  and point queries cost **at most 0.1 µs per box tested** (measured with 256 point queries over 256 boxes per frame;
  plan decision 17, confirmed by the maintainer 2026-11-23: the first target, 0.3 ms for that whole workload, would
  need a broadphase, a non-goal); per-ray and per-box costs are written in the docs. Spec 012's tile
  gate (4 layers + 300 sprites) still passes, and `collision::move` through `collision_grid()` costs the same as
  before (no measurable change on the gate's movers).
- **AC-8 Opt-in.** `examples/hello` builds with the same executable size as before on PC and 3DS (no query code
  linked); `examples/sprites`, `examples/scenes`, `examples/camera` and the platformer's benchmark keep their
  screenshots pixel-identical; spec 012's tests pass unchanged.
- **AC-9 Docs.** `docs/api/collision.md` (layers and masks with the detection rule and a layer-enum pattern, box views,
  filters, rays with the traversal rules and a picture of the corner rule, first/all hits, cells crossed, line of
  sight, box casts, point and box queries, determinism, costs), `docs/api/tilemap.md` (collision layer sets, filtered
  grids) and the guide [Levels in Tiled](../../docs/guides/levels.md) (a `collision_layers` flag enum step by step),
  a decision row in `docs/architecture/decisions.md` if the plan adds one (layer sets in the tilemap's collision, the
  `Grid` lookup carrying a mask), CLAUDE.md layout and ROADMAP updated (both items), `docs/REFERENCES.md` citing
  Godot, Unity, Box2D, Tiled's custom enums and Amanatides & Woo.

## Non-goals

- A physics world, rigid bodies, forces, collision response between boxes, joints: collision stays pure functions on
  the game's data (spec 012).
- A broadphase (spatial hash, grid buckets, BVH) or a registry of bodies: queries scan the view the game passes. A
  follow-up spec if the AC-7 bench or a real game shows it is needed; a game can already keep its own buckets and
  pass one bucket's view.
- Rotation, oriented boxes, circles, capsules, polygons, slopes and pixel-perfect masks: boxes and grid cells only.
  Circle casts are a candidate follow-up.
- Box casts against grids (FR-13): `collision::move` and rays cover them for now.
- Collision events (`BodyEntered`, `AreaExited`): state is queried (constitution IV); a game that wants enter/exit
  compares this frame's query with the last.
- A global layer collision matrix (Unity's) or layer **names** known to the engine: masks on the game's values say
  the same thing; names live in the game's enum (and in Tiled's custom enum).
- Tags and groups: a separate ROADMAP item (layers are for collision filtering only).
- Finding all overlapping pairs among N boxes (an n² helper) and continuous collision between many moving boxes: the
  game loops over its movers with FR-13/FR-15.
- Debug drawing inside the collision module: it would make collision depend on graphics. `examples/queries` draws rays,
  cells and normals with `gfx` in a small, copyable helper.
- Layered cells in game-described grids beyond "one grid per purpose or your own callback" (FR-5).
- 3D raycasts (M8).

## Constraints

- **I** — the same calls give the same hits on PC and 3DS (FR-16); all of it is portable core code with no platform
  part; the example and scripts run in the simulator and on the console.
- **II** — no memory beyond the caller's buffers; per-ray cost bounded by cells visited and boxes in the view, measured
  on an Old 3DS (AC-7); spec 012's gate and `move` costs unchanged.
- **III** — the collision header stays light (`<nnh/math.hpp>` and `<cstdint>`; no `<cmath>`, no templates heavier
  than the member-pointer view helper); building `examples/hello` is unaffected.
- **IV** — queries, never events; nothing is published.
- **V** — no exceptions, no RTTI, no `virtual`, no allocation anywhere (not even at the first call).
- **VI** — pure logic, unit-tested first on the fake platform with a brute-force reference for the traversal (AC-1);
  the tilemap change starts with a regression test of `collision_grid()` (AC-3); visible behaviour by screenshots and
  on the console (AC-4 to AC-6).
- **VII** — layers, views, grid traversal, box rays, casts and point/box queries in small separate files; still one
  public header for collision (the tilemap additions go in its header).
- **VIII** — header comments, `docs/api/collision.md`, `docs/api/tilemap.md`, the levels guide, `examples/queries`, the
  platformer updated.
- **IX** — the traversal is written from Amanatides & Woo's paper and the slab method from the literature; no code
  copied from Godot, Unity or Box2D (Box2D is MIT, compatible, but its ideas are enough); credited in
  `docs/REFERENCES.md`.
- **X** — the converter refuses bad `collision_layers` with `file: reason` (FR-7) and its readers stay fuzzed; misuse
  of queries is loud in Debug/Development and harmless in Release (FR-17).
- **XI, D25** — entirely optional; collision without tilemaps keeps working, tilemaps don't need queries, the
  platformer's benchmark keeps plain code.
- **Under pressure:** FR-6 touches the tilemap's collision lookup, the hot path of `collision::move` through a map:
  the plan must show `collision_grid()` is identical cell by cell (AC-3) and costs the same (AC-7), and how the mask
  reaches the lookup (a `Grid` field read only by the tilemap's callback, or a context the map hands out) without
  changing results for existing callers. "No libm" is bent once: the distance uses the IEEE square root, which is
  exact by the standard and inlined (FR-16); the fraction, which needs none, is the primary value. AC-4 step 2
  deliberately changes the platformer's golden replay lines; the regeneration procedure already exists and the console
  must reproduce the new lines.

## Prior art

- Godot physics layers and masks
  ([physics introduction](https://docs.godotengine.org/en/stable/tutorials/physics/physics_introduction.html#collision-layers-and-masks)):
  32 layers, "layer = what I am, mask = what I scan", one-directional detection (FR-1, FR-2);
  [`RayCast2D`](https://docs.godotengine.org/en/stable/classes/class_raycast2d.html) and
  [`PhysicsRayQueryParameters2D`](https://docs.godotengine.org/en/stable/classes/class_physicsrayqueryparameters2d.html):
  `target_position` as a segment, `collision_mask`, `exclude`, `hit_from_inside`, hit point and normal (FR-4, FR-8,
  FR-10); `PhysicsDirectSpaceState2D.intersect_point`/`intersect_shape` with a result limit (FR-14, FR-15).
- Unity layers and the [layer collision matrix](https://docs.unity3d.com/Manual/LayerBasedCollision.html) (a global
  matrix: non-goal; masks say the same per value); [`Physics2D.Raycast`](https://docs.unity3d.com/ScriptReference/Physics2D.Raycast.html)
  with a layer mask, `RaycastHit2D` (point, normal, distance, fraction), `RaycastAll`/`RaycastNonAlloc` into a caller
  array (FR-11), [`Physics2D.queriesStartInColliders`](https://docs.unity3d.com/ScriptReference/Physics2D-queriesStartInColliders.html)
  (FR-10), `OverlapPoint`/`OverlapBox`, `BoxCast` (FR-13 to FR-15).
- Box2D ([simulation docs](https://box2d.org/documentation/md_simulation.html)): category and mask bits with a
  symmetric rule (FR-2's documented variant), ray casts reporting a fraction and ignoring shapes that contain the
  origin (FR-8, FR-10).
- John Amanatides and Andrew Woo, "A Fast Voxel Traversal Algorithm for Ray Tracing", Eurographics 1987
  ([PDF](http://www.cse.yorku.ca/~amana/research/grid.pdf)): visiting every cell a ray crosses in order with two
  `tMax`/`tDelta` pairs (FR-9, FR-11's cells crossed).
- Tiled custom enums ([custom properties](https://doc.mapeditor.org/en/stable/manual/custom-properties/)): "allow
  multiple values" saved as a number stores the chosen options as bit flags, up to 31 (FR-6).

## Open questions

All answered with the recommended answer, pre-approved by the maintainer (2026-11-22).

- [x] **One spec for both ROADMAP items, or two?** — **answer: one.** The raycast item says "filtered by layers" and the
      layers item has little to test without queries; the same filter type serves `overlaps`-style queries and rays,
      and the platformer's adoption shows both at once.
- [x] **How many layers, and what type?** — **answer: 32 bits in one unsigned 32-bit value, an alias with named
      constants, not a strong class** (FR-1). Godot and Unity use 32; Box2D v2 used 16 (v3 moved to 64). A plain value
      makes the game's `enum : Layers { Player = 1u << 0, ... }` pattern natural and costs nothing; Tiled can set bits
      0..30, which is documented. 64 bits would double every layer array for no game we know of.
- [x] **Detection rule: one-directional (Godot) or symmetric (Box2D)?** — **answer: one-directional, `mask & layers`,
      with the symmetric rule documented as one line of game code** (FR-2). Queries are naturally one-directional (the
      querier has a mask, the targets have layers), and with no physics world there is no engine-side pair to decide
      symmetrically; the reacting side asks.
- [x] **How does a query see the game's boxes: copy into an engine container, a span of `Rect`, a callback, or a
      strided view?** — **answer: a strided view with optional strided layers and helpers from arrays and member
      pointers** (FR-3). No copy, no allocation, no per-box function call, and it reads the game's structs as they are
      (`Enemy { Rect box; Layers layers; ... }`). A callback is slower per box on the ARM11 and a `Rect`-only span
      forces parallel arrays. Disabled entries use "no layers" instead of a separate active flag.
- [x] **How do tilemap cells carry layers?** Options: (a) a layer set per collision tile layer and per solid/one-way
      tile id, with a filtered grid; (b) a per-cell layer array; (c) only "one grid per purpose" for everybody. —
      **answer: (a)** (FR-6). It matches how levels are drawn (a "water" layer, a "plank" tile), costs a few bytes per
      layer and per tile id instead of per cell, maps straight to Tiled properties, and leaves `Grid` and the game's own
      grids untouched (FR-5). (b) costs 4 bytes per cell (4 MiB on the largest maps); (c) is still the pattern for
      game-described grids.
- [x] **Tiled side: an int property, a string of names, or an enum?** — **answer: an integer property
      `collision_layers`, documented with a Tiled custom enum "allow multiple values, save as number"** (FR-6, FR-7).
      It gives checkboxes in Tiled with no name table in the engine; a plain int also works for people without a
      Tiled project. The values already travel as properties, so the map format need not change (plan confirms).
- [x] **Ray shape: origin + direction + distance (Unity) or from → to (Godot)?** — **answer: from → to** (FR-8). It
      needs no normalisation (no square root) and maps to "from the turret to the player" directly; the fraction is
      exact, the distance is derived.
- [x] **Distance needs a square root: allowed?** — **answer: yes, for the distance only, IEEE `sqrt` inlined (no
      `errno`)** (FR-16). IEEE 754 requires a correctly rounded square root, so PC and 3DS agree bit for bit; every
      other value (fraction, point, normal, order) is computed without it, and AC-2's golden checksum covers it.
- [x] **Corners: may a ray pass exactly between two diagonal solid cells?** — **answer: no; both neighbours of a
      corner are checked, and the reported face is the x face** (FR-9). Leaking line of sight through a wall's corner
      is the classic DDA bug; ties prefer x as `move` and `separation` already do.
- [x] **Touching: does a ray that grazes an edge or ends on a face hit?** — **answer: no — touching is not hitting**
      (FR-9, FR-10), the same half-open rule as spec 012 (boxes that touch don't overlap; a box on the floor is flush,
      not inside). A ray cast to the exact face of a wall therefore reports "clear", consistent with a box moved there.
- [x] **Shapes that contain the start** — **answer: ignored by default, reported at fraction 0 with a zero normal when
      the filter asks** (FR-9, FR-10). Godot (`hit_from_inside` false) and Box2D ignore them by default; it lets a
      turret cast from its own box without an ignore index. Unity's default is the opposite, so the docs say so.
- [x] **One-way cells and rays** — **answer: block only when entered through the top face going down; an option
      ignores them** (FR-9). It is the same rule as `move`, so a bullet falling onto a plank stops on it while a ray
      cast upwards passes, and line of sight can ignore planks altogether.
- [x] **All hits beyond the buffer** — **answer: return the total, keep the nearest `capacity` sorted by fraction**
      (FR-11). It is the contract of `cells_touching` and `find_objects`, and the nearest hits are the useful ones (a
      laser piercing three enemies).
- [x] **Box casts against grids** — **answer: out of scope** (FR-13, non-goals). A straight-line swept box through a
      grid is a different traversal; `collision::move` already moves boxes through grids without tunnelling and rays
      cover thin projectiles. Box casts against boxes are cheap (a ray against boxes grown by the cast box) and in.
- [x] **A broadphase?** — **answer: no; linear scans of the game's views, measured** (FR-17, AC-7). Homebrew 2D games
      keep tens to a few hundred boxes; 64 boxes × 128 rays fits the target. A spatial hash is a follow-up if a game
      needs it, and a game can already pass one bucket's view.
- [x] **Debug drawing of rays and hits** — **answer: in `examples/queries`, not in the module** (non-goals).
      Collision depends only on math (spec 012) and should stay so; the example's helper is short and copyable.
- [x] **Platformer: what visible use, and what happens to the replay checksum?** — **answer: two steps as spec 018**
      (AC-4). Step 1 moves coin pickup to a layered query with unchanged golden lines (proves the swap); step 2 adds
      turrets with line of sight and bullets moved by rays, harmless to the player's movement so the replay script's
      run stays valid, and adds their state to the checksum, regenerating the golden lines in the same commit with the
      console required to match. Leaving turrets out of the checksum would hide exactly the ray divergence the replay
      exists to catch.
- [x] **Example: new `examples/queries` or only the platformer?** — **answer: both** (AC-4, AC-5). The platformer shows
      layers and line of sight in a real game; `examples/queries` shows every query, the touch "what's under this
      point", the mask toggles and owns the console self-check and bench.
- [x] **Module and header** — **answer: inside the collision module and `<nnh/collision.hpp>`, in separate source
      files linked only when used; tilemap additions in `<nnh/tilemap.hpp>`** (FR-18). Layers and queries extend the
      same vocabulary (`Rect`, `Grid`, `Cell`, `CellHit`); a second header would split one topic. The plan checks the
      header cost and that `hello` links nothing new.
