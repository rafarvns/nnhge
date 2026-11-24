# pathfinding — paths on grids

Header: `<nnh/pathfinding.hpp>` · Spec: [025](../../specs/025-grid-pathfinding/spec.md) ·
Plan: [025](../../specs/025-grid-pathfinding/plan.md) · Decision: [D40](../architecture/decisions.md) ·
Example: [`examples/pathfinding`](../../examples/pathfinding/hero.cpp) · Grids: [collision.md](collision.md#grids) ·
Levels: [tilemap.md](tilemap.md), [guide](../guides/levels.md#slow-tiles-for-pathfinding)

A **path map** is a copy of a level's walkability, one byte per cell (blocked, or a weight 1..255), made from any
`collision::Grid` — the game's own array or callback, or a tilemap's (filtered) collision grid. **Searchers** run
bounded, time-sliceable A* over it and never allocate; a search can return a **partial path** to the closest cell it
reached; paths become a few straight **waypoints** (smoothing); **distance fields** give one shared answer for a crowd
heading to the same goal; a **follower** tells an agent which point to walk toward. Costs are integers and ties follow a
fixed order, so the same calls give **the same paths, bit for bit, on PC and 3DS**.

Optional (constitution XI): a game that doesn't call these links none of the module (a game calling only
`PathMap::create` and `find` links no sync, field, smoothing or follower code). The header includes only `<cstdint>`,
`<nnh/math.hpp>`, `<nnh/collision.hpp>` and `<nnh/result.hpp>`; `<nnh/collision.hpp>` and `<nnh/tilemap.hpp>` don't
change. No events: a finished search or field is known from `step()` and the queries.

```cpp
const nnh::collision::Grid grid = level.collision_grid();          // walls (a tilemap, or your own grid)
nnh::PathMap map = std::move(nnh::PathMap::create(nnh::path_map_desc(grid)).value()); // check it in real code
map.sync(grid, {ground_cost, &level});                              // walls blocked, weights from tile data
nnh::PathSearcher searcher = std::move(nnh::PathSearcher::create(map.width(), map.height()).value());

const nnh::PathResult r = searcher.find(map, map.cell_at(hero), map.cell_at(touch), {.partial = true});
nnh::CellPos cells[256];
const int n = nnh::smooth_path(map, cells, searcher.path(cells, 256), cells, 256); // waypoints, in place
nnh::Vec2 points[256];
follower.reset(points, nnh::cells_to_points(map, cells, n, points, 256), 3.0f);
// each frame: velocity toward follower.target(hero_centre), then collision::move
```

## Path maps

`PathMap::create(desc)` makes one block of tagged memory (`desc.tag`, default "path"): **width × height** cells,
1..1024 each, plus a blocked border (so searches never check bounds). A new map is all walkable with weight 1;
positions outside the map are never walkable. `path_map_desc(grid[, region])` copies a grid's origin and cell size and
sets the size; a **region** (`grid_x`, `grid_y`, size) makes a map of part of a huge level. Errors (`Result`):
`InvalidArgument` for sizes, costs, the 32-bit bound or the agent size; `OutOfMemory`. Maps move (also while searches
run over them); they don't copy.

| Call | What |
|------|------|
| `sync(grid[, weights])` | Every cell from grid cell `(grid_x + x, grid_y + y)`: **Empty** walkable, **Solid** blocked, **OneWay** blocked unless `desc.one_way_walkable`; beyond the grid `grid.outside` |
| `sync(grid, region[, weights])` | The same for a rectangle (a door that opened), clipped to the map |
| `set_cell(x, y, w)`, `fill(region, w)` | 1..255 or `kPathBlocked` |
| `set_diagonals(mode)` | The neighbour mode of later searches and builds |
| `blocked(x, y)`, `weight(x, y)` | What searches see (after the footprint) |
| `version()` | +1 per sync, set_cell, fill, set_diagonals: code keeping paths knows they may be out of date |
| `cell_at(world)`, `cell_center(cell)`, `contains(cell)` | Between world points and cells (spec 012's half-open cell rule) |

### Weights from Tiled

The weight callback (`PathWeights{fn, context}`) is called only by `sync`, once per walkable cell, row-major, with grid
coordinates; it returns 1..255 or `kPathBlocked` (anything else asserts; Release clamps it to 1..255). The pattern for a
Tiled level is a table per tile id, made once from a tile property, and a two-line callback
([`examples/pathfinding/level.cpp`](../../examples/pathfinding/level.cpp)):

```cpp
for (int id = 1; id <= map.tile_id_count(); ++id)                  // once, at load
    cost[id] = map.properties(nnh::PropertyOwner::tile(id)).get_int("path_cost", 1);
int ground_cost(const void* context, int x, int y) {                 // called by sync() only
    const auto& level = *static_cast<const Level*>(context);
    return level.cost[level.map.tile(level.ground, x, y).id];
}
```

One map per kind of mover: walkers from `collision_grid(kWalls | kWater)`, boats from `collision_grid(kWalls)`.

### Agent sizes

With `desc.agent_size = k` (1..4), a cell is walkable only if the whole k × k square whose top-left it is lies in the map
and is walkable in the cells as synced; its weight is the square's heaviest. Computed once per change (a partial sync
recomputes the squares overlapping it), never per search. Use one map per agent size; `cell_at`/`cell_center` work with
the agent's anchor and the centre of its footprint. Memory: + 1 byte per cell.

## Neighbours, costs and the 32-bit bound

`Diagonals` (Godot's names): **Never** — 4 neighbours; **Always**, **AtLeastOneWalkable**, **OnlyIfNoObstacles**
(default) — 8, a diagonal step passing the two orthogonal cells beside it (`A`, `B`) when:

```
 Always          AtLeastOneWalkable    OnlyIfNoObstacles
 . A             . A                   . A
 B ↘             B ↘                   B ↘
 A, B: ignored   A or B walkable       A and B walkable (no corner cutting:
                                       a box of one cell can follow every step)
```

Entering cell `c` costs `straight × weight(c)` or `diagonal × weight(c)` (defaults 10 and 14; `straight ≤ diagonal ≤
2 × straight`); the start cell's weight never counts. `create` refuses a map whose
`width × height × diagonal × 259` reaches 2³², so no cost or scaled heuristic can overflow (the defaults fit 1024 × 1024).

## Searches

A `PathSearcher` (`create(width, height[, tag])`) searches any map no larger than itself, one search at a time; make
several for several agents searching at once.

| Call | What |
|------|------|
| `find(map, from, to, options)` | A whole search; returns a `PathResult` |
| `begin(...)`, `step(n)`, `cancel()`, `running()`, `result()` | The same in slices: at most `n` expansions per `step`, `true` when finished |
| `path(out, capacity)` | The cells, start first, both ends included; writes the first `capacity`, returns the total (null + 0 counts); readable until the next search |
| `path_points(map, out, capacity)` | The same as world points |
| `reached(cell)`, `expanded(cell)`, `cost_to(cell)` | What the last (or running) search explored, for debug drawing |

`PathResult`: `status`, `stale`, `cost`, `cells`, `expanded`. Statuses: **Found** (the minimum cost, with scale 16),
**Partial** (options.partial: a path to the closest reached cell), **NoPath**, **LimitReached**, **Invalid** (misuse,
or the map destroyed during a sliced search), **Running**, **None**.

`PathOptions`: `max_expanded` (cells taken from the open set, the goal's included; 0 = the map's cell count),
`partial`, `heuristic_scale` (sixteenths, 16..64: the cost is at most scale/16 × the minimum, usually with fewer
expansions).

Rules: `from == to` is Found with one cell and cost 0 (blocked or not); a **blocked start** is fine (an agent a door
closed on can leave); a **blocked goal** is NoPath at once — with `partial`, a search toward it; `from`/`to` outside the
map is Invalid. **Partial paths** go to the reached cell with the lowest heuristic distance to the goal, ties to the
lower cost, then the lower row-major index: an island's shore, the cell in front of a closed door.

### The total order (determinism)

The search uses integers only. Among equal-cost paths the one returned is fixed: the open set is ordered by
`f = g + h`, then **lower h**, then **the most recent push**; neighbours are visited **N, E, S, W, NE, SE, SW, NW**; a
parent changes only on a **strictly** lower cost. This order is fixed forever (spec 025's golden checksum and a literal
reference A* in the tests hold it): a faster implementation must return the same cells, or be a new option.

### Time slicing and stale results

Any slice sizes give exactly `find`'s result. Round-robin a few searchers with `step(n)` to spread many agents' plans over
frames (the example's crowd does it for a field). If the map's `version()` changes while a search runs, the search goes
on over the current cells and the result is flagged `stale`; re-plan if that matters. Moving a `PathMap` during a sliced
search is fine; destroying it asserts, and in Release the search ends `Invalid` at its next `step`.

### Re-planning

Re-plan when the map's `version()` changed (a door), when the goal moved to another cell, or on a timer; keep the
expansion limit low for enemies that re-plan often, with `partial` on so they still move.

## Smoothing

`smooth_path(map, path, count, out, capacity)` keeps the first and last cells and, after each kept cell, the last cell of
the run of path cells a straight line between **cell centres** reaches — every cell it crosses walkable and no heavier
than the heaviest path cell it replaces, every grid corner it passes allowed by the map's diagonal rule (Never counts as
OnlyIfNoObstacles; Always doesn't check). The walk is spec 019's exact traversal (equal to `collision::cells_crossed`
cell for cell), in integers. `out` may be `path`. Greedy: a path that loses sight of a pillar and finds it again keeps one
waypoint more than a perfect one would. Safe for a **point**; a box smaller than a cell follows it by sliding with
`collision::move`; a bigger agent uses an agent-size map.

## Distance fields (crowds)

A `DistanceField` (`create({width, height, double_buffered, tag})`) is built over a map from one or more goal cells:
afterwards each cell holds the minimum cost for an agent standing there to reach the nearest goal (what a search from it
would cost), or `kUnreachable` (blocked, beyond `max_cost`, or cut off). Build in one call (`build`) or in slices
(`begin`, `step(n)` settling at most `n` cells, `building()`, `settled()`). **Double-buffered** fields (the default)
answer from the last finished build while the next one runs; single-buffered ones are `ready()` only when not building
(half the distance memory). `stale()` as for searches.

`next(map, at, out)` is the neighbour an agent at `at` steps into: the one minimising `step × weight + distance`, ties in
neighbour order, under the map's diagonal rule; false at a goal or an unreachable cell. `next_point(map, world, out)`
goes through `cell_at`/`cell_center` (false for a point off the map). Reads only: hundreds of agents per frame. Fleeing:
step to the neighbour with the **largest** distance — it runs into dead ends; a proper flee map is a follow-up.

## The follower

`PathFollower(points, count, radius)` follows world points the game owns: `target(position)` advances past every
waypoint within `radius` (the last one only arrives), returns the point to walk toward, and `arrived()` says when the
last is within the radius. Squared distances only: the same bits everywhere. Keep the radius ≥ the agent's speed per
frame. It never moves anything: turn the point into a velocity and call `collision::move`.

## Memory and cost

P = (width + 2) × (height + 2) padded cells.

| Object | Bytes | 64 × 64 | 128 × 128 | 1024 × 1024 |
|--------|-------|---------|-----------|-------------|
| `PathMap` | P (+ width × height for agent sizes 2–4) + 96 | 4.3 KB | 16.5 KB | 1 MiB |
| `PathSearcher` | 8 × P + 16 × width × height | 98 KB | 388 KB | 24 MiB |
| `DistanceField`, double-buffered | 10 × P + 8 × width × height | 74.5 KB | 293 KB | 18 MiB |
| `DistanceField`, single | 5 × P + 8 × width × height | 53 KB | 211 KB | 13 MiB |

A field keeps, per padded cell, a distance and a step byte (the answer `next` gives there): `next` with the map the
field was built over, unchanged since, is one byte read; after an edit, or with another map, it computes the answer
from the current cells (the same result the build would have stored on that map).

Nothing allocates after `create`; nothing logs in the normal path; the only game code called is the weight callback,
during `sync`. For a big level, search a **region** map around the action.

**Old 3DS costs (provisional).** Measured on an Old 3DS (Development build, the example's bench, 2026-11-24, console
run 1), then the field and `next` were optimised (spec 025 T10.3, no result change) and estimated from that run; the
budgets are set from the next console measurement ([plan 025](../../specs/025-grid-pathfinding/plan.md#optimisation-round-t103)):

| Workload | Console run 1 (avg) | After T10.3 (estimate) |
|----------|---------------------|------------------------|
| A*, per expanded cell (`rooms(128, 128)`, weights 1/3/8) | 4.3 µs | ≈ 4.1 µs |
| 32 re-plans of 10–30 cells (4 518 expansions) | 15.1 ms | ≈ 14.9 ms |
| one search across a 64 × 64 maze (494 expansions) | 0.72 ms | unchanged |
| a 64 × 64 field, one call (2 872 cells) | 8.3 ms | ≈ 6.3 ms |
| the same in slices of 512 cells, worst slice | 1.65 ms | ≈ 1.3 ms |
| `next` for 256 scattered agents | 0.77 ms | ≈ 0.17 ms |
| smoothing a 200-cell path | 0.22 ms | unchanged |
| `sync` of 64 × 64 from a tilemap with a `path_cost` table | 3.9 ms | unchanged |

So an expansion costs about 4 µs on weighted levels (about 1.5 µs in a corridor maze, where the heuristic is exact):
keep searches bounded (`max_expanded`) or sliced, prefer one field for a crowd (its `next` is a byte read per agent),
and remember that heavy weights make the heuristic weak (a level averaging weight 2 expands several times the cells of
an all-1 level).

## Misuse

Misuse asserts in Debug/Development (with an error on the `path` log channel) and is harmless in Release: cells, regions
or goals outside the map (nothing changed / blocked / `Invalid` / the goal ignored), weights 0 or above 255, a searcher
or field smaller than the map, bad buffers or capacities, a heuristic scale outside 16..64, NaN or infinite world points
(`cell_at` → `{-1, -1}`), `step()` with nothing begun, calls on empty or moved-from objects.
