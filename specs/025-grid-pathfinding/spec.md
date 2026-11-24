# Spec 025: Pathfinding on grids

Status: implemented 2026-11-24 (Linux T9.1 and Old 3DS T10.2 pending). Approved 2026-11-23 (overnight pre-approval by the maintainer: every "Decisions to take" item and open
question as recommended; to review in the morning)
Author: NNHGE maintainers · Date: 2026-11-23
Depends on: [spec 012](../012-tilemaps-and-collision/spec.md) (`<nnh/collision.hpp>`: `Grid`, `Cell`, solid and one-way
cells; `<nnh/tilemap.hpp>`: `collision_grid()`, tile properties, `set_tile`), [spec 019](../019-collision-layers-and-queries/spec.md)
(`collision_grid(mask)` per collision layer, the exact grid traversal and its "corners never leak" rule, `cell_at`),
[spec 002](../002-core-foundation/spec.md) (asserts, logs, `Result`, tagged memory, profiling). Works with
[spec 020](../020-entities/spec.md)'s worlds and pools, [spec 018](../018-camera-helpers/spec.md)'s screen ↔ world
conversions (a touch becomes a goal) and [spec 014](../014-data-assets/spec.md)'s data assets (an enemy's search limits
in a text file) without depending on any of them. Verified on the console with [spec 013](../013-console-remote/spec.md)'s
`nnh run --script`. Implements the ROADMAP item "Pathfinding on grids" (Features in the spirit of Godot and Unity).
Optional for games (constitution XI, decision D25).

## Problem and goal

A top-down game needs its guards to walk around walls to the player, its units to go where the player touched, its
zombies to swarm toward the hero from every corner of the level, and a door that opens to change all of that at once.
Today an NNHGE game has grids (spec 012), layered collision grids and rays (spec 019), and nothing that answers "how do
I get from here to there": every game writes its own A*, usually with a `std::priority_queue` and a `std::map` (heap
allocation per search), float costs whose ties break differently on PC and 3DS (so replays drift), corner cutting that
sends a box into a wall `collision::move` then refuses, and no bound on a search that floods a big level when the goal
is unreachable. Godot ships `AStarGrid2D` (solid cells, weights, diagonal modes, heuristics, partial paths), Unity's
navigation and the A* Pathfinding Project offer grid graphs with penalties and erosion for agent size, and roguelikes
use Dijkstra maps so a hundred monsters share one search. After this spec an NNHGE game builds a **path map** from any
`collision::Grid` (its own array or callback, or a tilemap's filtered collision grid) plus optional per-cell weights,
asks **bounded, time-sliceable A\*** searches that never allocate, gets **partial paths** to the closest reachable cell,
smooths them into a few straight waypoints, builds **distance fields** toward one or many goals for crowds, updates
cells when a door opens, and gets **the same paths, bit for bit, on PC and 3DS**. A game that doesn't use it pays nothing.

## User stories

- As a game developer, I want to turn my level's collision (a tilemap's `collision_grid(mask)` or my own grid) into
  something I can search, with walls blocked and swamp or water slower, in one call at level load, so that I don't
  write a second description of my level.
- As a game developer, I want to ask "the path from this cell to that cell" and get the cells in order into a buffer I
  own, with its cost, without the engine allocating, so that enemies can re-plan during play.
- As a game developer, I want to choose 4 or 8 directions and whether diagonals may cut past wall corners, so that
  every path my agents get is one `collision::move` will actually let them walk.
- As a game developer, I want a search to stop after a number of cells I choose and still give me a path to the
  closest place it reached, so that an unreachable goal (the player on an island) never costs a whole frame and my
  enemy still walks to the shore.
- As a game developer, I want to spread one long search over several frames, so that a big level never drops a frame.
- As a game developer, I want a path turned into a few straight segments (no staircase zigzag across an open room),
  and a small helper that tells my agent which point to walk toward next, so that movement looks natural.
- As a game developer, I want 50 enemies to chase the player through one shared distance field instead of 50 searches,
  so that crowds are affordable on the Old 3DS.
- As a game developer, I want to open a door or break a wall and update only those cells, and know that paths computed
  before are out of date, so that dynamic levels work.
- As a game developer, I want a big creature (two cells wide) to get paths that fit it, so that it doesn't get stuck in
  one-cell corridors.
- As a game developer, I want the same calls to give the same paths on PC and 3DS, so that replays and console checks
  compare exactly.
- As a game developer who doesn't need pathfinding (or writes my own), I want my game, its size and its build time to
  stay as they are.
- As a contributor, I want all of it tested without a console, on the fake platform, against brute-force references.

## Functional requirements

### Path maps (the searchable cells)

- **FR-1 A path map object.** A path map is an object the game creates and owns (a level member, a scene member),
  never in a hidden global registry. It is created by a `Result`-returning `create(desc)` in **one block** of tagged
  memory (tag chosen by the game, default an engine tag) and covers a rectangle of **width × height cells** (1..1024
  each) with a cell geometry (origin and cell size in pixels, taken from a `Grid` or given). Errors (`InvalidArgument`
  for sizes or costs out of range (FR-6), `OutOfMemory`) are returned. It is movable, not copyable. The plan states
  its bytes per cell (expected 1).
- **FR-2 Cells: blocked or a weight.** Every cell of a path map is either **blocked** or **walkable with an integer
  weight** 1..255 (the cost multiplier for entering it; Godot's `weight_scale`, A* Pathfinding Project's penalty). A new
  map is all walkable, weight 1. Positions outside the map are never walkable: paths stay inside it.
- **FR-3 Building from a grid.** `sync(grid)` fills the map from a `collision::Grid` — a game array, a game callback
  or a tilemap's `collision_grid()` / `collision_grid(mask)` (spec 019: one map per kind of mover, e.g. boats and
  walkers) — reading each grid cell once: **Empty → walkable**, **Solid → blocked**, **OneWay → blocked by default**
  (a per-map setting makes it walkable; Decisions to take). The map's cell (0, 0) may be any grid cell (a **region**
  offset), so a map can cover part of a huge level. An optional **weight callback** (a function pointer and a context,
  called only during `sync`, once per walkable cell) returns a weight or "blocked", so swamp, water and roads come from
  the game's own data (a documented pattern reads a Tiled tile property, e.g. `path_cost`; the pathfinding module never
  includes the tilemap header). `sync(grid, region)` refreshes only a rectangle of cells (a door), with the same rules.
- **FR-4 Direct edits.** `set_cell(x, y, weight or blocked)`, `fill(region, weight or blocked)` and read-back queries
  (`blocked(x, y)`, `weight(x, y)`) edit and read cells without a grid. Every change (sync, set, fill) increments the
  map's **version** (a counter the game reads) so code that keeps paths knows they may be out of date.
- **FR-5 Agent footprint.** A map has an **agent size** of 1..4 cells (default 1). With size k, a cell is walkable for
  this map only if every cell of the k × k square whose top-left is that cell is walkable in the source, and its weight
  is the largest weight of that square (the A* Pathfinding Project's erosion, computed once at sync, never per search).
  A partial `sync` recomputes the cells whose square overlaps the refreshed rectangle. One map per agent size is the
  documented pattern.
- **FR-6 Neighbours, corners and costs.** A map has a **neighbour mode** — 4 neighbours, or 8 with one of three
  diagonal rules (Godot's names): **Always** (diagonals ignore the two orthogonal cells beside them), **AtLeastOneWalkable**
  (a diagonal step needs one of them walkable) and **OnlyIfNoObstacles** (both walkable: no corner cutting; the
  default for 8 neighbours, the same rule as spec 019's "corners never leak"). Step costs are **integers**: a
  **straight** cost (default 10) and a **diagonal** cost (default 14, between straight and 2 × straight). Entering cell
  c costs `step × weight(c)`; the start cell's weight never counts. `create` refuses (`InvalidArgument`) a combination
  whose worst path cost (`cells × diagonal × 255`) could exceed 32 bits, so costs never overflow (the defaults fit a
  1024 × 1024 map).

### Searches

- **FR-7 A searcher object.** A **searcher** holds the state of one search over a map (any map no larger than the
  searcher's size). It is created by `create(width, height[, tag])` in one block; the plan states its bytes per cell (expected ≤ 8)
  and how it avoids clearing that memory per search (generation stamps, cleared only when the stamp wraps). One searcher
  runs one search at a time; a game that searches for several agents at once makes several searchers over the same map.
  It is movable, not copyable.
- **FR-8 Find a path.** `find(map, from, to, options)` searches from cell `from` to cell `to` and returns a **result**:
  a status (**Found**, **Partial**, **NoPath**, **LimitReached**, **Invalid** — names fixed by the plan), the **cost**
  (sum of step × weight), the number of **cells** in the path and the number of cells **expanded**. The path is then
  copied with `path(out, capacity)` as cells from the start to the end, **both included**; it returns the total, and
  when the total exceeds `capacity` the buffer holds the first `capacity` cells from the start (enough to start
  walking); a null buffer with capacity 0 counts. The path stays readable until the searcher's next search. A helper
  writes the same path as **world points** (cell centres through the map's geometry), and helpers convert between
  world points and cells.
- **FR-9 Optimal by default.** The search is A* with the heuristic matching the neighbour mode (octile distance with the
  map's straight and diagonal costs for 8 neighbours, Manhattan × straight for 4), which never overestimates because
  weights are ≥ 1, so a **Found** path's cost equals the minimum cost between the two cells. An option **scales the
  heuristic** (an integer in sixteenths, ≥ 16, default 16 = exact) for faster, bounded-suboptimal searches (cost ≤ scale
  × the minimum), documented as such.
- **FR-10 Same path everywhere: a total order.** The search uses only integer arithmetic. Which path is returned among
  paths of equal cost is fixed by documented rules, so the result is a pure function of the map's cells, its settings,
  `from`, `to` and the options:
  - the open set is ordered by **f = g + h**, then **lower h** (closer to the goal), then **the most recently pushed**
    (a push sequence number);
  - neighbours are visited in one fixed order (N, E, S, W, then NE, SE, SW, NW; the plan may fix another order once,
    before the golden values are recorded);
  - a cell's parent changes only when a **strictly** lower g is found.
  The same inputs give the same status, cost, cells and expansion count bit for bit on PC (every supported host
  compiler) and on the 3DS.
- **FR-11 Start and goal rules.** `from == to` (walkable or not) is **Found** with one cell and cost 0. A `from` or `to`
  outside the map is **Invalid** (asserts in Debug/Development). A **blocked start** is allowed: the agent is standing
  there (a door closed on it), so only cells entered count. A **blocked goal** gives **NoPath** at once (no flood), or,
  with partial paths on, a search toward it whose result is **Partial** (FR-13).
- **FR-12 Bounded.** Options carry a **limit of expanded cells** per search (default: no limit beyond the map's size). A
  search that reaches it stops with **LimitReached** (or **Partial**, FR-13). The open set's capacity is fixed by the
  searcher's size and can never overflow; no search is ever unbounded in time or memory.
- **FR-13 Partial paths.** With the **partial** option (default off, as Godot), a search that can't reach the goal —
  unreachable, blocked goal, or the limit reached — returns **Partial** and a path to the **closest reached cell**: the
  cell given a cost during the search with the lowest heuristic distance to the goal, ties to the lower g, then the lower
  cell index (row-major). Without the option those searches return **NoPath** or **LimitReached** with no cells.
- **FR-14 Time slicing.** A search can run over several frames: `begin(map, from, to, options)`, `step(n)` (expand at
  most n cells; returns whether it finished), `cancel()`, and the result and path once finished. A search run in slices
  of any sizes gives **exactly** the result of `find` with the same inputs (same path, cost, expansions). If the map's
  version changes while a search runs (FR-4), the search keeps going over the current cells and its result is flagged
  **stale** (the map version it began with is kept), so the game decides whether to re-plan; it is never undefined
  behaviour.
- **FR-15 Smoothing.** `smooth(path, count, out, capacity)` (or an option of `path`) turns a path into **waypoints**: a
  subset of its cells, in order, always keeping the first and the last, where each kept cell is followed by the
  **furthest** later cell of the path that a straight segment between the two cell centres reaches crossing only cells
  that are walkable in the map, **no heavier** than the heaviest cell of the part of the path it replaces, and — for
  every mode except **Always** — never squeezing between two blocked cells at an exact corner (spec 019's corner rule;
  for **AtLeastOneWalkable** one of the two must be walkable). The walk over cells is spec 019's exact traversal (the
  plan says whether it reuses spec 019's code over the map, or a copy of its rules over the map's bytes). Same buffer
  contract as FR-8; deterministic; cost bounded by the cells crossed. A smoothed path is a path for a point or a box
  no larger than one cell (or the map's agent size), not for an arbitrary shape.

### Distance fields (many agents, one goal)

- **FR-16 A distance field object.** A **distance field** is created like a searcher (one block, the plan states its
  bytes per cell, expected ≤ 4) and **built** over a map from one or more **goal cells** (an array the game passes) with
  an optional **maximum cost**: afterwards, every cell holds the minimum cost for an agent standing there to reach the
  nearest goal under FR-6's rules (weights of entered cells, diagonal rules), or "unreachable" (also beyond the maximum
  cost and for blocked cells). This is Dijkstra from the goals over reversed steps (roguelikes' Dijkstra maps, the
  integration field of flow-field pathfinding).
- **FR-17 Building in slices.** A field can be built in one call or with `begin` / `step(n)` / `finished` like a search
  (FR-14), with the same result for any slice sizes and the same stale flag when the map changes. While a rebuild runs,
  queries answer from the **previous** finished build (double-buffered) or report "not ready" — the plan picks, stating
  the memory (Decisions to take).
- **FR-18 Field queries.** `distance(x, y)`; `next(x, y)`: the neighbour an agent at (x, y) should step into — the one
  minimising `step × weight(neighbour) + distance(neighbour)` under the map's diagonal rule, ties in FR-10's neighbour
  order, none at a goal or an unreachable cell; and a world-point helper. These are reads only (no per-cell direction
  vectors are stored), cheap enough for hundreds of agents per frame (AC-7). A field equals, cell for cell, the cost
  of FR-8's search from that cell to the nearest goal (tested).

### Following a path

- **FR-19 A path follower.** A small value (no allocation, a few words) follows a list of world waypoints the game owns
  (a pointer and a count, e.g. FR-15's output converted to world points): given the agent's position each frame it
  returns the **point to walk toward**, advances past waypoints within an **arrival radius**, and reports **arrived**
  at the last one. It never moves anything: the game turns the point into a velocity and moves its box with
  `collision::move` (or a tween). It uses only `+ − ×` and comparisons on squared distances (no square root), so it
  gives the same bits everywhere. Re-planning is the game's choice (on a version change, a goal that moved to another
  cell, a timer).

### Debugging, events, cost, safety, opt-in

- **FR-20 Inspecting a search.** A searcher answers, after or during a search, whether a cell was **reached** or
  **expanded** and its g, and a field answers its distances, so a game or an example can draw what was explored (a
  heat map, the open frontier). The module draws nothing (it would depend on graphics); `examples/pathfinding` has a
  small, copyable debug-draw helper.
- **FR-21 No events.** Pathfinding publishes no event (constitution IV): a finished time-sliced search or field is
  queried (`step` returns it, a status says it). There are no per-cell callbacks during searches.
- **FR-22 Bounded cost, no allocation.** After creation nothing allocates; `find`, `step`, `smooth`, field builds and
  queries call no game code and no `virtual` function and don't log in the normal path. The only game code ever called
  is FR-3's weight callback, during `sync` (load time or a door's few cells). Costs are measured on an Old 3DS (AC-7).
- **FR-23 Misuse is loud in Debug, harmless in Release.** Cells, regions or goals outside the map, a searcher or field
  smaller than the map, a weight of 0 or above 255 passed as a weight, a null buffer with capacity > 0, a negative
  capacity or limit, a NaN or infinite world point, `step` without `begin`, a map destroyed or moved during a sliced
  search (detected where possible) — assert in Debug/Development and return **Invalid** / no cells / nothing changed in
  Release; never undefined behaviour.
- **FR-24 Optional and light.** Pathfinding is an optional module (XI) with one public header that includes only
  `<nnh/math.hpp>`, `<nnh/collision.hpp>` (for `Grid` and `Cell`) and the fixed-width integer header; it includes no
  tilemap, entities, gfx or data header, and `<nnh/collision.hpp>` is not changed by this spec. A game that doesn't
  include the header links nothing new and `examples/hello` keeps its size on PC and 3DS.

## Acceptance criteria

- **AC-1 Unit tests first** (constitution VI), on the fake platform: map creation errors and limits (sizes, step costs,
  the 32-bit cost bound); sync from an array grid, a callback grid and a region offset; Empty/Solid/OneWay mapping and
  the OneWay setting; the weight callback (called once per walkable cell, only in `sync`); partial sync; set/fill and
  the version counter; agent sizes 1–4 (footprint walkability and max weight, partial sync near a changed cell); each
  neighbour mode and diagonal rule on hand-drawn corner cases (a diagonal gap between two walls: passes only with
  Always; one wall: passes with AtLeastOneWalkable); step and weight costs; start == goal, blocked start, blocked goal,
  goal unreachable, cells outside the map; the expansion limit and every status; partial paths (closest by h, ties by
  g then index); path buffers (exact fit, too small keeps the first cells, counting with null); world point helpers;
  the heuristic scale; time slicing equal to `find` for slice sizes 1, 7, 64 and the whole search; the stale flag; a
  searcher reused for many searches and across the generation stamp's wrap; smoothing (open room → 2 points, L-shaped
  corridor → 3, a corner it must not cut in each mode, a heavier cell it must not cross, first and last kept); fields
  with one and several goals, a maximum cost, weights, diagonal rules, sliced builds equal to one-shot builds, `next`
  ties; the follower (radius, overshoot of the last point, empty list); inspection queries; misuse (FR-23). The Linux
  build VM runs them with GCC 13/14 and clang 18 and with ASan/UBSan, as previous specs did.
- **AC-2 Brute-force references.** On thousands of seeded random maps (the engine's integer hash, no `rand`; sizes 1×1
  to 64×64, 0–50 % blocked, weights 1–9, every neighbour mode and agent size 1–2), a test compares each search with a
  plain reference written in the test (a Dijkstra with standard containers, allowed in tests): the same reachability,
  the same minimum cost for **Found**, a returned path that is valid (adjacent steps allowed by the mode, every entered
  cell walkable, its summed cost equal to the reported cost); a second reference A* implementing FR-10's order literally
  returns the **same cells**; every field equals the reference's cost from each cell to the nearest goal; every smoothed
  segment is checked against a sampled traversal of the map.
- **AC-3 Golden values.** A test runs a fixed seeded script — 512 searches (all modes, limits, partial paths, sliced and
  not), 64 smoothings, 16 fields with 1–8 goals, door edits between them — on fixed maps and compares a checksum of every
  result's bits (status, cost, expansions, stale flag, cells, waypoints, field distances, `next` answers) with a golden
  value. The same test passes on every host compiler and in the console self-check (AC-5).
- **AC-4 Example `examples/pathfinding`** (original art or shapes only), modes chosen by `PATHFINDING_DEMO`
  (`showcase` (default) | `check` | `bench`): the top screen shows a top-down level (a tilemap with walls, slow water or
  swamp tiles whose weights come from a tile property, and doors) with a **hero** that walks a smoothed A* path through
  a path follower and `collision::move`, and a **crowd** of 32 chasers following one distance field toward the hero
  (rebuilt in slices when the hero enters another cell). The bottom screen shows the whole level small (or the plan's
  equivalent); a **touch** sets the hero's goal at the touched cell (converted through the example's camera or layout,
  shown in the code). Buttons toggle a door (partial sync; the hero re-plans when the version changes), cycle the
  neighbour mode, toggle smoothing and the debug view (expanded cells shaded, raw path vs waypoints, the field as a
  heat map), and switch the hero between 1×1 and 2×2 (a second map with agent size 2). The HUD shows the last search's
  status, cost, cells, expansions and time, and the field's build progress. PC screenshots for a path with its explored
  cells, a smoothed path, the field heat map, a partial path to an island's shore, a door closed and open, and the 2×2
  hero refusing a one-cell corridor, checked by reading the PNGs.
- **AC-5 Console script.** The `check` mode runs AC-3's script on the engine's real maps and logs
  `pathfinding: <n> sum <checksum>` every 64 searches and `pathfinding: PASS <checksum>` or `pathfinding: FAIL ...` at the
  end, against golden values recorded from the simulator. `examples/pathfinding/tests/check.nnhs` starts it, expects
  every line, then switches to the showcase, **touches** fixed points on the bottom screen and expects the logged answers
  (for example `goal: 23,9 found cells 31 cost 362 expanded 148 waypoints 4`), presses the door button and expects the
  re-plan line with a different cost, touches an unreachable island and expects `partial`, and takes screenshots. On an
  Old 3DS the maintainer only opens the Homebrew Launcher and presses Y, and
  `nnh run pathfinding --script examples/pathfinding/tests/check.nnhs` passes; the same script passes with `--pc`. The
  verdict, log and screenshots are in the saved log folder.
- **AC-6 The platformer is untouched.** Platformer pathfinding (jumps, falls) is a non-goal, so
  `examples/platformer` doesn't change: its replay lines in `tests/level.nnhs`, the ctest
  `platformer_replay_matches_script`, its benchmark, gate, zoom and flip demos pass unchanged.
- **AC-7 Cost on the console.** The `bench` mode, measured on an Old 3DS with the profiler and logged as
  `pathfinding bench: PASS <numbers>` (or FAIL). **Targets** (the plan confirms them before code; they predate any
  console measurement and are estimated from the Old 3DS's 268 MHz ARM11, its 16 KB data cache with 32-byte lines, no L2,
  no hardware prefetcher and ~110 cycles per miss: an expansion pops the open set and reads up to 8 neighbours across 3
  rows of the map and of the searcher, a handful of misses, about 2 µs):
  - A* on a seeded **128 × 128** map (30 % walls in rooms and corridors, weights 1/3/8, 8 neighbours,
    OnlyIfNoObstacles), 32 searches between seeded walkable cells: **at most 2.5 µs per expanded cell** on average;
  - one search **across a 64 × 64 maze** (path ≥ 120 cells): **under 8 ms** in one call, and the same search in slices
    of 256 expansions: **every slice under 0.8 ms**;
  - 32 short re-plans (paths of 10–30 cells on a 64 × 64 level, typical enemies) in one frame: **under 3 ms**;
  - a **distance field** over 64 × 64 (one goal, weights, 8 neighbours): **under 8 ms** in one call, every slice of 512
    cells **under 1 ms**; **`next` for 256 agents: under 0.3 ms**;
  - **smoothing** a 200-cell path: **under 1 ms**;
  - **sync** of a 64 × 64 map from a tilemap's `collision_grid(mask)` with a weight callback reading a tile property:
    **under 5 ms** (load time), and a 3 × 3 door refresh: **under 0.05 ms**.
  If a target is missed, the implementation is optimised first; if it is still missed, the budget is set from the first
  console measurement with margin and written in the spec and the docs with the measured numbers (the maintainer's
  policy for benches, as in specs 019 to 021). Memory per cell of maps, searchers and fields is written in the docs.
- **AC-8 Opt-in.** `examples/hello` builds with the same executable size as before on PC and 3DS; `examples/queries`,
  `examples/entities`, `examples/prefabs` and the platformer keep their screenshots pixel-identical and their scripts and
  goldens passing; every earlier spec's tests pass unchanged; `<nnh/collision.hpp>` and `<nnh/tilemap.hpp>` are byte for
  byte unchanged. Building a game `.cpp` that includes the pathfinding header stays within constitution III's budgets
  (measured before and after on `examples/pathfinding`).
- **AC-9 Docs.** A new `docs/api/` page for pathfinding (maps and syncing from grids and tilemaps, weights and the
  `path_cost` pattern, agent sizes, neighbour modes with a picture of each diagonal rule, costs and the 32-bit bound,
  searches and statuses, the total order, limits and partial paths, time slicing and stale results, smoothing, fields
  and crowds, the follower, re-planning patterns, determinism, costs, memory, misuse); `docs/api/collision.md` and
  `docs/api/tilemap.md` link to it from grids and filtered grids; the guide [Levels in Tiled](../../docs/guides/levels.md)
  gets a "slow tiles" step (a `path_cost` tile property); a decision row **D40** in `docs/architecture/decisions.md`;
  CLAUDE.md layout and ROADMAP updated; `docs/REFERENCES.md` citing the prior art below.

## Non-goals

- **Platformer pathfinding** (jumps, falls, ladders, the platformer's one-way planks as floors): it needs a movement
  model (jump arcs, gravity) per game; a follow-up spec could build a graph of "jump links" over a map.
- **Navigation meshes, polygons, any-angle search (Theta\*)** and non-grid graphs (Godot's `AStar2D` over arbitrary
  points): grids only. Smoothing (FR-15) covers the visible part of any-angle paths.
- **Hexagonal and isometric cell shapes** (Godot's `cell_shape`, A* Pathfinding Project's six neighbours): square cells
  only; isometric games search in cell space and draw however they like.
- **Jump Point Search** (Harabor & Grastien): it prunes symmetric paths on **uniform-cost** grids and returns a different
  (equally short) path than FR-10's order, so it would be an option with its own goldens and no weights (Godot disables
  weights when `jumping_enabled`). A follow-up if AC-7 shows searches are too slow.
- **Hierarchical pathfinding (HPA\*)**, precomputed all-pairs tables and path caches: for maps much larger than the
  Old 3DS's levels; a follow-up if a game needs them.
- **Local avoidance and steering** (agents avoiding each other, RVO, separation): the follower returns a point; crowds
  overlapping is the game's to handle (boxes and `collision::separation` already exist).
- **Per-cell direction vectors** stored in fields and "flee" maps (negated, rescaled Dijkstra maps): fields answer
  `next` by reading neighbours; fleeing is a documented pattern or a follow-up.
- **Threads** and searches running in the background: searches run where the game calls them; time slicing (FR-14)
  spreads them over frames.
- **A request queue** or scheduler that shares a per-frame budget among many agents: the example shows the pattern
  (round-robin `step(n)` over a few searchers); a follow-up if games repeat it.
- **An engine path component** for spec 020 worlds or agents stored by the engine: maps, searchers, fields and followers
  are values the game keeps wherever it wants.
- **Debug drawing inside the module** (FR-20) and **events** (FR-21).
- **Changing the platformer** (AC-6).

## Constraints

- **I** — portable core code with no platform part and no platform `#ifdef`; the same calls give the same paths, costs
  and fields on PC and 3DS (FR-10); the example and scripts run in the simulator and on the console.
- **II** — one block per map (≈ 1 byte/cell), searcher (≤ 8 bytes/cell) and field (≤ 4 bytes/cell), sized at creation:
  a 128 × 128 level costs about 16 KB + 128 KB per searcher + 64 KB per field; the largest map (1024 × 1024) costs
  1 MiB + 8 MiB per searcher, which the docs state, with the region offset (FR-3) as the way to search part of a huge
  level. Costs designed for the ARM11's cache and integer pipeline, measured on an Old 3DS (AC-7).
- **III** — one light header (math, collision, `<cstdint>`; no standard containers, `<functional>` or templates beyond a
  member-pointer helper if the plan needs one); `hello` unaffected; build times measured (AC-8).
- **IV** — queries and commands only; no events (FR-21).
- **V** — no exceptions (`Result` at creation), no RTTI, no `virtual`, no allocation after creation; the only game code
  called is the weight callback during `sync`, a plain function pointer.
- **VI** — pure logic, unit-tested first on the fake platform against brute-force references and a golden checksum
  (AC-1 to AC-3); visible behaviour by screenshots and on the console (AC-4, AC-5).
- **VII** — maps, syncing, footprints, the open set, the search, partial paths, slicing, smoothing, fields and the
  follower in small separate files; one public header for the module.
- **VIII** — header comments, a `docs/api/` page, collision/tilemap/levels pages linked, `examples/pathfinding`.
- **IX** — written from the published algorithms (Hart, Nilsson & Raphael's A*, Dijkstra, Amit Patel's notes) and
  Godot's and the A* Pathfinding Project's documentation; no code copied; original art only; credited in
  `docs/REFERENCES.md`.
- **X** — no new parser or file format (the weight pattern reads tile properties through the tilemap's existing,
  fuzzed loader); every search is bounded in time and memory (FR-12); misuse is loud in Debug/Development and harmless in
  Release (FR-23).
- **XI, D25** — optional: pathfinding without tilemaps (any `Grid`, or cells set by hand), tilemaps without
  pathfinding, nothing required from entities, data assets or the camera; a game's own A* stays supported.
- **Under pressure:**
  - **"Same bits everywhere" for a search** depends on a total order fixed **forever** once the goldens are recorded
    (FR-10): later optimisations (a faster heap, JPS, a different neighbour order) must return the same cells or become
    a new option with new goldens. AC-2's literal reference A* exists to hold that line.
  - **Spec 019's corner rule** must match FR-6's OnlyIfNoObstacles and FR-15's smoothing exactly; if the plan reuses
    spec 019's traversal, it reads the map's cells through a `Grid` callback (a function call per cell during smoothing)
    or needs an internal entry point into the collision module's traversal (a `src/collision/*_internal.hpp`, as D36 did
    for entities) — the plan chooses and states the cost.
  - **D34** (layers live beside the map, the `Grid` callback unchanged) is relied on as is: a path map per mask comes
    from `collision_grid(mask)`; `sync` of a large map pays one tilemap callback per cell (load time, AC-7).
  - **Memory (II)**: searchers sized per cell trade memory for speed; on the largest maps that is 8 MiB per searcher
    (D40, Decisions to take).
  - **AC-7's** targets predate any console measurement and fall back to the measurement-with-margin policy.
  - A new decision row **D40** (path map baked from grids, per-cell searchers with stamps, the total order, fields).

## Prior art

- **Godot** [`AStarGrid2D`](https://docs.godotengine.org/en/stable/classes/class_astargrid2d.html): a grid that owns its
  solid flags and weight scales (`set_point_solid`, `fill_solid_region`, `set_point_weight_scale`, "multiplies the cost
  of segments entering that point"), a `region`, `cell_size` and `offset`, four `diagonal_mode`s (`ALWAYS`, `NEVER`,
  `AT_LEAST_ONE_WALKABLE`, `ONLY_IF_NO_OBSTACLES`), heuristics (Euclidean, Manhattan, octile, Chebyshev),
  `get_id_path(from, to, allow_partial_path)` returning a path to the closest reachable point (with a warning that a
  solid goal can make that slow — FR-12's limit answers it), `jumping_enabled` (JPS) which disables weights, and
  `update()`/`is_dirty()` after geometry changes. FR-2, FR-6, FR-11, FR-13 follow it;
  [`AStar2D`](https://docs.godotengine.org/en/stable/classes/class_astar2d.html) (arbitrary points) is a non-goal.
- **A\* Pathfinding Project** (Unity), [grid graphs](https://arongranberg.com/astar/docs/gridgraph.html): four, six or
  eight neighbours, `cutCorners`, penalties per node, tags, and `erodeIterations` "to add extra margin to obstacles" for
  bigger agents — FR-5's agent footprint and FR-6. **Unity's navigation** ([NavMesh](https://docs.unity3d.com/Manual/nav-BuildingNavMesh.html))
  bakes a walkable surface ahead of time, as FR-3 bakes cells; meshes themselves are a non-goal.
- **Amit Patel, Red Blob Games**: [Introduction to A\*](https://www.redblobgames.com/pathfinding/a-star/introduction.html),
  [Implementation of A\*](https://www.redblobgames.com/pathfinding/a-star/implementation.html) (tie-breaking and "ugly
  paths" from equal-cost neighbours, which FR-10 fixes by rule and FR-15 straightens) and
  [Amit's A\* pages](http://theory.stanford.edu/~amitp/GameProgramming/) ([heuristics](http://theory.stanford.edu/~amitp/GameProgramming/Heuristics.html):
  octile distance, scaling the heuristic for speed, breaking ties toward the goal) — FR-9, FR-10;
  [flow field pathfinding for tower defence](https://www.redblobgames.com/pathfinding/tower-defense/) (one breadth-first
  or Dijkstra pass from the goal serves every unit) — FR-16 to FR-18.
- P. E. Hart, N. J. Nilsson and B. Raphael, "A Formal Basis for the Heuristic Determination of Minimum Cost Paths", IEEE
  Transactions on Systems Science and Cybernetics, 1968 — A* and admissible heuristics (FR-9).
- **Dijkstra maps** in roguelikes: Brian Walker, ["The Incredible Power of Dijkstra Maps"](https://www.roguebasin.com/index.php/The_Incredible_Power_of_Dijkstra_Maps)
  (RogueBasin): one map toward many goals, monsters roll downhill, fleeing by rescaling — FR-16 to FR-18 and the
  non-goal on flee maps.
- Elijah Emerson, ["Crowd Pathfinding and Steering Using Flow Field Tiles"](http://www.gameaipro.com/GameAIPro/GameAIPro_Chapter23_Crowd_Pathfinding_and_Steering_Using_Flow_Field_Tiles.pdf),
  *Game AI Pro*, 2013: cost field → integration field → flow field for thousands of units; FR-16's field is the
  integration field and FR-18's `next` computes the flow direction on demand.
- Daniel Harabor and Alban Grastien, ["Online Graph Pruning for Pathfinding on Grid Maps"](https://users.cecs.anu.edu.au/~dharabor/data/papers/harabor-grastien-aaai11.pdf),
  AAAI 2011 (Jump Point Search) — a non-goal for weighted grids, a candidate follow-up for uniform ones.
- Adi Botea, Martin Müller and Jonathan Schaeffer, ["Near Optimal Hierarchical Path-Finding"](https://webdocs.cs.ualberta.ca/~mmueller/ps/hpastar.pdf),
  Journal of Game Development, 2004 (HPA\*) — a non-goal for the Old 3DS's level sizes.

## Decisions to take

Every question has a recommended answer; none is decided until the maintainer approves the spec.

- [x] **Module and header.** Options: (a) **a new optional module with one header (`<nnh/pathfinding.hpp>`, name fixed by
      the plan) over `<nnh/collision.hpp>`**; (b) inside the collision module and header; (c) inside the tilemap module.
      — **recommended: (a)** (FR-24). Collision stays a small pure module (spec 019 kept it to math), pathfinding needs
      no tilemap (XI), and `hello` links nothing.
- [x] **Where do searches read cells?** Options: (a) **a path map baked from a `Grid` (1 byte per cell: blocked or a
      weight), refreshed by `sync(grid[, region])` and edits**; (b) the `Grid` read live per neighbour (no copy; a
      tilemap callback and its per-layer loop for each of 8 neighbours of every expansion); (c) both. — **recommended:
      (a)** (FR-1 to FR-4). It is what Godot's `AStarGrid2D` and the A* Pathfinding Project do, keeps the inner loop on
      one compact array (cache, AC-7), is where weights and agent footprints live, and makes "a door opened" an
      explicit, cheap partial sync. (b) would make every expansion several function calls and give weights nowhere to
      live.
- [x] **One-way cells.** Options: (a) **blocked by default, a per-map setting makes them walkable**; (b) walkable by
      default. — **recommended: (a)** (FR-3). `collision::move` stops a box moving down onto a one-way cell, so a
      top-down path through one could be refused; a map built for a platformer's flying enemies sets it walkable.
- [x] **Costs.** Options: (a) **integer weights 1..255 per cell and integer straight/diagonal step costs (10/14 by
      default), all in 32 bits with a bound checked at creation**; (b) float weights and costs (Godot); (c) 16-bit
      weights. — **recommended: (a)** (FR-2, FR-6). Integers make ties exact and the same on every CPU, a byte per cell
      keeps maps small, and 255 levels are plenty for terrain; (b) makes ties depend on rounding order.
- [x] **Neighbours and corners.** — **recommended: 4, or 8 with Godot's three diagonal rules; OnlyIfNoObstacles by
      default** (FR-6). It is the rule under which a box of one cell, moved by `collision::move`, can follow every
      diagonal step, and it matches spec 019's "corners never leak".
- [x] **Heuristic.** Options: (a) **fixed by the neighbour mode (octile or Manhattan with the map's step costs), exact by
      default, with an integer scale for bounded-suboptimal speed**; (b) a choice of four heuristics as Godot; (c) a game
      callback. — **recommended: (a)** (FR-9). Euclidean needs a square root and Chebyshev underestimates diagonals with
      10/14 costs (slower, same paths); the scale gives the speed knob games actually want.
- [x] **Tie-breaking and determinism.** Options for equal f: (a) **lower h, then most recently pushed; fixed neighbour
      order; parent only on strictly lower g**; (b) lower h, then lowest cell index; (c) unspecified (whatever the heap
      does). — **recommended: (a)** (FR-10). Preferring cells near the goal expands fewer cells (Amit Patel's notes), and
      "most recent" keeps a search going in its current direction, which gives straighter raw paths than an index bias
      toward the top-left; (c) would let a heap change alter paths between versions and platforms.
- [x] **Objects.** Options: (a) **three objects — a path map (cells), searchers (one search's state each, many per map)
      and distance fields — plus a follower value**; (b) one object holding cells and one search (Godot); (c) searches
      as free functions over caller-given scratch. — **recommended: (a)** (FR-1, FR-7, FR-16, FR-19). Several agents can
      search one map at once (in slices) without copying cells, and a field coexists with searches; (b) forces one
      copy of the map per concurrent search.
- [x] **Searcher memory (D40).** Options: (a) **per-cell arrays sized to the map (g, parent, state) with generation
      stamps, ≤ 8 bytes per cell, open set sized so it can't overflow**; (b) a node pool and hash table sized to the
      expansion limit (memory follows the search, not the map; a hash lookup per neighbour). — **recommended: (a)**
      (FR-7, FR-12). O(1) access per neighbour with no hashing, no clearing per search, and a known size; the region
      offset and smaller searchers bound memory on huge maps. (b) is the fallback if a real game's map makes (a) too
      big.
- [x] **Bounded search and partial paths.** — **recommended: a per-search expansion limit; partial paths off by default,
      to the reached cell with the lowest h (ties lower g, then lower index); distinct statuses for NoPath and
      LimitReached** (FR-12, FR-13). Godot's semantics with the bound it lacks.
- [x] **Blocked start and blocked goal.** Options for the start: (a) **allowed, only entered cells count**; (b) no path
      (Godot). Goal: (a) **NoPath at once, or a partial search when asked**; (b) the nearest walkable cell automatically.
      — **recommended: (a) and (a)** (FR-11). An agent a door closed on must be able to leave; a blocked goal answered
      at once costs nothing, and "nearest walkable" is exactly what partial paths give when the game wants it.
- [x] **Time slicing and changes during a search.** Options for a map change during a sliced search: (a) **continue
      over the current cells and flag the result stale**; (b) cancel the search; (c) forbid (assert). — **recommended:
      (a)** (FR-14). Doors opening every few frames would starve (b); (c) makes slicing unusable in dynamic levels; the
      game decides whether a stale path is good enough.
- [x] **Distance fields: in this spec or a follow-up?** — **recommended: in this spec** (FR-16 to FR-18). The ROADMAP's
      "bounded" and the Old 3DS make crowds the case where per-agent A* fails; a field shares the map, the integer costs
      and the open set with searches, and its correctness is testable against the same reference. Stored direction
      vectors and flee maps stay out.
- [x] **Fields during a rebuild.** Options: (a) **double-buffered: queries answer from the last finished build while the
      next one is built in slices (2 × the distances' memory)**; (b) "not ready" until the rebuild finishes (half the
      memory; agents stand still for a few frames). — **recommended: (a)** (FR-17); a crowd that freezes whenever the
      player changes cell looks broken. A game short on memory builds in one call instead.
- [x] **Smoothing.** Options: (a) **string pulling over the path's cells with the map's walkability, weights no heavier
      than the replaced part and the diagonal rule's corner check, using spec 019's exact traversal**; (b) none (the game
      calls `line_of_sight` itself); (c) any-angle search (Theta\*). — **recommended: (a)** (FR-15). Raw grid paths
      zigzag; a game-written version would use the collision `Grid` (ignoring weights and the agent footprint); (c) is
      a different search (non-goal).
- [x] **Agent sizes.** Options: (a) **a footprint of 1..4 cells per map, applied at sync (erosion)**; (b) a follow-up;
      (c) a clearance value per cell with any agent size per search (more memory, a compare per neighbour). —
      **recommended: (a)** (FR-5). It costs nothing per search, the A* Pathfinding Project does the same, and one map
      per size is natural; (c) is a follow-up if games mix many sizes.
- [x] **Following a path.** Options: (a) **a small follower value that returns the point to walk toward, with an arrival
      radius and squared distances only**; (b) the game's job entirely. — **recommended: (a)** (FR-19). It is the part
      every game rewrites with the same off-by-one bugs, it is a few words and no dependency, and moving stays the
      game's (`collision::move`, tweens).
- [x] **Weights from Tiled.** Options: (a) **a weight callback at sync time plus a documented pattern reading a tile
      property (`path_cost`)**; (b) a `sync(tilemap, mask, property_name)` in the module (one source file including the
      tilemap header, as spec 021's `prefab_map.cpp`); (c) a new Tiled convention read by the converter. —
      **recommended: (a)**, with (b) as a convenience the plan may add if the pattern exceeds ~15 lines (FR-3). The
      module stays free of the tilemap and the map format is untouched.
- [x] **Jump Point Search and HPA\*.** — **recommended: not in this spec** (Non-goals). JPS doesn't work with weights and
      changes which equal-cost path is returned; HPA\* is for bigger maps than the Old 3DS holds comfortably.
- [x] **Events.** — **recommended: none; statuses and `step`'s return** (FR-21).
- [x] **Example and platformer.** Options: (a) **a new top-down `examples/pathfinding` (showcase, check, bench) and the
      platformer untouched**; (b) also enemies in an existing example (`examples/entities`' arena: chasers on a field);
      (c) pathfinding enemies in the platformer. — **recommended: (a)** (AC-4, AC-6), with (b) as an optional extra
      if the plan finds it shows the module better without changing that example's golden lines. (c) needs jump
      pathfinding (non-goal) and would change the replay.
- [x] **Old 3DS cost targets** — **recommended: AC-7's targets, confirmed by the plan, and set from the first console
      measurement with margin if they are still missed after optimisation** (the maintainer's bench policy).

## Open questions

- [ ] **Maximum map size for searchers and fields.** 1024 × 1024 (the tilemap's limit) costs 8 MiB per searcher.
      Recommended: allow it (the bound is memory, which `create` reports), document the region offset for big levels,
      and revisit with a real game.
- [ ] **Diagonal cost default: 10/14 or 2/3 or 5/7?** Recommended: 10/14 (error 1 % against √2; leaves room for weights
      in 32 bits); the plan confirms with the overflow bound.
- [ ] **A per-frame scheduler for many searchers** (round-robin `step` with a shared budget). Recommended: the example's
      pattern first, a follow-up if two games copy it.
- [ ] **Flee maps** (rescaled, negated fields so agents run from the player). Recommended: a documented pattern over two
      fields if it fits in a few lines; otherwise a follow-up.
- [ ] **Reusing spec 019's traversal for smoothing** (a `Grid` callback over the map or an internal entry point into the
      collision module). Recommended: the plan measures both on the PC and picks; the corner rule must be identical either
      way (AC-2 checks it).
- [ ] **A JPS option for uniform-cost maps** with its own goldens. Recommended: only if AC-7's searches miss after
      optimisation.
- [ ] **Smaller searchers for local searches** (a 32 × 32 window around an enemy on a big map). FR-7 needs a searcher at
      least as large as the map it searches. Recommended: keep that; a "window" search (cells outside the window treated
      as blocked) is a follow-up if games ask.
