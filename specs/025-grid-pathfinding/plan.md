# Plan 025: Pathfinding on grids

Status: implemented 2026-11-24 (stages 0-9 and 11; Old 3DS T10.2 run 1 done, T10.3 optimisation done on the PC, console
rerun pending). Approved 2026-11-24 (overnight pre-approval)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decision D40 ([decisions.md](../../docs/architecture/decisions.md))
Uses spec 012 (`collision::Grid`, `Cell`, spec 012's half-open cell rule; tilemaps, tile properties, `set_tile`), spec
019 (`collision_grid(mask)`, the exact grid walk and its corner rule, `cells_crossed`), spec 002 (asserts, logs,
`Result`, tagged memory, profiler), spec 013 (scripts), spec 018 only in the example (its camera), spec 014 not at all.

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ⚠️→✅ | Pressure point: "the same paths bit for bit". Answered: searches, fields, smoothing and footprints are integer-only (no float, no division in any loop); the total order of FR-10 is implemented once and checked against a literal reference A* (AC-2); the float parts (cell ↔ world, the follower) are compiled with `-ffp-contract=off` and use spec 012's own cell rule; AC-3's golden on every host compiler and the console |
| II 3DS constraint | ⚠️→✅ | Pressure points: AC-7's targets predate any measurement (and specs 019–023 showed estimates 1.3–2× low), and the searcher/field memory per cell. Answered: a layout planned for the 16 KB L1 (one interleaved 8-byte record per padded cell, the map's bytes apart, heap entries moved as 4-word `ldm`/`stm`), no bounds checks (a blocked border), a buffer that skips heap work for the best child, an ARM pre-check with miss counts before the console (T7.6), and the measurement-with-margin policy. Memory is stated exactly (24 B per cell per searcher, 16 per double-buffered field: above the spec's estimate, see "Data and memory") and returned by `create` |
| III Build speed | ✅ | One header (`<cstdint>`, `<nnh/math.hpp>`, `<nnh/collision.hpp>`, `<nnh/result.hpp>`), no templates in it, no standard containers; `-fsyntax-only` within 0.05 s of `<nnh/collision.hpp>` + `<nnh/result.hpp>` (T0.1, T8.4) |
| IV Event-driven | ✅ | No events (FR-21): statuses and `step()`'s return are queried |
| V No hidden costs | ✅ | No exceptions (`Result` at creation), RTTI, `virtual` or allocation after creation; the only game code called is the sync weight callback (a function pointer); no logging in the normal path |
| VI Spec + test first | ✅ | Everything is pure logic, tested first on the fake platform against hand-drawn cases, a brute-force Dijkstra and a literal reference A* (AC-2), and a golden script (AC-3); mutation pass (stage 8) |
| VII Small files | ✅ | One module (`pathfinding`, ≈ 24 sources ≤ 200 lines each), one public header |
| VIII Documented API | ✅ | `docs/api/pathfinding.md` (pictures of the diagonal rules), collision, tilemap and levels pages linked, `examples/pathfinding`, D40 |
| IX Clean and legal | ✅ | Own code from the published algorithms and documentation in the spec's prior art; original art |
| X Safe by default | ✅ | No parser or file format; every search bounded in time (an expansion limit) and memory (capacities fixed at creation); misuse asserts in Debug/Development and is harmless in Release (FR-23); a fuzz target over random operation streams |
| XI Opt-in, never in the way | ✅ | A separate module linked only when called; `<nnh/collision.hpp>` and `<nnh/tilemap.hpp>` byte for byte unchanged; `hello` unchanged; the platformer untouched (AC-6) |

## Design

### Overview

```
<nnh/pathfinding.hpp>
  PathMap ─── 1 byte per cell (0 = blocked, 1..255 = weight), a blocked border, version
     ▲  sync(grid[, region], weights) · set_cell · fill · set_diagonals        (agent size 2–4: + source bytes, eroded)
     │  read by (the block, not the object)
  PathSearcher ── per padded cell {g, stamp|closed|dir} 8 B · open heap 16 B entries · one-entry best-child buffer
     find / begin · step(n) · cancel · result · path · path_points · reached/expanded/cost_to
  DistanceField ── front + back u32 distances · Dijkstra heap 8 B entries
     build / begin · step(n) · distance · next · next_point
  smooth_path (integer centre-to-centre walk, spec 019's rules)  ·  cells_to_points  ·  PathFollower (floats, no sqrt)

src/pathfinding/: path_block · path_check · map_create · map_sync · map_edit · footprint · map_geometry ·
                  neighbors.hpp · heuristic.hpp · open_heap · searcher_create · search_begin · search_step ·
                  search_path · smooth_walk.hpp · smooth · field_create · field_build · field_query · follower
```

### Decision D40: a baked path map shared by per-search searchers with generation stamps, a fixed total order, Dijkstra fields

**Where searches read cells.** A `PathMap` is one tagged block the game owns: a header (geometry, costs, diagonal
mode, agent size, version, the padded stride and the eight neighbour offsets) and a row-major byte per cell with a
one-cell **blocked border** all round (stride `W + 2`, `H + 2` rows), so a neighbour read never needs a bounds check:
`0` = blocked, `1..255` = the weight of entering the cell. Agent sizes 2–4 add the **source** bytes (`W × H`, the
cells as synced or set) and store the eroded cells (FR-5). Cells come from any `collision::Grid` through
`collision/grid_internal.hpp`'s inline `detail::cell_at(grid, x, y)` (the same reads, `outside` beyond the edges, as
`move` and the rays), and world points become cells through its `detail::first_cell` (spec 012's half-open rule):
`<nnh/collision.hpp>` and `<nnh/tilemap.hpp>` don't change, and D34's filtered grids are used as they are.

**Searchers.** A `PathSearcher` is one tagged block sized for maps up to `W × H`; it runs one search at a time over
any such map and keeps its state across `step()` calls. Per **padded** cell it holds an 8-byte record
`{u32 g; u32 meta}` with `meta = stamp (28 bits) | closed (1) | dir (3)`, `dir` the direction the cell was entered
from. A search starts by incrementing the stamp: a record whose stamp differs is "not reached" — nothing is cleared
per search; when the stamp would wrap (2²⁸ searches) every `meta` is zeroed once. The **open set** is a binary heap of
16-byte entries `{u32 h; u32 f; u32 seq; u32 cell}` ordered by the 64-bit key `(f, h)` ascending, then `seq`
descending (most recent first), with **lazy deletion**: a cell is pushed again only when its `g` strictly drops, and an
entry is stale when its cell is closed or `f − h ≠ g`. The heap has room for `W × H` entries; when a push finds it
full, it is **compacted** (stale entries dropped, at most one valid entry per open cell remains, then heapified), which
can never fail because open cells ≤ walkable cells < `W × H` while a cell is being expanded. In front of the heap sits
a **one-entry buffer**: of the children pushed by an expansion, the best is held aside; the next pop takes it if its
key beats the heap's top, else it goes into the heap. The pop order is the total order either way, so the buffer,
compaction and the heap's layout never change a result.

**The total order (FR-10), fixed forever once AC-3's golden is recorded.** f = g + h' (h' the heuristic at the
search's scale), then lower h', then the most recent push (`seq`, one counter per search incremented per push, in
neighbour order); neighbours visited N, E, S, W, NE, SE, SW, NW (the spec's order, kept); a parent changes only on a
strictly lower g. A later optimisation (a faster heap, neighbour masks, JPS) must return the same cells, costs and
expansion counts or become a new option with new goldens; AC-2's literal reference A* (a vector scanned for the
minimum key, decrease-key in place, no buffer) guards it.

**Fields.** A `DistanceField` is Dijkstra from goal cells over reversed steps (entering cell c costs `step × w(c)`
from every predecessor, so a settled cell's two candidates `d + S·w(c)` and `d + D·w(c)` are computed once and the
relaxations need no multiply): two `u32` distance arrays per padded cell (front = the last finished build, back = the
one being built) and a lazily deleted heap of 8-byte entries `{u32 cell; u32 d}` (key `(d, cell)`, `W × H` entries,
compacted when full). `next` reads the neighbours (no stored directions: a non-goal).

**Smoothing** copies spec 019's traversal rules into an integer walk between cell centres in cell space (FR-15's
choice: a copy over the map's bytes, not the collision walk through a `Grid` callback), proven equal to
`collision::cells_crossed` cell for cell and corner for corner (T4.1).

**Map lifetime during sliced work.** Searchers and fields reference the map's **block**, not the `PathMap` object, and
count themselves in the block while a sliced search or build runs. Moving a `PathMap` during one is harmless and
supported. Destroying it then asserts (Debug/Development); in Release the block is kept ("doomed") and the next
`step()` ends with `Invalid` and frees it when the last user lets go — never a dangling read (spec 021's doomed
library, the same idea).

**Rejected:** reading the `Grid` live per neighbour (several calls per expansion; no place for weights or footprints;
the spec's decision); float costs; clearing searcher memory per search; a node pool with a hash table (spec decision,
kept as a follow-up for huge maps); an indexed heap with decrease-key (a heap position per cell, rewritten on every
sift: random writes into the record array, no smaller); entries without `h` and `seq` (tie compares would read the
record array: a miss per tie, and ties are common on grids); heaps whose ties fall to their layout (paths would change
between versions and platforms); a 4-byte "neighbour mask" per cell now (a speed-up that changes no result: kept as
the first mitigation, risk 1); reusing spec 019's `GridWalk` through a unit `Grid` (an internal include of its
double/int64 machinery for segments that are always on a half-integer lattice); the true furthest visible cell in
smoothing (O(n³) worst case); a per-cell clearance value for any agent size (spec option c); events.

### Public API (sketch)

```cpp
// <nnh/pathfinding.hpp>: <cstdint>, <nnh/math.hpp>, <nnh/collision.hpp>, <nnh/result.hpp>. Optional (XI): a game that
// doesn't call these links none of the module. Nothing allocates after create(); nothing calls game code except the
// sync weight callback; nothing logs in the normal path; nothing here needs a frame.
namespace nnh {
struct MemTag; // <nnh/memory.hpp>
namespace detail { struct PathMapBlock; struct PathSearchBlock; struct FieldBlock; }

/// A cell of a path map: (0, 0) is the map's first cell (grid cell (grid_x, grid_y) of the grid it mirrors).
struct CellPos {
    int x = 0;
    int y = 0;
    friend constexpr bool operator==(CellPos, CellPos) = default;
};
/// A rectangle of cells in map coordinates.
struct CellRect { int x = 0, y = 0, w = 0, h = 0; };

inline constexpr int kPathBlocked = -1;                     ///< a cell value: not walkable
inline constexpr std::uint32_t kUnreachable = 0xFFFFFFFFu;  ///< a cost or distance nobody reached

/// Godot's diagonal modes. Never: 4 neighbours. The others: 8, a diagonal step past the two cells beside it allowed
/// always / when one is walkable / when both are (no corner cutting: spec 019's "corners never leak").
enum class Diagonals : std::uint8_t { Never, Always, AtLeastOneWalkable, OnlyIfNoObstacles };

struct PathMapDesc {
    int width = 0, height = 0;          ///< cells, 1..1024 each
    Vec2 origin{};                      ///< the grid's origin: world position of grid cell (0, 0)'s top-left
    float cell_w = 16, cell_h = 16;     ///< pixels, > 0
    int grid_x = 0, grid_y = 0;         ///< the grid cell that is map cell (0, 0) (a region of a bigger level)
    Diagonals diagonals = Diagonals::OnlyIfNoObstacles;
    int straight_cost = 10;             ///< ≥ 1
    int diagonal_cost = 14;             ///< straight..2 × straight; width × height × diagonal × 259 < 2³²
    int agent_size = 1;                 ///< 1..4: a k × k agent, anchored at its top-left cell
    bool one_way_walkable = false;      ///< sync: OneWay cells walkable (a flyer's map) instead of blocked
    MemTag* tag = nullptr;              ///< default: the engine's "path" tag (Heap)
};
/// A desc mirroring `region` of `grid` (default: the whole grid) with its geometry.
PathMapDesc path_map_desc(const collision::Grid& grid, CellRect region = {0, 0, -1, -1});

/// Weights from the game's own data, called during sync() only, once per walkable grid cell, with grid coordinates:
/// return 1..255 or kPathBlocked. (The documented pattern reads a per-tile-id table built from a `path_cost` property.)
struct PathWeights {
    int (*weight)(const void* context, int grid_x, int grid_y) = nullptr;
    const void* context = nullptr;
};

/// The searchable cells (FR-1..FR-6). One block; movable, not copyable. Map (0, 0)..(width−1, height−1); everything
/// outside is blocked. Every sync/set/fill/set_diagonals call increments version().
class PathMap {
public:
    /// InvalidArgument (sizes, costs, the 32-bit bound, agent size, cell size), OutOfMemory.
    static Result<PathMap> create(const PathMapDesc& desc);
    PathMap() = default;
    PathMap(PathMap&& other) noexcept;
    PathMap& operator=(PathMap&& other) noexcept;
    PathMap(const PathMap&) = delete;
    PathMap& operator=(const PathMap&) = delete;
    ~PathMap();

    void sync(const collision::Grid& grid, const PathWeights& weights = {});                  ///< every cell
    void sync(const collision::Grid& grid, CellRect region, const PathWeights& weights = {}); ///< a door: only these
    void set_cell(int x, int y, int weight);   ///< 1..255 or kPathBlocked (agent size > 1: the source cell)
    void fill(CellRect region, int weight);    ///< clipped to the map
    void set_diagonals(Diagonals diagonals);

    bool blocked(int x, int y) const;          ///< what searches see (after the footprint); true outside
    int weight(int x, int y) const;            ///< 1..255, or kPathBlocked
    std::uint32_t version() const;
    int width() const;
    int height() const;
    int agent_size() const;
    Diagonals diagonals() const;
    int straight_cost() const;
    int diagonal_cost() const;
    bool contains(CellPos cell) const;
    Vec2 cell_center(CellPos cell) const;      ///< the centre of the agent's k × k footprint, world pixels
    CellPos cell_at(Vec2 world) const;         ///< the anchor of an agent centred at `world` (may be outside: contains())
    bool valid() const;                        ///< false for a default-constructed or moved-from map

private:
    detail::PathMapBlock* block_ = nullptr;
};

enum class PathStatus : std::uint8_t {
    None,         ///< no search yet
    Running,      ///< a sliced search between begin() and its last step()
    Found,        ///< a path to `to`; its cost is the minimum (heuristic scale 16)
    Partial,      ///< options.partial: a path to the reached cell closest to `to`
    NoPath,       ///< `to` can't be reached (blocked, or every reachable cell expanded)
    LimitReached, ///< options.max_expanded cells expanded first
    Invalid,      ///< misuse (FR-23) or the map destroyed during a sliced search
};
struct PathOptions {
    int max_expanded = 0;      ///< cells taken from the open set (the goal's included); 0: the map's cell count
    bool partial = false;      ///< FR-13
    int heuristic_scale = 16;  ///< sixteenths, 16..64: cost ≤ scale/16 × the minimum, fewer expansions
};
struct PathResult {
    PathStatus status = PathStatus::None;
    bool stale = false;        ///< the map's version changed between begin() and the end
    std::uint32_t cost = 0;    ///< Σ step × weight of the entered cells
    int cells = 0;             ///< path length, both ends included (0 without a path)
    int expanded = 0;
};

/// One search at a time over any map no larger than the searcher (FR-7..FR-14, FR-20). One block; movable.
class PathSearcher {
public:
    static Result<PathSearcher> create(int width, int height, MemTag* tag = nullptr);
    PathSearcher() = default;
    PathSearcher(PathSearcher&&) noexcept;
    PathSearcher& operator=(PathSearcher&&) noexcept;
    PathSearcher(const PathSearcher&) = delete;
    PathSearcher& operator=(const PathSearcher&) = delete;
    ~PathSearcher();

    PathResult find(const PathMap& map, CellPos from, CellPos to, const PathOptions& options = {});
    void begin(const PathMap& map, CellPos from, CellPos to, const PathOptions& options = {}); ///< cancels a running one
    bool step(int max_expanded);   ///< ≥ 0; true when finished (also when nothing runs)
    void cancel();                 ///< result() becomes None
    bool running() const;
    PathResult result() const;

    /// The last search's path, start first: writes min(total, capacity) cells, returns the total (null + 0 counts).
    /// Readable until the next begin()/find(). No path: 0.
    int path(CellPos* out, int capacity) const;
    int path_points(const PathMap& map, Vec2* out, int capacity) const; ///< the same, as map.cell_center()s

    bool reached(CellPos cell) const;         ///< given a cost by the last (or running) search
    bool expanded(CellPos cell) const;        ///< taken from the open set
    std::uint32_t cost_to(CellPos cell) const; ///< its g, kUnreachable if not reached
    int width() const;
    int height() const;

private:
    detail::PathSearchBlock* block_ = nullptr;
};

/// Waypoints of `path` (FR-15): the first and last cells and, after each kept cell, the last cell of the run of path
/// cells a straight centre-to-centre segment reaches (walkable cells, none heavier than the heaviest path cell it
/// replaces, the map's corner rule). Same buffer contract as path(); `out` may be `path` (in place).
int smooth_path(const PathMap& map, const CellPos* path, int count, CellPos* out, int capacity);
int cells_to_points(const PathMap& map, const CellPos* cells, int count, Vec2* out, int capacity);

struct DistanceFieldDesc {
    int width = 0, height = 0;    ///< the largest map it is built over, 1..1024 each
    bool double_buffered = true;  ///< queries answer from the last finished build while the next one runs (FR-17)
    MemTag* tag = nullptr;
};
/// Dijkstra from goal cells (FR-16..FR-18). One block; movable.
class DistanceField {
public:
    static Result<DistanceField> create(const DistanceFieldDesc& desc);
    // (default, move, no copy, destructor as PathSearcher)

    /// Goals: map cells (outside: misuse; blocked: ignored). Cells costing more than max_cost stay unreachable.
    void build(const PathMap& map, const CellPos* goals, int count, std::uint32_t max_cost = kUnreachable);
    void begin(const PathMap& map, const CellPos* goals, int count, std::uint32_t max_cost = kUnreachable);
    bool step(int max_cells);      ///< settles at most max_cells cells; true when finished
    void cancel();
    bool building() const;
    bool ready() const;            ///< a finished build answers queries (single-buffered: false while building)
    bool stale() const;            ///< the answering build saw its map's version change
    int settled() const;           ///< cells settled by the running (or last) build: progress

    std::uint32_t distance(CellPos cell) const;                     ///< kUnreachable: blocked, too far, not ready
    bool next(const PathMap& map, CellPos at, CellPos& out) const;  ///< false at a goal or an unreachable cell
    bool next_point(const PathMap& map, Vec2 at, Vec2& out) const;  ///< the same through cell_at/cell_center

private:
    detail::FieldBlock* block_ = nullptr;
};

/// Follows world waypoints the game owns (FR-19): + − × and compares on squared distances only. 20 bytes (24 on
/// 64-bit); no allocation; never moves anything.
class PathFollower {
public:
    PathFollower() = default;
    PathFollower(const Vec2* points, int count, float arrival_radius);
    void reset(const Vec2* points, int count, float arrival_radius);
    /// Advances past every waypoint within the radius of `position` (the last one only arrives) and returns the point
    /// to walk toward (`position` itself when the list is empty).
    Vec2 target(Vec2 position);
    bool arrived() const;          ///< within the radius of the last point (or an empty list)
    int index() const;             ///< the waypoint walked toward

private:
    const Vec2* points_ = nullptr;
    int count_ = 0;
    int next_ = 0;
    float radius2_ = 0;
    bool arrived_ = true;
};
} // namespace nnh
```

Exact names may still change while building; stage notes record any change. The header has no templates.

### Events introduced

| Event | Published by | Payload | Typical listeners |
|-------|--------------|---------|-------------------|
| (none) | — | — | — |

FR-21: a finished sliced search or field is known from `step()`'s return and the queries.

### Semantics fixed by the plan

**Map creation.** `width`, `height` 1..1024; `cell_w`, `cell_h` finite and > 0; `straight_cost ≥ 1`; `straight ≤
diagonal ≤ 2 × straight`; `agent_size` 1..4; and `width × height × diagonal_cost × 259 < 2³²` (computed in 64 bits):
FR-6's bound `cells × diagonal × 255` plus room for the largest scaled heuristic (h' ≤ 4 × diagonal × max(W, H)), so
neither g nor f can overflow 32 bits. The defaults fit 1024 × 1024 (3.80 × 10⁹). A new map is all weight 1 (the
border 0). `path_map_desc(grid, region)` copies the grid's origin and cell size and sets `grid_x/grid_y/width/height`
(a region `w < 0` means "to the grid's edge").

**Sync.** For each map cell (x, y) of the rectangle, the grid cell `(grid_x + x, grid_y + y)` is read once with
`detail::cell_at` (beyond the grid: `grid.outside`). Solid → blocked; OneWay → blocked unless `one_way_walkable`;
otherwise walkable, weight `weights.weight(context, gx, gy)` if a callback is given (called exactly once per such
cell, row-major) else 1. A callback value outside 1..255 that isn't `kPathBlocked` asserts; Release clamps it to 1..255.
With agent size k > 1 the values go to the source bytes, then every cell whose k × k square overlaps the rectangle
(the rectangle grown by k − 1 up and left, clipped) is recomputed: blocked if any square cell is blocked or outside the
map, else the largest weight of the square (k² reads per cell: ≤ 16). `set_cell`/`fill` follow the same path. The
version increments once per call.

**Neighbours and costs.** Direction index d = 0..7 is N, E, S, W, NE, SE, SW, NW (`Never` uses 0..3); offsets
`−S, +1, +S, −1, −S+1, S+1, S−1, −S−1` with S the padded stride. Entering n from c costs `straight × w(n)` or
`diagonal × w(n)`. A diagonal step's side cells are the two orthogonal neighbours it passes (NE: N and E): Always — not
read; AtLeastOneWalkable — one walkable; OnlyIfNoObstacles — both walkable. The rule is symmetric (the same two cells
from either end), so fields and searches agree.

**Heuristic.** `dx = |x − tx|`, `dy = |y − ty|`; Never: `h = straight × (dx + dy)`; else `h = straight × (max − min) +
diagonal × min` (octile). Admissible and consistent (weights ≥ 1), so with scale 16 a Found cost is minimal. Scaled:
`h' = (h × scale) >> 4` (32-bit: h × 64 < 2³² by the creation bound), the value in f and in the tie.

**A search** (`search_begin.cpp`, `search_step.cpp`). `begin` checks (FR-23), captures the map's block, stride, size,
version and costs, increments the stamp, empties the heap and the buffer, sets `seq = 0`. Then, in order:
`from` or `to` outside the map → `Invalid` (assert); `from == to` → `Found`, cost 0, 1 cell, 0 expanded (blocked or
not); `to` blocked and not `partial` → `NoPath`, 0 expanded; otherwise the start is reached with g = 0 (a blocked start
is fine: only entered cells count) and pushed. Each `step(n)` loops: if `expanded == limit` → `LimitReached` (or
`Partial`); pop (buffer or heap; stale entries skipped without counting); none left → `NoPath` (or `Partial`); mark
closed, `expanded++`; the goal → `Found`; else for each neighbour in order: blocked (byte 0) → skip; diagonal rule;
record not reached this stamp → reached (g, dir), push; reached, not closed and `ng < g` → update (g, dir), push;
closed → skip (no reopening, also with a scaled heuristic: Likhachev's bound still holds). Every assignment of a g
with `partial` on compares `(h, g, row-major index)` against the best so far (unscaled h; the start included). The
loop stops after n valid pops. The end sets the status, `stale = (map version ≠ the captured one)`, and walks the
parent directions once from the end cell to count `cells`. `find()` is `begin()` + `step(INT_MAX)`. `path()` walks the
directions again (coordinates updated with dx/dy, no division) and writes the first `capacity` cells from the start.

**Partial paths (FR-13).** The closest cell is the reached cell with the lowest unscaled h, ties to the lower final g,
then the lower row-major index (a cell's g only drops, so comparing at every assignment gives the argmin over final
values). Its path and cost are the result; a search that reached only the start returns `Partial` with 1 cell.

**Smoothing (FR-15).** Anchor a = 0; for j = a + 2, a + 3, …: the segment from the centre of `path[a]` to the centre
of `path[j]` (cell space, coordinates doubled so centres are odd integers and lines even) is walked with spec 019's
rules: it never lies on a grid line; at an exact corner the x-neighbour, then the y-neighbour, then the diagonal cell
are visited (the side cells only touched at the point). The segment passes when every cell it crosses through its
interior after the first is walkable and no heavier than `max(w(path[a+1..j]))`, and at every corner the side cells
satisfy the map's diagonal rule (Always: not read). At the first j that fails, `path[j − 1]` is kept and becomes the
anchor ("the furthest" = the end of the first visible run: bounded by Σ run² cell visits; a true furthest search would
be cubic). The last cell is always kept; `j = a + 1` always passes (a valid step is a valid segment). The walk compares
`(2a + 1) · |dy|` with `(2b + 1) · |dx|` in 32 bits (≤ 2049 × 2046). Safe for a **point**; a box follows a segment
exactly only if it is smaller than a cell and slides (`collision::move`); documented (risk 4).

**Fields (FR-16..FR-18).** `begin` checks, fills the back buffer with `kUnreachable` (one `memset`-like loop over the
padded cells), pushes every walkable goal with distance 0 (duplicates harmless), records the version. Each `step(n)`
settles up to n cells: pop `(d, c)`; stale if `d ≠ back[c]`; `d > max_cost` never happens (not pushed); settled++;
`ds = d + S·w(c)`, `dd = d + D·w(c)`; for each neighbour p in order: blocked → skip; diagonal rule between p and c;
candidate ≤ `max_cost` and `< back[p]` → store, push. Finished: double-buffered → front and back swap, `ready()`
true; single → the only buffer answers, `ready()` false while building. `stale()` = the answering build's version
check. `next(at)`: `d = distance(at)`; 0 or unreachable → false; else the neighbour minimising `step × w(n) +
distance(n)` (reachable, rule allowed), the first in order on ties. Blocked cells and blocked goals are unreachable,
so "field = search cost from the cell" (AC-2) holds for every walkable cell.

**Follower (FR-19).** `target(p)`: empty list → arrived, returns p; while `next < count − 1` and `|p − points[next]|²
≤ r²` → next++; arrived when `next == count − 1` and within r; returns `points[next]`. A NaN/inf position or radius
asserts (Release: the current target, nothing advances). Radius-only by design: the docs say "radius ≥ the agent's
speed per frame".

**Misuse (FR-23)**, each an assert in Debug/Development with a message naming the call, then in Release:

| Misuse | Release |
|--------|---------|
| any call on an invalid (default or moved-from) object | nothing; queries return blocked / kUnreachable / 0 / None |
| cell or region outside the map (set_cell, fill, blocked, weight, from, to, goals, inspection) | nothing changed / blocked / `Invalid` / goal ignored |
| weight 0 or > 255 to set_cell/fill (not kPathBlocked) | nothing changed |
| a searcher or field smaller than the map | `Invalid` / build not started |
| null buffer with capacity > 0, negative capacity, count, limit or step size | 0 cells / `Invalid` / nothing |
| heuristic scale outside 16..64 | `Invalid` |
| NaN or infinite world point (cell_at, next_point, follower) | `cell_at` → `{-1, -1}` (outside) / `next_point` false / the follower unchanged |
| step() without begin() | true, result unchanged |
| the map destroyed during a sliced search or build | assert in the map's destructor; Release: the block lives, the next step ends `Invalid` and frees it |

### Data and memory

P = (W + 2) × (H + 2) padded cells.

| Object | Per cell | 64 × 64 | 128 × 128 | 1024 × 1024 | Notes |
|--------|----------|---------|-----------|-------------|-------|
| `PathMap`, agent 1 | 1 B × P | 4.3 KB | 16.5 KB | 1.0 MiB | + 96-byte header; FR-1's "1 byte" |
| `PathMap`, agent 2–4 | + 1 B × W·H | + 4 KB | + 16 KB | + 1 MiB | the source bytes |
| `PathSearcher` | 8 B × P + 16 B × W·H | 98 KB | 388 KB | 24.0 MiB | records + heap; + 96-byte header |
| `DistanceField`, double | 10 B × P + 8 B × W·H | 74.5 KB | 293 KB | 18.0 MiB | front, back (4 B distance + 1 B step each, T10.3; was 8 B × P), heap |
| `DistanceField`, single | 5 B × P + 8 B × W·H | 53 KB | 211 KB | 13.0 MiB | (was 4 B × P) |
| `PathResult` / `PathFollower` | — | 16 B / 20 B (3DS), 24 B (64-bit) | | | values |

**Against the spec's estimate.** FR-7 expected ≤ 8 bytes per cell for a searcher and the spec's decision (a) says
"per-cell arrays (g, parent, state) ≤ 8 bytes per cell, open set sized so it can't overflow": the per-cell records are
8 bytes; the open set is 16 more. Constraint II's "128 KB per searcher at 128 × 128" assumed the open set inside the 8
bytes, which no exact design reaches: an open entry needs f and h (ties), a push sequence (FR-10's "most recent") and
the cell — 13+ bytes — and up to ≈ 4/5 of the cells can be open at once (an 8-connected closed set of n cells has up to
4n + 4 neighbours). Fields: FR-16 expected ≤ 4 bytes of distances (8 double-buffered); the Dijkstra heap adds 8. These
are flagged for the maintainer (decision 9, risk 2) with the smaller alternatives: 12-byte entries with the sequence
renumbered at compaction (20 B per searcher cell), a heap sized to the proven 4/5 bound (≈ 20.8 B), or spec decision
(b)'s node pool sized to the expansion limit for huge maps. The example uses ≈ 100 KB in all; a 1024 × 1024 searcher
(24 MiB) is the docs' "use a region" case.

Fixed per object: none per frame; `find`, `step`, `smooth_path`, `next` use a few words of stack; nothing allocates
after `create`.

### ARM11 cost plan (AC-7)

Lessons from specs 019–023 on the Old 3DS (268 MHz ARM11, 16 KB L1D with 32-byte lines, no L2, no prefetcher, ≈ 110
cycles per miss): instruction counts were 1.3–2× low, so misses are counted, the estimates use ≈ 1.9 cycles per
instruction (spec 020/021 calibration) plus ≈ 110 cycles per random miss (≈ 40 with a `pld` well ahead), and the upper
end is multiplied by 1.3. Built in:

1. **Locality.** The search reads three rows of two arrays per expansion: the map's bytes (shared by every searcher, 1
   B per cell) and the searcher's 8-byte records (g and meta interleaved: one access per neighbour). A 3 × 3
   neighbourhood is ≈ 3 lines of bytes and ≈ 5 lines of records; consecutive expansions sit on the frontier, so most
   are hits. A 64 × 64 searcher's records are 34 KB (> L1): the frontier, not the map, is the working set. The heap's
   top levels stay in L1; entries move as 16-byte `ldm`/`stm` through a hole (one write per level, no swaps).
2. **No bounds checks** (the border), **no division** anywhere in search, field, smoothing or footprint loops: a cell
   travels as `(y << 16) | x`, its index is one `mla` with the stride, path walks update x and y by the direction's
   deltas; the heuristic is `abs`, `min`/`max`, two multiplies; no `%`. Bench indices use masks.
3. **Binary heap tuned for ARM11**: the `(f, h)` key compares as one 64-bit value (`cmp`/`sbcs`), `seq` only on a full
   tie; the best-child buffer skips the push and the pop for the child that wins (typical of straight progress).
4. **The neighbour loop is unrolled and specialised** per diagonal mode (4 instantiations of one template in
   `search_step.cpp`), the orthogonal bytes loaded first and reused for the side checks; `step × w` is one `mul` (the
   costs are run-time values, so no shift-add chains; T7.6 checks GCC didn't make one).
5. **Fields** compute `d + S·w(c)` and `d + D·w(c)` once per settled cell: the eight relaxations are loads, compares and
   stores. `next` is ≈ 9 instructions per neighbour plus its lines.
6. **No `vmrs`, no `__aeabi_*`, no `memcpy`/`memset`** in hot paths (the field's begin fill and the stamp wrap are
   loops over words, outside the measured slices' steps except begin). Floats only in geometry and the follower.

Estimates (1 ms = 268 000 cycles):

| AC-7 workload | Hot path | Estimate (upper × 1.3) | Target | Verdict |
|---------------|----------|------------------------|--------|---------|
| A*, 128 × 128 rooms (30 % walls, weights 1/3/8, OnlyIfNoObstacles), 32 searches, per expansion | pop ≈ 15 (buffer hit, ≈ 60 % in rooms) or ≈ 85 (heap: ≈ 8 levels × ≈ 10); 8 unrolled neighbours ≈ 100; ≈ 2.2 pushes × ≈ 30 (h, record, heap append, sift-up ≈ 1 level); coordinates, loop ≈ 25 → 190–270 instructions; 1.0–2.5 misses | 1.75–2.9 µs (3.8) | ≤ 2.5 µs | **at risk** (risk 1) |
| one search across a 64 × 64 maze (path ≥ 120 cells) | ≈ 900–1800 expansions (≈ 2000 walkable) at 1.3–2.2 µs (corridors: ≈ 1.3 pushes, rows reused) | 1.2–4.0 ms (5.2) | < 8 ms | PASS |
| the same in slices of 256 expansions, worst slice | 256 × 1.3–2.2 µs | 0.33–0.56 ms (0.73) | < 0.8 ms | PASS, tight |
| 32 re-plans of 10–30 cells on a 64 × 64 level | ≈ 1.2–2.5 expansions per path cell (exact octile, ties toward the goal) → ≈ 770–1600 expansions + 32 begins and path copies (≈ 2 µs each) | 1.4–4.8 ms (6.2) | < 3 ms | **at risk** (risk 1) |
| distance field 64 × 64, one goal, weights, 8 neighbours, one call | ≈ 2870 settled × (150–210 instructions: pop ≈ 60, 8 relaxations ≈ 64, ≈ 2 pushes × 20, ≈ 0.3 stale pops) + 1–2 misses + the begin fill (17 KB) | 4.2–6.6 ms (8.6) | < 8 ms | **at risk** at the upper end |
| the same, every slice of 512 settled cells | 512 × 1.5–2.3 µs | 0.75–1.2 ms (1.5) | < 1 ms | **at risk** |
| `next` for 256 agents at seeded walkable cells of the whole 64 × 64 field | ≈ 85–110 instructions + ≈ 7 lines per agent, the field + map (21.7 KB) > L1: ≈ 2.5–5 misses each | 0.42–0.73 ms (0.95) | < 0.3 ms | **likely FAIL** (risk 1 (b)); a 16 × 16 cluster of agents (a crowd) is logged too: ≈ 0.12–0.2 ms |
| smoothing a 200-cell path (128 × 128 rooms) | ≈ 2 700–5 400 walk steps × ≈ 14 instructions + ≈ 20 tests; the path's rows in L1 | 0.27–0.6 ms (0.78) | < 1 ms | PASS |
| sync 64 × 64 from `collision_grid(mask)` + weight callback reading the `path_cost` table | per cell: the tilemap's filtered-grid callback ≈ 40–80 instructions, the weight callback ≈ 25, the loop ≈ 15, two calls → 170–280 cycles | 2.6–4.3 ms (5.6) | < 5 ms | **at risk** at the upper end |
| a 3 × 3 door refresh (agent 1) | 9 cells as above + the call | 0.01–0.015 ms (0.02) | < 0.05 ms | PASS |

As AC-7 says, the targets stay as written; T7.6 checks the disassembly against this table before the console, the
console measures (T10.2), a missed target is optimised first (risk 1's list, no result may change), then the budget is
set from the measurement with ≈ 25 % margin and written in the spec and the docs with the numbers.

### Self-check and bench (AC-3, AC-5, AC-7)

- **Maps** (`examples/pathfinding/check_maps.{hpp,cpp}`, pure, FNV-1a of (seed, x, y, salt), no `rand`): `rooms(w, h,
  seed)` (rooms and corridors carved from solid, every walkable cell connected, ≈ 30 % walls, weights 1/3/8), `maze(w,
  h, seed)` (a recursive-backtracker maze on odd cells, weights 1), `swamp(w, h, seed)` (open, weights 1..9, a walled
  island). Shared by the self-check, the bench and the tests.
- **Self-check** (`examples/pathfinding/self_check.{hpp,cpp}`, pure, compiled into the example and `nnh_tests`):
  four maps — M1 `rooms(64, 64, 1)`, M2 `maze(48, 48, 2)`, M3 `swamp(32, 24, 3)`, M4 = M1's cells at agent size 2 —
  and a fixed table driving **512 searches** (pairs from the hash: walkable, blocked, equal, outside-the-island; every
  `Diagonals` mode by `set_diagonals`; limits 0/50/200; partial on/off; scales 16/24/64; one-shot or slices of
  1/7/64/256), **door edits** every 32 searches (a 3 × 3 `fill`, a `set_cell`, a `sync(grid, region)` from a callback
  grid), one sliced search with an edit in its middle every 64 (stale), **64 smoothings** of the found paths, and **16
  fields** with 1–8 goals (some with a `max_cost`, half built in slices of 37) with `next` asked for 64 seeded cells
  each. The FNV-1a checksum takes every result's status, stale flag, cost, cells and expansions, every path cell,
  every waypoint, every field distance and every `next` answer. Checkpoints every 64 searches; goldens from pc-debug in
  `kPathfindingCheckGolden`/`kPathfindingCheckpoints`, equal in pc-release, GCC 13/14, clang 18 and the console. The
  example runs 8 searches (and their smoothing/field share) per frame, ≈ 80 frames, logging `pathfinding: <n> sum
  <checksum>` every 64 searches and `pathfinding: PASS <checksum>` or `pathfinding: FAIL search <n> expected <x> got
  <y>`.
- **Bench** (`examples/pathfinding/bench.{hpp,cpp}` pure workloads, `bench_scene.{hpp,cpp}` timing them with the
  profiler, 300 frames per workload, average and worst, only the measured calls inside the scope): `astar` (`rooms(128,
  128, 7)`, 32 seeded pairs in one frame; time / expansions), `maze` (`maze(64, 64, 8)`, its two farthest corners: one
  call, then the same in slices of 256, one per frame), `replans` (`rooms(64, 64, 9)`, 32 seeded pairs whose reference
  paths have 10–30 cells, one frame), `field` (`rooms(64, 64, 9)`, one goal: one call, then slices of 512), `next`
  (256 seeded walkable cells of the whole map; and `next_crowd`, a 16 × 16 cluster, not judged), `smooth` (the first
  200 cells of a seeded long path on the 128 × 128 map), `sync` (a 64 × 64 `Tilemap` built in code with a collision
  layer, a layer mask and a ground layer with `path_cost` tile properties; the documented table pattern) and `door` (a
  3 × 3 `sync(grid, region)`). Lines: `pathfinding bench: <name> avg X <unit> worst Y (target < Z) PASS|FAIL`, then
  `pathfinding bench: PASS` or `pathfinding bench: FAIL (<names>)`; a Debug build logs `not judged (Debug)`.

### The example `examples/pathfinding` (AC-4)

Original art (`art/make_art.py`: 16 × 16 tiles for floor, wall, water, swamp, road, open and closed doors, the hero
(1 × 1 and 2 × 2), the chasers, a goal marker) and level (`art/make_level.py` writes `assets/levels/keep.tmj`: 48 × 30
cells, a `ground` layer whose tiles carry `path_cost` (water 8, swamp 3, road 1), a `collision` layer with walls and
three doors (`door` tile property), an island in the lake, a one-cell corridor, objects for the hero's and the crowd's
spawns). Scenes and pieces:

- `level.{hpp,cpp}`: loads the map, builds the per-tile-id cost table from `path_cost` (the documented pattern), two
  `PathMap`s from `collision_grid()` (agent 1 and agent 2), door toggles (`set_tile` + `sync(grid, door rect)` on both).
- `hero.{hpp,cpp}`: a `PathSearcher` (48 × 30), the path, `smooth_path`, `cells_to_points`, a `PathFollower` (radius 3
  px), `collision::move` of a 12 × 12 box (26 × 26 at 2 × 2) at 1.5 px per frame; re-plans when the map's version
  changes (from its current cell to its goal); logs.
- `crowd.{hpp,cpp}`: 32 chasers (10 × 10 boxes), one double-buffered `DistanceField` rebuilt with `begin` when the hero
  enters another cell and stepped 512 cells per frame; each chaser walks toward `next_point()` with `collision::move`.
- `minimap.{hpp,cpp}`: the bottom screen shows the whole level at 6 px per cell (288 × 180 at (16, 30)); a touch at
  (tx, ty) is the cell `((tx − 16) / 6, (ty − 30) / 6)` (integers, shown in the code), the hero's goal.
- `debug_view.{hpp,cpp}`: the copyable debug-draw helper (FR-20): expanded cells shaded, the open frontier outlined,
  the raw path as dots and the waypoints as lines, the field as a heat map (top screen over the level, bottom over the
  minimap).
- `hud.{hpp,cpp}`: status, cost, cells, expansions and time of the last search; the field's `settled()` / walkable
  cells; mode, smoothing, hero size.
- `showcase_scene`, `check_scene`, `bench_scene`, `main.cpp`, `demos.hpp` (`PATHFINDING_DEMO` = `showcase` | `check` |
  `bench`; `PATHFINDING_SHOT` = `explored` | `smoothed` | `field` | `partial` | `door-closed` | `door-open` | `big`, each a
  fixed goal and settings at frame 5, the screenshot at a fixed frame). `AppConfig::gfx.max_draws = 4096` (the heat
  maps).

Controls (Old 3DS buttons): touch = goal; A = toggle the next door; X = hero 1 × 1 / 2 × 2; L = cycle `Diagonals`
(`set_diagonals` on both maps); R = smoothing on/off; D-Pad Up = debug view (off, search, field); SELECT = self-check;
Y = bench; START = quit. Log lines (the script's): `goal: <x>,<y> <status> cells <n> cost <c> expanded <e> waypoints
<w>` (status lower-case: `found`, `partial`, `no-path`, `limit`), `door: <n> open|closed version <v>`, `replan:
version <v> <status> cells <n> cost <c> expanded <e> waypoints <w>`, `mode: <diagonals>`, `hero: 1x1|2x2`,
`smoothing: on|off`, `field: built <x>,<y> settled <n> frames <f>`.

### The platformer (AC-6)

Untouched: no file under `examples/platformer/` changes. The stage-end guard runs ctest
`platformer_replay_matches_script` with the lines **as they are in the tree** (spec 021's `B2418A28`, or spec 022's
regenerated lines after that merge) and `git diff --exit-code examples/platformer`.

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/pathfinding.hpp` | new | the public API above (≈ 280 lines with comments, as `entities.hpp`/`tilemap.hpp`) |
| `engine/core/src/pathfinding/path_block.hpp` | new | `PathMapBlock` (header, padded cells, source), index helpers, neighbour offsets, the users count and doomed flag, the `path` tag |
| `engine/core/src/pathfinding/path_check.{hpp,cpp}` | new | misuse reporting (assert + error log channel `path`), shared checks (cell, region, buffer, point) |
| `engine/core/src/pathfinding/map_create.cpp` | new | desc validation, the 32-bit bound, allocation, move/destroy, the doomed block, `path_map_desc` |
| `engine/core/src/pathfinding/map_sync.cpp` | new | sync from a `Grid` (whole, region), weights, one-way setting |
| `engine/core/src/pathfinding/map_edit.cpp` | new | `set_cell`, `fill`, `set_diagonals`, queries, version |
| `engine/core/src/pathfinding/footprint.{hpp,cpp}` | new | erosion of a rectangle for agent sizes 2–4 |
| `engine/core/src/pathfinding/map_geometry.cpp` | new | `contains`, `cell_center`, `cell_at`, `cells_to_points` (floats) |
| `engine/core/src/pathfinding/neighbors.hpp` | new | directions, deltas, the diagonal rule (inline) |
| `engine/core/src/pathfinding/heuristic.hpp` | new | octile/Manhattan, the scale (inline) |
| `engine/core/src/pathfinding/open_heap.{hpp,cpp}` | new | 16-byte entries, inline push/pop with a hole, the best-child buffer; compaction and heapify (cold) |
| `engine/core/src/pathfinding/search_internal.hpp` | new | `PathSearchBlock` (records, heap, the running search's state) |
| `engine/core/src/pathfinding/searcher_create.cpp` | new | create, move/destroy, stamp wrap |
| `engine/core/src/pathfinding/search_begin.cpp` | new | `begin`/`find`/`cancel`/`result`, start and goal rules, finish (status, stale, length), map users |
| `engine/core/src/pathfinding/search_step.cpp` | new | `step(n)`: the expansion loop, one template per diagonal mode, partial tracking |
| `engine/core/src/pathfinding/search_path.cpp` | new | `path`, `path_points`, `reached`/`expanded`/`cost_to` |
| `engine/core/src/pathfinding/smooth_walk.hpp` | new | the integer centre-to-centre walk (inline class) |
| `engine/core/src/pathfinding/smooth.cpp` | new | `smooth_path` |
| `engine/core/src/pathfinding/field_internal.hpp`, `field_create.cpp`, `field_build.cpp`, `field_query.cpp` | new | fields: block, create/move, Dijkstra slices, `distance`/`next`/`next_point` |
| `engine/core/src/pathfinding/field_heap.hpp` | new (T10.3) | the field's 4-ary heap of 64-bit keys (inline) |
| `engine/core/src/pathfinding/follower.cpp` | new | `PathFollower` |
| `engine/core/CMakeLists.txt` | changed | one `_nnh_pathfinding_sources` block (with `-ffp-contract=off`), merge notes' anchor |
| `tests/pathfinding/path_reference.{hpp,cpp}` | new | ASCII maps, seeded random maps, reference Dijkstra (std containers), the literal reference A*, path validator, sampled segment checker, brute-force erosion |
| `tests/pathfinding/test_reference_self.cpp` | new | the references on hand-computed tiny cases |
| `tests/pathfinding/test_path_map_create.cpp`, `test_path_map_sync.cpp`, `test_path_map_edit.cpp`, `test_footprint.cpp`, `test_path_points.cpp` | new | FR-1..FR-5, FR-8's helpers |
| `tests/pathfinding/test_neighbors.cpp`, `test_open_heap.cpp`, `test_find_basic.cpp`, `test_find_costs.cpp`, `test_tie_order.cpp` | new | FR-6..FR-11 |
| `tests/pathfinding/test_limits_partial.cpp`, `test_slicing.cpp`, `test_searcher_reuse.cpp`, `test_inspect.cpp`, `test_path_misuse.cpp`, `test_path_cost.cpp` | new | FR-12..FR-14, FR-20, FR-22, FR-23 |
| `tests/pathfinding/test_smooth_walk.cpp`, `test_smooth.cpp`, `test_follower.cpp` | new | FR-15, FR-19 |
| `tests/pathfinding/test_field_build.cpp`, `test_field_slices.cpp`, `test_field_next.cpp` | new | FR-16..FR-18 |
| `tests/pathfinding/test_reference_random.cpp` | new | AC-2 |
| `tests/pathfinding_example/test_self_check.cpp` | new | AC-3 golden |
| `tests/fuzz/fuzz_path_ops.cpp`, `libfuzzer_path_ops.cpp`, `test_fuzz_path_ops.cpp` | new | random operation streams (X, FR-23) |
| `tests/CMakeLists.txt` | changed | one 025 block + two ctests (merge notes' anchors) |
| `examples/pathfinding/` | new | `main.cpp`, `demos.hpp`, `level.*`, `hero.*`, `crowd.*`, `minimap.*`, `debug_view.*`, `hud.*`, `showcase_scene.*`, `check_maps.*`, `self_check.*`, `check_scene.*`, `bench.*`, `bench_scene.*`, `art/make_art.py`, `art/make_level.py`, `assets/…`, `CMakeLists.txt`, `tests/check.nnhs` |
| `examples/CMakeLists.txt` | changed | `add_subdirectory(pathfinding)` |
| docs (stage 11) | new / changed | `docs/api/pathfinding.md`, `docs/images/pathfinding/*.png` (new); `docs/api/collision.md`, `docs/api/tilemap.md`, `docs/guides/levels.md` (slow tiles), `docs/architecture/decisions.md` (D40 implemented), `patterns.md` (row implemented), `docs/REFERENCES.md`, `CLAUDE.md`, `docs/ROADMAP.md` |

### Platform work

- **pc / 3ds:** none (portable core). The 3DS runs the check, the showcase and the bench (stage 10).
- **fake:** nothing new; every test runs on it as is.

## Test strategy

All unit tests run in `nnh_tests` on the fake platform (Windows; VM 120 with GCC 13/14 and clang 18, ASan/UBSan).
Maps are drawn in ASCII in the tests (`#` blocked, `.` 1, digits weights, `S`/`G` start/goal); the references live in
`tests/pathfinding/path_reference.cpp` and may use standard containers (tests only).

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `test_path_map_create.cpp`: sizes 1 and 1024 accepted, 0/1025 refused (`InvalidArgument`), costs and agent sizes out of range, the 32-bit bound at its edge (largest accepted, smallest refused, computed in 64 bits), the default tag and a game tag counted, move leaves the source invalid, bytes per cell from `mem::tag_stats` |
| FR-2 | `test_path_map_create.cpp` / `test_path_map_edit.cpp`: a new map is all weight 1; outside is blocked |
| FR-3 | `test_path_map_sync.cpp`: array grid, callback grid, a tilemap's `collision_grid(mask)` with two masks (boats/walkers), region offset (cells beyond the grid read `outside`), Empty/Solid/OneWay and `one_way_walkable`, the weight callback called once per walkable cell with grid coordinates (counted, row-major) and never by searches, bad callback values (assert; Release clamp), partial sync touching only its rectangle |
| FR-4 | `test_path_map_edit.cpp`: `set_cell`/`fill` (clipped), `blocked`/`weight`, version +1 per call (sync, set, fill, set_diagonals) |
| FR-5 | `test_footprint.cpp`: sizes 1–4 against brute-force erosion on seeded maps; max weight of the square; last k − 1 columns/rows blocked; partial sync and `set_cell` near a changed cell recompute exactly the overlapping cells |
| FR-6 | `test_neighbors.cpp`: the order and offsets; a diagonal gap between two walls passes only with Always; one wall passes with AtLeastOneWalkable and Always; OnlyIfNoObstacles never cuts; Never has 4; `test_find_costs.cpp`: step × weight of entered cells, the start's weight never counts |
| FR-7 | `test_searcher_reuse.cpp`: 10 000 searches on one searcher equal fresh searchers' results; maps smaller than the searcher; the stamp wrap (test hook `detail::path_searcher_set_stamp`) gives the same results across it; two searchers on one map interleaved; record and entry sizes `static_assert`ed (8, 16) |
| FR-8 | `test_find_basic.cpp`: statuses, cost, cells, expanded; `path()` exact fit, too small (first cells), null + 0 counts, readable until the next search; `test_path_points.cpp`: `path_points`, `cells_to_points`, `cell_center` (agent 1 and 2), `cell_at` equal to `collision::cell_at` on the same grid for 10 000 seeded points (and its anchor shift) |
| FR-9 | `test_find_costs.cpp`: Found cost = the reference Dijkstra's on 500 seeded maps; scales 24 and 64: cost ≤ scale/16 × minimum, expansions never more than at 16 on the test set's sum |
| FR-10 | `test_tie_order.cpp`: hand-drawn tie cases with their expected cells (an open room straight and diagonal, a symmetric pillar, equal weights both ways); `test_open_heap.cpp`: pop order = a sorted reference for random keys with ties, through compaction (a test hook shrinking the capacity) and the buffer; AC-2's literal reference |
| FR-11 | `test_find_basic.cpp`: from == to (walkable and blocked), outside (Invalid, assert), blocked start leaves, blocked goal → NoPath with 0 expanded, with partial → Partial |
| FR-12 | `test_limits_partial.cpp`: LimitReached at exactly the limit (the goal as the limit-th pop is Found), every status; the heap never overflows (a comb map that keeps ≈ 4/5 of cells open) |
| FR-13 | `test_limits_partial.cpp`: closest by h, ties by g then index (crafted), unreachable island → its shore, blocked goal, limit → Partial; without the option NoPath/LimitReached with 0 cells |
| FR-14 | `test_slicing.cpp`: slices of 1, 7, 64 and the whole equal `find` (status, cost, cells, expanded, path) on 300 seeded searches; cancel; begin while running restarts; stale when the version changes mid-search (and the search keeps going over current cells); the map moved during a search (fine), destroyed (assert; Release: Invalid, block freed: allocation counter back to zero) |
| FR-15 | `test_smooth_walk.cpp`: the walk equals `cells_crossed` on a 16-px grid for 20 000 seeded centre pairs (cells, order, corner flags); `test_smooth.cpp`: open room → 2, L corridor → 3, a corner it must not cut in each mode, a heavier cell it must not cross, first and last kept, in place, buffer contract, blocked start |
| FR-16 | `test_field_build.cpp`: one and several goals, max cost, weights, every diagonal mode, blocked goals ignored, blocked cells unreachable, against the multi-source reference |
| FR-17 | `test_field_slices.cpp`: slices of 1, 37, 512 equal one call; queries answer the previous build while the next runs (double), "not ready" (single); stale; destroyed map |
| FR-18 | `test_field_next.cpp`: `next` = brute-force argmin with neighbour-order ties, none at goals and unreachable cells, the diagonal rule; `next_point`; field = search cost from every walkable cell (AC-2's equality on 200 maps here too) |
| FR-19 | `test_follower.cpp`: radius advance, several points in one call, overshoot of the last point (walks back, arrives), empty list, NaN position (assert; Release unchanged), squared distances only |
| FR-20 | `test_inspect.cpp`: `reached`/`expanded`/`cost_to` during a sliced search and after; field distances during a double-buffered rebuild |
| FR-21 | `test_path_cost.cpp`: `events::stats()` unchanged over every operation |
| FR-22 | `test_path_cost.cpp`: allocation counter unchanged after creation over 10 000 operations; no log line in the normal path; the weight callback is the only callback (a counting callback) |
| FR-23 | `test_path_misuse.cpp`: each row of the misuse table asserts (`run_expecting_assert`) and, on the Release path (pc-release run), behaves as listed |
| FR-24 | include check (the header pulls only its four includes; no tilemap, entities, gfx, data header), T8.3's probes, `collision.hpp`/`tilemap.hpp` byte-identical (`git diff --exit-code`) |
| AC-1 | all of the above on Windows and VM 120 (GCC 13/14, clang 18, ASan/UBSan) |
| AC-2 | `test_reference_random.cpp`: 2048 seeded maps (1 × 1 to 64 × 64, 0–50 % blocked, weights 1–9, every mode, agent 1–2): 4 searches each against the reference Dijkstra (reachability, minimum cost, valid path, summed cost) and the literal reference A* (same status, cost, expansions, cells; partial best); one field per map (1–4 goals) against the multi-source reference; every smoothed segment against the sampled checker. Debug budget ≤ 5 s: if over, 512 maps in Debug and 2048 in pc-dev/pc-release (recorded at T6.1) |
| AC-3 | `tests/pathfinding_example/test_self_check.cpp`, ctest `pathfinding_self_check_pc`, the console's `pathfinding: PASS` |
| AC-4 | PC screenshots read back (T7.2) |
| AC-5 | `examples/pathfinding/tests/check.nnhs` with `--pc` (ctest `pathfinding_check_script_pc`), then on the console |
| AC-6 | the platformer guard (above) at every stage end; T10.2 run 2 |
| AC-7 | the bench lines on the console; T7.6's table |
| AC-8 | stage 0 references compared at stage ends; `hello` sizes and `nm`; probes; build times |
| AC-9 | stage 11 |

## Risks and alternatives

1. **AC-7: A* per expansion, the re-plans, the field (one call and slices), `next` for 256 scattered agents and the
   sync are at risk.** Mitigations in order, each changing no result (every golden holds): (a) a per-cell byte of
   allowed steps in the map (the diagonal rule precomputed at sync: no blocked or side checks per neighbour; +1 B per
   cell); (b) `pld` of the heap top's record rows during an expansion, and for `next` a batch form
   `next(map, cells, out, count)` that prefetches two agents ahead (an API addition); (c) a 4-ary heap (the same total
   order); (d) fields: the map's weight packed into the distance word when the map's worst cost is below 2²⁴ (one array
   per neighbour); (e) sync: reading `cells` directly when the grid has them. Then AC-7's measurement-with-margin policy.
2. **Memory above the spec's estimate** (searchers 24 B per cell, fields 16): see "Data and memory". Alternatives if the
   maintainer prefers: 12-byte entries with the sequence renumbered at compaction, a heap sized to the 4/5 bound, or a
   node pool for huge maps (spec decision (b)). None changes a result.
3. **The total order is forever** (FR-10): the buffer, the lazy heap and compaction are proven equal to the literal
   reference on AC-2's maps and by `test_open_heap.cpp`; the mutation pass targets them.
4. **Smoothed segments are checked for the centre line** (FR-15 as written): a full-cell box on a segment that runs
   close to a cell edge overlaps the cell beside it. The docs say so; the example's hero is 12 px in 16-px cells and
   slides with `collision::move`; a big agent uses an agent-size map. A thick-segment check is a follow-up.
5. **The collision-internal header** (`first_cell`, inline `cell_at`): a pinned test compares `PathMap::cell_at` with
   `collision::cell_at` and sync's reads with `cells_crossed`'s kinds, so a change there is caught.
6. **"Furthest" is greedy**: a path around a pillar that comes back into view keeps one waypoint more than a true
   furthest search would; documented (cubic otherwise).
7. **The doomed block** (a map destroyed during sliced work): tests under ASan; the allocation counter proves the free.
8. **AC-2's run time** in Debug (thousands of maps with std containers): budgeted, measured at T6.1.
9. **Merging with specs 022 and 023** (parallel work): see "Merge notes".

## Merge notes (separate worktree; merged after specs 022 and 023)

Spec 025 is built in its own git worktree (branch `spec-025`, from `main` at this plan's commit) and merged after
022 and 023. Every edit to a shared file is an insertion at its own anchor; nothing is reordered or reformatted.

| Shared file | 022 / 023 | 025's change, at a distinct anchor |
|-------------|-----------|------------------------------------|
| `engine/core/CMakeLists.txt` | 022: transform/hierarchy blocks after the prefabs block, entities lines; 023: `input_block.cpp` in the main list, the transition block after the camera block | the `_nnh_pathfinding_sources` block (and its `-ffp-contract=off`) inserted **right after the `_nnh_query_sources` block's `endif()`**, before the `# Object pools (spec 020, D35)` comment |
| `tests/CMakeLists.txt` | 022: alphabetical lines, `hierarchy_example` after `prefabs_example`, ctests near prefabs; 023: one block after the `camera_example` line, ctests after `camera_self_check_pc` | **one** block (every 025 test, `target_sources(nnh_fuzz_targets PRIVATE fuzz/fuzz_path_ops.cpp)`, and an `if(NNH_BUILD_FUZZERS)` with `fuzz_path_ops`) placed **right before `add_test(NAME unit COMMAND nnh_tests)`**; the two ctests right after the `data_self_check_pc` block's `endif()`; nothing in the alphabetical list, the fuzz library line or the libFuzzer `foreach` |
| `examples/CMakeLists.txt` | 022: `hierarchy`; 023: `transitions` after `text` | `add_subdirectory(pathfinding)` between `input` and `platformer` |
| `docs/ROADMAP.md` | their own items | only the existing "Pathfinding on grids" line |
| `CLAUDE.md` | 022/023: layout paragraphs after "Prefabs"/"Camera helpers", commands after `prefabs`/`queries` | the layout paragraph after the "Collision layers and queries" paragraph; the commands after the `ANIMATION_DEMO=check …` line |
| `docs/REFERENCES.md`, `docs/architecture/decisions.md` change log, `specs/README.md` | their own blocks and lines | 025's block appended; one change-log entry appended; the index row's status — on conflict keep both sides |
| `docs/architecture/patterns.md` | 022/023 rows | 025's row "Time-sliced job" (already on `main` with this plan) |
| `docs/api/collision.md`, `docs/api/tilemap.md`, `docs/guides/levels.md` | not touched by 022/023 (checked at merge) | a "Pathfinding" link at the end of "Grids"; one under "Collision layers"; a "Slow tiles for pathfinding" subsection at the end of "4. Collision" |
| `tests/framework/test_main.cpp` (`kMaxTests` 4096) | maybe raised | not changed unless T0.3 finds fewer than 400 left; then the larger value wins at the merge |
| `examples/platformer/**` | 022: barrels, regenerated `level.nnhs`; 023: doors | **never touched** |

After the merge the lead runs: the full ctest (pc-debug, pc-release), every example's `--pc` script, AC-3's golden,
022's and 023's goldens and the replay ctest with the tree's lines, and one 3ds-release build. No golden of 022 or 023
may move because of 025 (025 changes no existing source file), and 025's golden doesn't depend on theirs.

## Stage 10: console run (prepared)

T10.1 on the PC first: `scripts/dkp.sh cmake --build --preset 3ds-release` and `scripts/dkp.sh cmake --build --preset
3ds-dev --target pathfinding_3dsx platformer_3dsx`, without a warning; T7.6's table still describes the objects;
3ds-release `hello.elf` sizes equal T0.1's. Then on an Old 3DS, nobody touching it after Y. Two runs, each: open the
Homebrew Launcher, press **Y**, then on the PC (`nnh` builds the Development `.3dsx`, sends it, plays the script and
brings the console back to the Homebrew Launcher; logs and screenshots in `build/logs/`):

1. `build/pc-debug/tools/nnh/nnh run pathfinding --script examples/pathfinding/tests/check.nnhs` (≈ 2 min). Expected,
   as on the PC (pc-debug), with the values recorded in "Stage 6 results" and "Stage 7 results":
   `pathfinding: check started`, `pathfinding: 64 sum <c1>` … `pathfinding: 512 sum <c8>`, **`pathfinding: PASS
   <golden>`**; `pathfinding: showcase`; after each scripted touch the PC's `goal: <x>,<y> found cells <n> cost <c>
   expanded <e> waypoints <w>` line (the spec's example form `goal: 23,9 found cells 31 cost 362 expanded 148
   waypoints 4`, with the values the level gives); A → `door: 1 closed version <v>` and `replan: version <v> found cells
   … cost <c'>` with c' ≠ c; the island touch → `goal: <x>,<y> partial …`; X → `hero: 2x2`, the corridor touch → `goal:
   <x>,<y> no-path …` (or `partial` per the script); L → `mode: always` (and back); `field: built … settled <n> frames
   <f>` lines as on the PC; then `pathfinding bench: started` and the lines `astar … (target < 2.5 us)`, `maze one … (<
   8 ms)`, `maze slices … (< 0.8 ms)`, `replans … (< 3 ms)`, `field one … (< 8 ms)`, `field slices … (< 1 ms)`, `next …
   (< 0.3 ms)`, `next_crowd …` (not judged), `smooth … (< 1 ms)`, `sync … (< 5 ms)`, `door … (< 0.05 ms)`, and the
   verdict `pathfinding bench: PASS` or `pathfinding bench: FAIL (<names>)`; `nnh: PASS (<n> lines, …)`. **Expected from
   the ARM pre-check** (T7.6 refines it): every logic line equal to the PC's; `maze` and `smooth` and `door` PASS;
   `astar` 1.75–2.9 µs, `replans` 1.4–4.8 ms, `field` 4.2–6.6 ms (slices 0.75–1.2), `sync` 2.6–4.3 ms — at risk; `next`
   0.42–0.73 ms — likely FAIL. A FAIL fails the run (`fail-on "pathfinding bench: FAIL"`) after every number is
   logged: record them in "Console run 1"; T10.3 applies risk 1's list, or the budgets are set from the measurement
   with margin. Read the screenshots (explored, smoothed, field, partial, doors, big hero): the same pictures as the
   `--pc` run's.
2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` (≈ 40 s). Expected:
   exactly the `replay: … sum` lines **in the tree at that time**, `replay: done 900 frames sum <last>: 60 Hz PASS`;
   `nnh: PASS (79 lines, …)`. A different sum is a defect (025 touches nothing the platformer runs).

**Completed with the recorded values (stage 7).** Before the console, on the PC: `scripts/dkp.sh cmake --build --preset
3ds-release` and `scripts/dkp.sh cmake --build --preset 3ds-dev --target pathfinding_3dsx platformer_3dsx` without a
warning; `sh build/ref-025/sizes.sh now` equal to `build/ref-025/base/sizes.txt` (hello unchanged). Then, Homebrew
Launcher open on the Old 3DS, press **Y**, and on the PC:

```sh
build/pc-debug/tools/nnh/nnh run pathfinding --script examples/pathfinding/tests/check.nnhs   # ~75 s: 3 700 frames
```

Expected, every line as the `--pc` run's (`nnh: PASS (104 lines, …)` on the PC): `field: built 3,3 settled 1135 frames
3`; `pathfinding: check started`; `pathfinding: 64 sum 8332890F`, `128 sum BF55A00B`, `192 sum B3E6C49A`, `256 sum
C825A5E7`, `320 sum 77C6FFC5`, `384 sum E738F010`, `448 sum 528F7687`, `512 sum 1C0B5992`; **`pathfinding: PASS
1C0B5992`**; `pathfinding: showcase`; `hero: paused`; `goal: 24,5 found cells 22 cost 438 expanded 251 waypoints 3`;
`door: 1 closed version 2`; `replan: version 2 found cells 48 cost 1040 expanded 777 waypoints 9`; `goal: 34,7 partial
cells 41 cost 1012 expanded 1134 waypoints 7`; `hero: 2x2`; `goal: 24,20 partial cells 18 cost 428 expanded 378
waypoints 3`; `mode: never`; `hero: 1x1`; `hero: walking`; `pathfinding bench: maze path 405 cells, 494 expanded; smooth
path 200 cells; replans 32 pairs, 4518 expanded; astar 81359 expanded` (the counts must match the PC's); then the eleven
bench lines and the verdict. **Expected verdict from the pre-check: `pathfinding bench: FAIL (replans, next, …)`**
(`field slices`, `astar`, `field one` and `sync` at risk): the script then fails at its verdict line, after every number
is logged — record them under "Console run 1" and go on with T10.3 (find the cost in the 3ds-dev disassembly, apply risk
1's list without changing a result, run again; then set the budgets from the measurement with ≈ 25 % margin in the spec's
AC-7, `bench_scene.cpp` and `docs/api/pathfinding.md`). Screenshots (`build/logs/pathfinding-*/`): start, door-open,
door-closed, partial, big, walking, bench — the same pictures as the `--pc` run's. Run 2 (the platformer) as above.

## Decisions taken (overnight, to review)

The spec's "Decisions to take" are approved as recommended. Its open questions and these plan-level decisions were
taken overnight, each the recommended or simplest option; the maintainer reviews them in the morning:

1. **Names**: module `pathfinding`, header `<nnh/pathfinding.hpp>`; `PathMap` (+ `PathMapDesc`, `path_map_desc`,
   `PathWeights`), `PathSearcher`, `DistanceField` (+ `DistanceFieldDesc`), `PathFollower`, `CellPos`, `CellRect`,
   `Diagonals`, `PathStatus` (`None`, `Running`, `Found`, `Partial`, `NoPath`, `LimitReached`, `Invalid`),
   `PathOptions`, `PathResult`, `kPathBlocked`, `kUnreachable`, `smooth_path`, `cells_to_points`; log channel `path`;
   example `examples/pathfinding` (`PATHFINDING_DEMO`, `PATHFINDING_SHOT`).
2. **The header also includes `<nnh/result.hpp>`** (FR-1's `Result`, as `pool.hpp` and `tilemap.hpp`) besides FR-24's
   three; `MemTag` is forward-declared.
3. **The neighbour mode is Godot's single enum** `Diagonals{Never, Always, AtLeastOneWalkable, OnlyIfNoObstacles}`
   (Never = 4 neighbours), a map property changeable with `set_diagonals` (which increments the version).
4. **Map layout**: a byte per cell (0 blocked, 1..255 weight) with a blocked one-cell border; agent sizes 2–4 keep the
   source bytes (+1 B per cell) and the eroded cells; erosion by k² reads per affected cell.
5. **The 32-bit bound** is `width × height × diagonal × 259 < 2³²` (FR-6's 255, plus the largest scaled heuristic);
   the heuristic scale is limited to **16..64** (up to 4× the minimum cost).
6. **Open question 1**: maps, searchers and fields up to 1024 × 1024 are allowed; the docs give their memory and
   recommend a region map for big levels.
7. **Open question 2**: 10/14 stays the default (the bound above holds at 1024 × 1024).
8. **Open question 3** (scheduler): the example's round-robin pattern only; a follow-up if two games copy it.
9. **Searcher memory**: 8-byte records per padded cell + 16-byte open entries for every cell (24 B per cell); fields
   16 B per cell double-buffered (12 single): above the spec's estimate, flagged (risk 2).
10. **Open set**: a binary heap with lazy deletion, compacted when full, the 64-bit `(f, h)` key then `seq`, and a
    one-entry best-child buffer.
11. **Stamps**: 28 bits in the record's meta word; the wrap zeroes every meta once.
12. **Counting**: `expanded` counts cells taken from the open set, the goal's pop included; the limit is checked before
    each pop (default: the map's cell count); `from == to` has 0 expanded; a blocked goal without `partial` has 0.
13. **Partial paths** use the unscaled heuristic; the start counts as reached.
14. **Statuses** add `None` (no search) and `Running` (a sliced search in progress) to the spec's five.
15. **Fields**: blocked cells and blocked goals are unreachable (FR-18's equality holds for walkable cells); goals
    outside the map are misuse.
16. **Fields during a rebuild** (spec decision (a)) double-buffered by default; `double_buffered = false` gives the
    smaller single-buffered field whose queries report "not ready" while building.
17. **Field heap order** `(d, cell index)`; `next` reads neighbours (no stored directions). *Changed in T10.3 (no result
    change):* the build stores each cell's `next` answer in a step byte, read while the map is the build's and unchanged;
    otherwise `next` reads neighbours as before ("Optimisation round (T10.3)").
18. **Open question 5**: smoothing uses its **own integer walk** copying spec 019's rules over the map's bytes (no
    collision-internal walk, no int64 or double), proven equal to `cells_crossed`; T4.1 also measures spec 019's
    `GridWalk` on the PC for the record.
19. **Smoothing rules**: "furthest" = the end of the first visible run; weights checked on the cells crossed through
    their interior; corners by the map's rule; the anchor cell never checked (a blocked start); `out` may alias `path`.
20. **World points**: `cell_center` is the centre of the agent's k × k footprint; `cell_at` is the anchor of an agent
    centred at the point (spec 012's half-open rule through `collision/grid_internal.hpp`'s `first_cell`).
21. **Follower**: radius only (no "passed" rule); an empty list is arrived and returns the position.
22. **Map lifetime**: searchers and fields hold the map's block; moving a map during sliced work is supported;
    destroying it asserts and the block is freed by its last user (Release: `Invalid`).
23. **Weight callback** gets grid coordinates; Release clamps bad values to 1..255.
24. **Weights from Tiled**: the pattern (a per-tile-id table from `path_cost`, then a two-line callback), no
    `sync(tilemap)` convenience (the pattern is under 15 lines).
25. **Open question 4** (flee maps): a follow-up; the docs show the naive "step to the neighbour with the largest
    distance" with its limitation.
26. **Open questions 6 and 7** (JPS, window searches): follow-ups, not in this spec.
27. **Spec option (b)** (chasers in `examples/entities`): not done; the new example shows the module.
28. **Example layout and controls** as described above (Old 3DS buttons only).
29. **Bench**: workloads as above; `next` judged on scattered agents (the honest worst), the crowd cluster logged.
30. **AC-7 targets kept as written**; `astar`, `replans`, `field`, `sync` at risk, `next` likely to miss.
31. **AC-2's size**: 2048 maps, reduced to 512 in Debug only if it takes more than 5 s there.
32. **A fuzz target** `fuzz_path_ops` (mini-fuzz in `nnh_tests`, libFuzzer with `NNH_BUILD_FUZZERS`).
33. **Console stage**: two scripted runs (pathfinding `check.nnhs` with the bench; platformer `level.nnhs`).

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | Integer searches, fields, smoothing and footprints; the total order implemented once and checked against a literal reference; floats only in geometry and the follower, with `-ffp-contract=off` and spec 012's cell rule; AC-3 on every compiler and the console |
| II | ✅ | Memory stated and returned by `create` (above the spec's estimate, flagged); a cache-planned layout, no division, an unrolled neighbour loop, the best-child buffer; misses counted; the disassembly checked before the console; several AC-7 targets flagged at risk with the measurement-with-margin fallback |
| III | ✅ | One light header, measured |
| IV | ✅ | No events; queries and commands |
| V | ✅ | No allocation after creation, no `virtual`, RTTI or exceptions; one game callback, at sync |
| VI | ✅ | Tests first; brute-force and literal references; golden; fuzz; mutation pass |
| VII | ✅ | One module of ≈ 24 small sources |
| VIII | ✅ | API page with pictures, linked pages, example, D40 |
| IX | ✅ | Own code, prior art cited, original art |
| X | ✅ | No parser; bounded searches; misuse loud in Debug, harmless in Release; doomed blocks instead of dangling reads |
| XI | ✅ | Optional, linked only when called; collision and tilemap headers unchanged; `hello` and the platformer unchanged |

No constitution exception is requested. D40 is new (see [decisions.md](../../docs/architecture/decisions.md)); D34 is
not amended (filtered grids are read through the unchanged `Grid` callback; the module includes
`collision/grid_internal.hpp` for spec 012's cell rule, as D36/D37 use other modules' internal headers). The spec's
memory estimates (searchers ≤ 8 B, fields ≤ 4 B per cell) are not met and are flagged (decision 9, risk 2). The
patterns page gains a "Time-sliced job" row.

## Stage 0 results

Windows, WinLibs GCC 16.2, devkitARM; built in the `spec-025` worktree from `main` at `1e4570b`. Three other builds ran
on this PC at the same time (other agents): times below are noisy, sizes are exact.

- **Sizes** (`build/ref-025/base/sizes.txt`, `nm -C` lists beside it): `hello.exe` pc-release 278 856 B, pc-debug
  1 297 082 B; `hello.elf` 3ds-release 2 488 800 B (`.3dsx` 246 512), 3ds-dev 2 748 028 B (`.3dsx` 268 012). `.text` of
  the 3ds-release examples: queries 256 004, entities 250 580, prefabs 313 548, platformer 327 132.
- **Headers** (one-line TUs, best of 5 `-fsyntax-only`, pc GCC): `<nnh/collision.hpp>` 691 lines 301 ms;
  `<nnh/collision.hpp>` + `<nnh/result.hpp>` 7 745 lines 421 ms; `<nnh/tilemap.hpp>` 8 485 lines 435 ms.
- **Disassembly** of `collision::detail::first_cell` and `collision::cell_at` (3ds-dev `grid.o`, `point_queries.o`) in
  `build/ref-025/dis0/`.
- **Screenshots** (T0.2): `build/ref-025/ref/` — hello, queries (`all` at 30), entities (60), prefabs (60), platformer
  (90), replay (64, 312); `build/ref-025/compare.sh now` retakes and compares them.
- **Goldens and counts** (T0.3): ctest `platformer_replay_matches_script` with the tree's `replay: done 900 frames sum
  B2418A28: 60 Hz PASS`; `queries: PASS 4ACAA177`, `entities: PASS C8FB9A41`, `prefabs: PASS 7BB7901E`; 2 414 tests in
  `nnh_tests`, `kMaxTests` 4096 (1 682 left: not raised).
- **Not measured**: the clean `nnh_core` time and the edit-a-game-file times at stage 0 (three parallel builds made them
  meaningless); the edit times of `examples/pathfinding` are measured against `examples/queries` in the same session at
  T8.4.
- Guards: `build/ref-025/guards.sh` (builds without warnings, both ctests, 3DS presets, sizes and `nm` against the base,
  untouched headers and platformer, docs links).

## Stage 1 refinements and results

- **Header** as sketched, plus `PathSearcher::valid()`, `DistanceField::width()/height()/valid()` and
  `friend struct detail::PathAccess` in the three classes (the internal accessor the searcher, fields and tests use;
  no public API).
- **Regions** (`fill`, `sync(grid, region)`): clipped to the map; a negative size or a non-empty region entirely
  outside the map is misuse (assert, nothing changed); an empty region is accepted and still increments the version
  (every accepted call does, as written).
- **Queries on an invalid (empty) map** (`blocked`, `weight`, accessors) answer blocked / 0 without asserting; commands
  on it (`sync`, `set_cell`, `fill`, `set_diagonals`, `cell_center`, `cell_at`) assert. `blocked`/`weight` outside a valid
  map assert as the misuse table says (Release: blocked).
- **Memory**: exactly `sizeof(PathMapBlock)` (96 B on 64-bit) + (W + 2)(H + 2) (+ W·H for agent sizes 2–4), tested
  through the tag's stats.
- **Header cost** (T1.6): `<nnh/pathfinding.hpp>` 8 023 lines, 437 ms against 421 ms for `collision.hpp` + `result.hpp`
  (+16 ms, within 0.05 s); it pulls only `collision.hpp`, `math.hpp`, `result.hpp` (and `assert.hpp`, `build.hpp`,
  `detail/printf_format.hpp` through it) and `<cstdint>`.
- Tests: `test_path_map_create.cpp` (10), `test_path_map_sync.cpp` (9), `test_path_map_edit.cpp` (5),
  `test_footprint.cpp` (3), `test_path_points.cpp` (5). Guards: all green (ctest 268 pc-debug, 258 pc-release), sizes and
  `nm` identical to stage 0.

## Stage 2 refinements and results

- **Open set** as planned (`open_heap.{hpp,cpp}`): 16-byte entries `{h, f, seq, cell}`, `(f, h)` compared as one 64-bit
  key then `seq` descending, hole sifts, the one-entry buffer (`offer`/`take`; a held entry that loses replaces the
  heap's top with one sift-down, so taking never grows the heap), compaction by a predicate then Floyd's heapify.
  The searcher compacts before an `offer` that finds the heap full (an `offer` adds at most one heap entry).
- **Cells in entries** are packed `(y << 16) | x`; neighbours move them by constant packed deltas (`kPackedDelta`),
  and the record index is `(y + 1) · stride + x + 1` (one multiply). Partial ties on the row-major index compare the
  packed cells (the same order).
- **Records** `{g, meta}`, meta = stamp (28 bits) | closed | dir; "reached this search" is `meta >> 4 == stamp`; the
  start's record is stamped at `begin` (so `reached(from)` is true from the start, also for `from == to`).
- **The expansion** (`search_step.cpp`): `Expander<M>` per mode, the orthogonal bytes loaded once and reused for the
  diagonal rule; the heuristic scale is skipped when 16. The mode, costs and scale are captured at `begin` (a
  `set_diagonals` during a sliced search applies to the next search).
- **`step()` misuse**: `step()` with no search begun since creation or `cancel()` (status None) asserts; `step()`
  after a finished search returns true silently (a round-robin loop may call it).
- Tests: `path_reference.{hpp,cpp}` (RefMap copy of what searches see, Dijkstra, the literal A*, the multi-source
  field, path validator, an **exact** segment checker — every cell whose open interior the centre line crosses, by
  corner cross products, and every lattice corner on the segment — instead of 1/64 sampling, which can miss thin
  slivers), `test_reference_self.cpp` (6), `test_neighbors.cpp` (6), `test_open_heap.cpp` (4), `test_find_basic.cpp`
  (9), `test_find_costs.cpp` (4), `test_tie_order.cpp` (5: straight, diagonal-vs-straight by lower h — path (0,0),
  (1,1), (2,1), 3 expansions; the symmetric pillar by the most recent push — over the top, 7 expansions; equal weights
  both ways; **find = literal A\* on 300 seeded maps × 3 searches**, scale 16 and 24).
- **Probe** (T2.6, `build/ref-025/probe/`): a 3ds-release program calling only `PathMap::create` and `find` links 20
  pathfinding symbols (create, the searcher, begin/step/finish, misuse) and nothing of sync, fields, smoothing or the
  follower; one calling only `collision::move` links none. Guards green; sizes and `nm` unchanged.

## Stage 3 refinements and results

- **Limits, partial paths, slicing** as planned; the stage-2 code already held them (the tests were written against
  it; the mutation pass in stage 8 is what proves they bite).
- **The doomed block** (map destroyed during sliced work): `~PathMap` marks the block doomed and clears its own pointer
  *before* the assert, so the Debug crash path and the Release path leave the same state; the searcher's next `step()`
  ends `Invalid` and frees the block, and so do `cancel()`, a new `begin()` and the searcher's destructor (tested
  through `mem::live_allocations()`).
- **`step()` after a finished search** returns true and changes nothing; a negative step size asserts and returns
  whether the search is finished.
- **Cost test**: `nnh::events::Stats` has no publish counter, so FR-21 is checked through its counters (subscribers,
  dropped events) and by the module never including `<nnh/events.hpp>`; no console line is written in the normal path
  (fake platform's `console_lines`).
- Tests: `test_limits_partial.cpp` (8: the goal as the limit-th pop, every status, crafted partial ties, an island's
  shore, blocked goal / limit / start-only partials, a 63 × 63 comb map with a searcher of exactly its size at scales 16
  and 64 against the literal A*), `test_slicing.cpp` (6: slices 1/7/64/whole = find on 300 searches, cancel and
  restart, stale with the current cells, a moved map, a destroyed map two ways), `test_searcher_reuse.cpp` (4: 10 000
  searches = fresh searchers, the stamp wrap, two interleaved searchers, sizes), `test_inspect.cpp` (3),
  `test_path_misuse.cpp` (2), `test_path_cost.cpp` (2). All path tests run in 2.3 s in pc-debug. Guards green.

## Stage 4 refinements and results

- **The walk** (`smooth_walk.hpp`, decision 18): one compare per step, `(2kx + 1)·|dy|` against `(2ky + 1)·|dx|`, which
  also orders the steps once an axis is done (no special cases); a corner is reported on the diagonal cell with the walk's
  signs, its side cells being `(x, y − sy)` (the x-neighbour) and `(x − sx, y)` (the y-neighbour), spec 019's order.
  Equal to `collision::cells_crossed` cell for cell and corner for corner on 20 000 seeded centre pairs (short ones
  on 9 × 9 for frequent corners, long ones on 64 × 64), plus horizontal, vertical and 45° cases.
- **PC measurement** (decision 18, pc-release GCC 16, 200 000 random pairs on 64 × 64, three runs, noisy machine): the
  integer walk 118–124 ns per segment, `cells_crossed` (spec 019's `GridWalk`, lattice path, plus the cell lookups and
  the output) 846–884 ns: ≈ 7×. The copy stays.
- **Corners with `Never`** (4 neighbours): treated as OnlyIfNoObstacles (both side cells walkable) — a 4-neighbour
  agent can't squeeze through a corner either; the spec's text names only the other modes.
- **Weights**: a cell crossed through its interior must be no heavier than the heaviest path cell of the replaced
  part `path[a + 1 .. j]` (the anchor excluded, so a blocked or heavy start is never checked); side cells at a corner
  are only checked by the rule.
- **Follower**: advances only from its current waypoint (never skips one that isn't within the radius), arrival only
  at the last; an empty list or a bad `reset` (null points with a count, negative count, NaN or negative radius) leaves
  it empty and arrived.
- Tests: `test_smooth_walk.cpp` (2), `test_smooth.cpp` (7, incl. 500 seeded smoothings whose every segment passes the
  exact checker with the heaviest replaced weight), `test_follower.cpp` (6). Guards green.

## Stage 5 refinements and results

- **Fields** as planned: `field_build.cpp` settles a cell, computes `d + S·w(c)` and `d + D·w(c)` once and relaxes the
  4 or 8 neighbours with loads, compares and stores (the side cells of a diagonal are the settled cell's orthogonal
  bytes, loaded once); the heap is keyed `(d, padded index)` and compacted when full. `begin` checks every goal before
  touching anything (an outside goal asserts first, so the Debug crash leaves the field as it was; Release ignores it).
- **Single-buffered** fields are not ready from `begin` until the build finishes (a cancelled build leaves them not
  ready: their only array was being rewritten); double-buffered ones keep answering from the front array, whose map size
  is kept apart (`ans_w/ans_h/ans_stride`).
- **A map destroyed mid-build**: the next `step()` ends the build (as `cancel()`), the last finished build keeps
  answering (double-buffered), the block is freed by the field.
- **`next()`** needs the map the field answers for (same size; else misuse, false); `next_point()` returns false
  without asserting for a point outside the map (an agent pushed off the level).
- **Memory** (tested through the tag): `header + 4 B × padded cells × (1 or 2) + 8 B × cells`.
- Tests: `test_field_build.cpp` (8, incl. 300 seeded maps against the multi-source reference with caps),
  `test_field_slices.cpp` (5), `test_field_next.cpp` (5: brute-force `next` on 100 maps (> 10 000 cells), neighbour-
  order ties, the corner rule, `next_point`, field = the search's cost from every walkable cell on 200 maps with 1–2
  goals). The pc-release run caught a wrong expected value in a Release-path check (34, octile, not 40) — fixed in the
  test. Guards green.

## Stage 6 refinements and results

- **AC-2** (`test_reference_random.cpp`): pc-debug runs 512 maps (all path tests together took 5.6 s in Debug, over
  the 5 s budget with 2048), pc-dev/pc-release run **2048 maps**: 8 192 searches (2 647 Found, 2 723 Partial, 2 048
  sliced; limits 1–60 on a quarter, scale 40 on a fifth), 2 048 fields (1–4 goals, a seventh capped at 200) with every
  cell's `next` checked to lower the cost-to-go by exactly its step, 26 990 smoothed segments against the exact checker
  — all equal to the references (pc-release: 3.1 s for every path test).
- **Maps** (`examples/pathfinding/check_maps.{hpp,cpp}`): `rooms` (8 × 8 blocks, a 6–7-cell room in each, joined right
  and down; 29 % walls at 64 × 64 and 128 × 128, 100/100 seeded pairs connected), `maze` (iterative backtracker with
  a caller-given stack; 54 % walls at 48 × 48), `swamp` (4 × 4 weight patches, a walled island). `ByteGrid` gives the
  `collision::Grid` callback and the weight callback over the same bytes (so `sync(grid, weights)` is exercised).
- **The golden** (`self_check.{hpp,cpp}`): as planned, M4 being a copy of M1's bytes at agent size 2 (its doors toggle
  its own copy). The fields' `next` asks 64 seeded cells; door edits rotate fill / set_cell / region sync. Recorded in
  pc-debug, equal in pc-release: **`kPathfindingCheckGolden = 1C0B5992`**, checkpoints `8332890F BF55A00B B3E6C49A
  C825A5E7 77C6FFC5 E738F010 528F7687 1C0B5992`. The same in slices of 8 searches (64 frames) as in one run.
- **Hand check** (T6.2): flipping the `seq` tie (`a.seq < b.seq`) moves the golden to `23575D7F` and fails 8 tests
  (the golden, the literal-A* comparisons, the heap order tests, AC-2); restored.
- **Fuzz** (T6.3): `tests/fuzz/fuzz_path_ops.{hpp,cpp}` (own header: `fuzz_targets.hpp` untouched for the merge),
  `libfuzzer_path_ops.cpp`, mini-fuzz `test_fuzz_path_ops.cpp` (200 inputs ≤ 4 KiB): paths valid, sliced = one-shot
  (status, cost, cells, expansions, every cell), waypoints keep both ends, `next` lowers the distance; misuse values only
  in Release builds. In `tests/CMakeLists.txt`: one `target_sources(nnh_fuzz_targets ...)` and an
  `if(NNH_BUILD_FUZZERS)` with `fuzz_path_ops`, both in 025's block.
- Guards green.

## Stage 7 refinements and results

- **Level** (`art/make_level.py` → `assets/levels/keep.tmj`; `art/make_art.py` → `assets/art/tiles.png`, 8 tiles):
  48 × 30; `ground` tiles carry `path_cost` (road 1, floor and doorways 2, swamp 4, water 8 — floor 2 so roads matter;
  the plan named swamp 3); `walls` (layer property `collision`) holds walls, deep water and closed doors; three `door`
  points, open at start (A closes/opens them in turn: the `door` tile on `walls`, then a one-cell region sync of both
  maps); shallow water (weight 8) around deep water around an island; the thick wall's corridor at row 20 is one cell
  wide. Actors are shapes (hero green, chasers red, goal yellow).
- **Weights pattern** (`level.cpp`): `cost[id]` from each tile's `path_cost`, a callback returning
  `cost[map.tile(ground, x, y).id]` — well under 15 lines, so no `sync(tilemap)` convenience (decision 24 holds).
- **Controls** as planned plus **D-Pad Down = pause the hero** (it still plans): the script pauses it so its door and
  mode lines come from the spawn cell, whatever it walked.
- **Frame order**: the hero and the crowd update *before* the frame's controls, so a re-plan caused by a door or a mode
  change is logged the frame after the change (an `expect` window starts after the previous line's frame). The check
  scene logs its verdict the frame after the last checkpoint for the same reason.
- **Script waits** are `wait 10` after an `expect` (the guide's minimum; the parser warns below it), not 3.
- **Bench**: workloads as planned; judged on the average except `maze slices` and `field slices` (the worst slice: AC-7
  says "every slice"); `next_crowd` logged, never judged; the door's tile edits are untimed (`prepare_door`), its 3 × 3
  sync timed. The smoothing path is two searches on `rooms(128, 128, 7)` joined end to end, first 200 cells (no single
  seeded pair reached 200). A first line logs the sizes: maze path 405 cells / 494 expanded, smooth path 200 cells,
  replans 32 pairs **4 518 expanded**, astar 32 pairs 81 359 expanded.
- **Screenshots** read back (pc-debug, frame 40, hero paused at the spawn): `explored` (705 expanded cells tinted, the
  frontier outlined, the raw path through the corridor, 47 cells, cost 834), `smoothed` (5 waypoints), `field` (heat map
  on both screens), `partial` (the island: 34 cells to the lake's edge, every reachable cell expanded), `door-closed`
  (cost 1040, round through the corridor) and `door-open` (cost 438, through door 1), `big` (the 2 × 2 hero, partial on
  its side of the one-cell corridor and doors).
- **Self-check in the example**: `PATHFINDING_DEMO=check` logs the 8 checkpoints at frames 8..64, `pathfinding: PASS
  1C0B5992`, then `pathfinding: showcase`; ctest `pathfinding_self_check_pc` (2.6 s).
- **Script** `examples/pathfinding/tests/check.nnhs` with `--pc` (pc-debug): `nnh: PASS (104 lines, frames 1-3681)`;
  ctest `pathfinding_check_script_pc` (71 s, `RESOURCE_LOCK nnh_remote_port`). Its lines: `field: built 3,3 settled 1135
  frames 3`; the 8 checkpoints and `PASS 1C0B5992`; `pathfinding: showcase`; `hero: paused`; `goal: 24,5 found cells 22
  cost 438 expanded 251 waypoints 3`; `door: 1 closed version 2`; `replan: version 2 found cells 48 cost 1040 expanded
  777 waypoints 9`; `goal: 34,7 partial cells 41 cost 1012 expanded 1134 waypoints 7`; `hero: 2x2`; `goal: 24,20
  partial cells 18 cost 428 expanded 378 waypoints 3`; `mode: never`; `hero: 1x1`; `hero: walking`; the bench lines and
  `pathfinding bench: PASS` (Debug: not judged).
- **PC bench** (pc-dev, GCC 16 -O2, three other builds running — noisy, and PC numbers say nothing about the console):
  astar 0.200 µs per expanded cell (worst frame 0.386); maze one 0.043 ms; maze slices avg 0.025, worst 0.058 ms;
  replans 1.086 ms; field one 0.667 ms; field slices avg 0.163, worst 7.656 ms (one preempted frame); next 0.034 ms;
  next_crowd 0.020 ms; smooth 0.012 ms; sync 0.140 ms; door 0.002 ms.

## Stage 7 ARM pre-check (T7.6)

3ds-dev objects (`arm-none-eabi-objdump -drC`, listings in `build/ref-025/dis/`). **Found and fixed before any console
run (no result changes; golden unchanged):** GCC kept `Expander::relax` out of line (28 call sites in
`search_step.o`), and `OpenHeap::take` too — a call, spills and reloads per neighbour. They are now forced inline
(`NNH_PATH_INLINE`, as `det_sine.hpp` does), and so are the field heap's `before/heap_push/sift_down/side_ok`. After the
fix the expansion loop calls only `open_compact` (cold, never in practice) and `search_finish` (once); `field_run`
calls nothing; there is no `__aeabi_*` anywhere in the module and no `vmrs` in search, field, smoothing or sync (only in
float geometry, the follower and misuse checks); one `memset` (the field's `begin` fill, 4 B × padded cells); heap
entries move with four-word `ldm`/`stm`; the `(f, h)` key compares with `cmp`/`sbcs`; `step × w` is one `mul` (run-time
costs: no shift-add chain). `search_step.o` grew to 6 939 instructions (four modes × eight inlined relaxations: about
7 KB per mode, within the 16 KB I-cache for the one mode a game uses).

From the listing (OnlyIfNoObstacles): a pop through the buffer ≈ 12 instructions, through the heap ≈ 14 per sift level
(two stack reloads: register pressure); a skipped neighbour 3–4; a reached-not-lower one ≈ 8; a relaxed and offered
one ≈ 38 (heuristic 9, record 6, partial 7 when on, offer 8–16, sift-up ≈ 10 per level). Misses: the map at 1 B per
cell (a 3 × 3 block is 3 lines), records at 8 B (5 lines), the heap's top in L1; fields 4 B per cell (3 lines) plus
the map's 3; `next` for scattered agents misses on nearly every row it reads.

| AC-7 workload | Work (measured on PC) | Estimate (1.9 CPI + 110 per miss; × 1.3 upper) | Target | Verdict |
|---------------|-----------------------|-------------------------------------------------|--------|---------|
| astar, per expanded cell | 81 359 expansions in 32 searches | 190–270 instr, 1.0–2.5 misses → 1.8–2.9 µs (3.8) | < 2.5 µs | **at risk** |
| maze one | 494 expansions (corridors: ≈ 1.2 pushes each) | 1.3–2.0 µs each → 0.65–1.0 ms (1.3) | < 8 ms | PASS |
| maze slices (worst) | 256 expansions | 0.33–0.51 ms (0.67) | < 0.8 ms | PASS |
| replans | **4 518 expansions** (≈ 7 per path cell: weights 1/3/8 average 2.1 against a weight-1 heuristic) + 32 begins | 1.8–2.9 µs each → 8.2–13 ms (17) | < 3 ms | **FAIL expected** (the plan assumed 770–1 600 expansions) |
| field one | ≈ 2 900 settled | 150–210 instr + 1–2 misses → 1.6–2.3 µs → 4.6–6.7 ms (8.7) | < 8 ms | **at risk** |
| field slices (worst) | 512 settled (+ the begin fill in a build's first) | 0.8–1.2 ms (1.6) | < 1 ms | **at risk, likely FAIL** |
| next, 256 scattered agents | 8 neighbours, 2 arrays | ≈ 100 instr + 4–6 misses → 1.9–3.2 µs → 0.48–0.82 ms (1.1) | < 0.3 ms | **FAIL expected** (as the plan said) |
| smooth, 200 cells | ≈ 3 000 walk steps | ≈ 12 instr per step → 0.25–0.5 ms (0.65) | < 1 ms | PASS |
| sync 64 × 64 (tilemap callback + table) | 4 096 cells, two calls each | 170–280 cycles each → 2.6–4.3 ms (5.6) | < 5 ms | **at risk** |
| door 3 × 3 | 9 cells + the call | 0.01–0.02 ms (0.03) | < 0.05 ms | PASS |

Expected console verdict: `pathfinding bench: FAIL (replans, next, …)` — the run fails at the verdict, after every
number is logged. The remaining "no result change" mitigations (risk 1: a per-cell allowed-steps byte, `pld` and a
batch `next`, a 4-ary heap, packed field words, direct `cells` reads in sync) can't reduce `replans`' expansion count,
which comes from the workload's weights; they are left for T10.3 with the console's numbers (the
measurement-with-margin policy) rather than guessed now.

## Mutation testing (T8.1)

58 hand mutants (`build/ref-025/mutants.py`, applied one at a time by `mutate.py`; each rebuilds `nnh_tests` and runs
every `path*`/`pathfinding*` test; 54 in pc-debug, 4 Release-path mutants in pc-release): the order's keys and its
`seq` tie, the buffer, the heap's sifts, compaction and heapify, the strict-g rule, reopening, the neighbour order, each
diagonal rule and a diagonal's side cells and cost, the limit, staleness, partial ties (index, g, scaled h), the heuristic
scale and octile formula, the blocked-goal answer, the stamp wrap, the stale flag, the default limit, footprints (edge,
max, range), one-way cells, versions, the walk's corners and tie, smoothing's corner cells, weight rule and Never rule,
the field's diagonal cost, cap, side cells, swap, stale skip and duplicate goals, `next`'s ties, goal and rule, the
follower, region clipping, the path prefix, inspection stamps, the doomed map, and Release clamps and ignores.

**First pass: 52 killed, 6 survived.** Three were real gaps, now tested and killed: `field-dup-goal` (a duplicate goal
settled twice: the several-goals test now checks `settled()` against the same build without duplicates),
`follower-radius` (`<` for `<=`: a waypoint exactly on the radius is now passed), `rel-follower-nan` (Release: a NaN
position must leave an arrived follower arrived). Three are **equivalent** and stay:
- `heap-offer-keeps-worse` — which child the buffer holds doesn't change the pop order: `take()` always compares it with
  the heap's top (only speed changes);
- `stale-entry-kept` (the pop's `g == f − h` check dropped) — a newer entry for a cell has the same h and a strictly
  lower g, so it pops first and closes the cell; the closed check then rejects every older one (the g check matters only
  in compaction, which keeps it);
- `follower-arrived` (arrival without "at the last waypoint") — the advance loop stops before the last point only when
  the point is out of the radius, so the extra condition never decides.

**Score: 55 of 58 killed (94.8 %); every non-equivalent mutant killed.** Results in `build/ref-025/mutation.txt`.

## Stage 8 refinements and results

- **Release strings** (T8.2): `scripts/check_release_strings.sh` passes on the 3ds-release `pathfinding.elf` (4.49 MB)
  and the pc-release `pathfinding.exe`.
- **Opt-in by linking** (T8.3, `build/ref-025/probe/`, against the 3ds-release libraries): a program calling only
  `PathMap::create` and `find` links 22 pathfinding symbols (map create, searcher, begin/step/finish, misuse) and no
  sync, field, smoothing or follower code (`.text` 154 764 B, 135 020 before the forced inlining of stage 7); one calling
  only `collision::move` links none (`.text` 42 152). `hello` sizes and `nm` lists equal stage 0's at every stage end.
- **Regression** (T8.4): `compare.sh now` — hello, queries, entities, prefabs, platformer, replay 64/312 byte-identical
  to stage 0; every example's `--pc` script ctest passes (prefabs, pathfinding); `.text` of the 3ds-release queries,
  entities, prefabs and platformer ELFs unchanged; `pathfinding` `.text` 294 340 B.
- **Build times** (T8.4, pc-debug simulator / 3ds-dev `.3dsx` after touching one game file, three runs, noisy):
  `examples/pathfinding/hero.cpp` 1.90–1.94 s / 4.55–4.62 s; `examples/queries/queries_scene.cpp` 1.86–1.90 s /
  3.81–3.88 s (the 3DS times include `dkp.sh`'s MSYS2 start). Within constitution III (< 2 s, < 5 s).
- **Header** (T11.4): `<nnh/pathfinding.hpp>` 8 023 preprocessed lines, 405 ms `-fsyntax-only` against 408 ms for
  `collision.hpp` + `result.hpp`.

## Stage 9: Linux checks (left to the lead)

On `nnhge-build` (VM 120), as specs 020–021: GCC 13, GCC 14 and clang 18 with ASan/UBSan — `nnh_tests` and ctest pass;
the golden `1C0B5992` and its checkpoints, AC-2 (2 048 maps in pc-dev/pc-release), the walk = `cells_crossed` and the
replay ctest identical on all three; libFuzzer `fuzz_path_ops` 10 minutes (`-DNNH_BUILD_FUZZERS=ON`, clang).

## Stage 9 results

- GCC 13 and GCC 14 built and passed `nnh_tests` and ctest (this spec's own VM run); `fuzz_path_ops` 600 s, 1 573 982
  runs, cov 1765, no finding.
- LeakSanitizer (clang 18, leaks on) reported 2 037 bytes in four allocations, all test-only: a `PathMap` or
  `PathSearcher` created inside a function run by `run_expecting_crash` leaks because the assert longjmps past its
  destructor — `step_without_begin` (`test_path_misuse.cpp`), `map_too_big` (`test_find_basic.cpp`, a map and a
  searcher) and `map_too_big` (`test_field_build.cpp`). Fixed as spec 020 did: the objects are made by the test and
  reached through file-scope pointers, so nothing is constructed inside the asserting call.
- Rerun with spec 022's stage 9 fixes ([plan 022](../022-scene-tree-and-transforms/plan.md#stage-9-results)): GCC 13
  and 14 ctest 269/269, clang ASan/UBSan ctest 269/269 and `nnh_tests` 2571 with leaks on, clean.

## Stage 11 results

- `docs/api/pathfinding.md` (maps, syncing, weights from Tiled, agent sizes, the diagonal rules drawn in text, costs and
  the 32-bit bound, searches and statuses, the total order, slicing and stale results, re-planning, smoothing, fields
  and crowds with the naive flee, the follower, memory, cost, misuse). The diagonal rules are drawn as text diagrams in
  the page instead of `docs/images/pathfinding/*.png` (no image pipeline needed; the wiki renders them as they are).
- Links: `collision.md` (end of "Grids"), `tilemap.md` (end of "Collision layers"), `guides/levels.md` ("Slow tiles for
  pathfinding" at the end of "4. Collision"); `patterns.md` (the Time-sliced job row points at the module);
  `decisions.md` (D40 implemented, change-log entry); `REFERENCES.md` (025's block); `CLAUDE.md` (layout paragraph after
  "Collision layers and queries", two commands after `ANIMATION_DEMO=check`); `ROADMAP.md` (the item checked);
  `specs/README.md` and the three spec files' status. `python -I scripts/docs_tool.py check`: 0 broken links.

## Console run 1 (T10.2)

Old 3DS, Development `.3dsx`, `nnh run pathfinding --script examples/pathfinding/tests/check.nnhs`, 2026-11-24. **Logic:
every line equal to the PC's** — the eight checkpoints and `pathfinding: PASS 1C0B5992`, the showcase lines (goals,
door, re-plan, partial, 2 × 2 hero, mode, field). **Bench** (avg, worst; AC-7's targets as written; T7.6's estimate):

| Workload | Avg | Worst | Target | Verdict | T7.6 estimate |
|----------|-----|-------|--------|---------|---------------|
| astar, per expanded cell | 4.257 µs | 4.416 | < 2.5 µs | FAIL | 1.8–2.9 |
| maze one | 0.716 ms | | < 8 ms | PASS | 0.65–1.0 |
| maze slices | 0.384 ms | 0.498 | < 0.8 ms worst | PASS | 0.33–0.51 |
| replans (32 pairs, 4 518 expansions) | 15.065 ms | 16.589 | < 3 ms | FAIL | 8.2–13 |
| field one | 8.282 ms | 8.337 | < 8 ms | FAIL | 4.6–6.7 |
| field slices | 1.424 ms | 1.645 | < 1 ms worst | FAIL | 0.8–1.2 |
| next (256 scattered agents) | 0.772 ms | 0.882 | < 0.3 ms | FAIL | 0.48–0.82 |
| next_crowd | 0.634 ms | | not judged | | |
| smooth | 0.224 ms | | < 1 ms | PASS | 0.25–0.5 |
| sync | 3.856 ms | 5.939 | < 5 ms (avg) | PASS | 2.6–4.3 |
| door | 0.039 ms | 0.123 | < 0.05 ms | PASS | 0.01–0.02 |

Verdict `pathfinding bench: FAIL (astar, replans, field one, field slices, next)`; per expanded or settled cell the cost
was ≈ 1.5–2× T7.6's estimate. T10.3 follows (risk 1's list first, no result may change), then the budgets.

## Optimisation round (T10.3)

Done on the PC in the `opt-025` worktree (from `main` at `499f433`); no console run yet.

**A calibrated ARM11 model.** To find where the cycles go without the console, the module was run under a small ARMv6
interpreter (scratch tools in `build/o25/`, not committed): the pathfinding sources and `check_maps.cpp` compiled with
the 3ds-dev flags (the objects' instructions hash-identical to `build/3ds-dev`'s), stubs for memory and logging, and a
driver running the bench's workloads between markers. Timing: 1 cycle per instruction, load results usable 3 cycles
after issue, MUL +1 and UMULL +2 cycles, LDM/STM ⌈n/2⌉, a 2-bit predictor per branch (backward taken at first), a
return stack, 6 cycles per mispredict, a 16 KB 4-way L1 D with 32-byte lines (random replacement, allocate on read and
write, flushed at each workload's start) and 110 cycles per miss — textbook ARM11 values, nothing fitted. Against run 1
it lands within −10 % … +14 %: astar 4.47 µs (console 4.26), replans 13.5 ms (15.1), maze one 0.70 ms (0.72), field
one 7.87 ms (8.28), field slices worst 1.55 ms (1.65) and average 1.37 ms (1.42), next 0.88 ms (0.77), next_crowd
0.61 ms (0.63).

**Why T7.6 was ≈ 1.5–2× low.** Misses were about right (A* ≈ 2.8 per expansion); the **dynamic instruction counts**
were 2.5–3× the listing-based guesses, and T7.6's 1.9 CPI did not hold for this branchy, register-starved code:

- A* (645 instructions, 1 198 cycles per expansion; estimated 190–270): on the weighted rooms map the heap holds stale
  entries and the one-entry buffer wins only ≈ 15 % of takes, so an expansion pays ≈ 8 sift levels and ≈ 20 key
  compares (each ≈ 7 instructions with its seq tie test: ≈ 300 cycles), 13 mispredicts (≈ 80 cycles), ≈ 125 cycles of
  load-use stalls (the orthogonal bytes spilled and reloaded per side check) and 2.8 misses (≈ 300 cycles: records,
  heap children, map rows).
- Fields (469 instructions per settled cell; estimated 150–210): the neighbour loop wasn't specialised (the mode
  tested, the side cells read through tables and the orthogonal bytes spilled inside the loop), two-word `(d, cell)`
  compares, 17 mispredicts.
- `next` (349 instructions per agent; estimated 85–110): `neighbour_offsets` rebuilt per call (8 multiplies), the loop
  over directions, and ≈ 3 misses on two arrays for scattered agents.

**Changes (no result changes; every golden holds):**

1. **Fields, the build**: one template per diagonal mode with the neighbour loop unrolled, the orthogonal walkability in
   a 4-bit mask, `d + S·w` / `d + D·w` checked against `max_cost` once; the open set is a **4-ary heap of 64-bit keys**
   `(d << 32) | padded index` (`field_heap.hpp`): one compare orders two entries, a node's four children share a
   32-byte line (the array starts 8 bytes before a line), a pop walks the hole to a leaf and lifts the last entry
   (fewer compares). Keys are unique (a cell is pushed only with a strictly lower d, a goal once), so the pops are the
   sorted order whatever the heap's shape: the same settled cells in every slice.
2. **Fields, `next`**: the build stores each cell's answer in a **step byte** (1 + the direction, beside each distance
   array): a relaxation that sets a cell's distance stores the direction back to the settled cell, one that ties it
   keeps the lower direction — every neighbour whose candidate equals d(p) is settled before p (step × weight ≥ 1), so
   when p settles the neighbour-order argmin is complete, and it is exactly `next`'s minimum (Bellman). `next` reads the
   byte while asked with the build's map (a new `PathMapBlock::serial`, unique per created map, never 0) at the version
   the build began at; after any edit (versions only grow) or with another map it computes the answer from the current
   cells as before. Tests first: three pinning that fallback (an edit, another map of the same size and version, a
   double-buffered rebuild answering from the finished build) and the memory test (5 B per padded cell per array);
   six hand mutants killed (no version check, no serial check, the back buffer's steps, no tie update, no clear at
   `begin`, goals unmarked).
3. **Fields, `begin`**: only the step bytes are cleared (4.3 KB at 64 × 64 instead of 17 KB of distances and the
   steps): step 0 means "not reached by this build" (its distance word is stale), goals are marked `kGoalStep` (9);
   `distance()` and the computed `next` read through the byte. A build's first slice no longer pays ≈ 0.2 ms of write
   misses.
4. **A\***: the plain pop refills the hole bottom-up (to a leaf along the better child, then the last entry lifted:
   about half the compares of sifting it down); the heap array starts 16 bytes past a 32-byte line, so a node's two
   children share a line (the searcher's block now puts the heap before the records). The buffer, the compares and the
   total order are unchanged.

**Tried and dropped** (no gain in the model): the A* loop on local copies of the block's constants, open set and
sequence (aliasing was not the cost; register pressure is); a binary field heap with the bottom-up pop (+60 %
mispredicts: the unrolled 4-ary code compiles to conditional moves); the 17 KB fill kept with the mask (slower per
cell than the marker's worst slice gains); a relaxation that branches on the step byte first (a longer dependent load
chain).

**Not done from risk 1's list**: (a) a per-cell allowed-steps byte — the side checks are ≈ 5 % of an expansion and it
would cost a byte per map cell and sync time (sync is the next workload at risk); (b) `pld` — the model can't say what
the ARM11 MPCore does with it, and `next` no longer needs a batch form; (c) a 4-ary A* heap — compares, not lines,
dominate A*, and a 4-ary level needs three; (d) packing weights into the field — superseded by the step byte; (e) sync
— untouched, it passed.

**Model, per unit, before → after** (instructions; cycles; misses; total):

| Workload | Instructions | Cycles | Misses | Model total |
|----------|--------------|--------|--------|-------------|
| astar, per expansion | 645 → 619 | 1 198 → 1 142 | 2.88 → 2.61 | 4.47 → 4.26 µs |
| replans, per expansion | 535 → 526 | 800 → 792 | 0.47 → 0.46 | 13.5 → 13.3 ms |
| maze one, per expansion | 209 → 208 | 382 → 382 | 0.76 → 0.76 | 0.70 → 0.70 ms |
| field one, per settled cell | 469 → 401 | 734 → 559 | 0.41 → 0.43 | 7.87 → 5.99 ms |
| field slices (6 per build), worst / avg | 451–487 → 375–420 | | | 1.55 / 1.37 → 1.19 / 1.06 ms |
| next, per agent | 349 → 84 | 923 → 207 | 3.20 → 0.81 | 0.88 → 0.20 ms |
| next_crowd, per agent | 349 → 84 | 642 → 163 | 0.66 → 0.41 | 0.61 → 0.16 ms |

**Old 3DS estimate after T10.3** (the model's new total × that workload's console/model ratio in run 1):

| Workload | Run 1 | Estimate | Target | Expected |
|----------|-------|----------|--------|----------|
| astar | 4.257 µs | ≈ 4.06 µs | < 2.5 µs | FAIL |
| maze one / maze slices worst | 0.716 / 0.498 ms | unchanged | < 8 / 0.8 ms | PASS |
| replans | 15.065 ms | ≈ 14.9 ms | < 3 ms | FAIL |
| field one | 8.282 ms | ≈ 6.3 ms | < 8 ms | PASS |
| field slices worst (avg) | 1.645 (1.424) ms | ≈ 1.27 (1.10) ms | < 1 ms worst | FAIL |
| next | 0.772 ms | ≈ 0.17 ms | < 0.3 ms | PASS |
| next_crowd | 0.634 ms | ≈ 0.16 ms | not judged | |
| smooth, sync, door | 0.224, 3.856, 0.039 ms | unchanged (code untouched) | | PASS |

A* stays ≈ 1.6× its target and the re-plans ≈ 5×: the expansion count is fixed by the order (4 518), and per expansion
the cost is ≈ 20 heap compares plus record misses — what's left is an algorithmic choice (a bucketed open set for small
cost ranges, JPS for uniform costs, a stronger heuristic for weighted levels), not a tuning, so AC-7's policy applies:
the budgets come from the next console measurement with ≈ 25 % margin.

**PC bench** (pc-dev, GCC 16 -O2, avg / worst, other builds running: noisy, and the PC says little about the ARM11;
before = a clean `git archive` of `499f433`): astar 0.183 / 0.386 → 0.187 / 0.349 µs; maze one 0.033 → 0.032 ms; maze
slices 0.021 / 0.054 → 0.020 / 0.078 ms; replans 0.880 → 0.916 ms; field one 0.563 → 0.326 ms; field slices 0.103 /
0.246 → 0.060 / 0.529 ms (one preempted frame); next 0.023 → 0.005 ms; next_crowd 0.020 → 0.005 ms; smooth 0.012 →
0.011 ms; sync 0.101 → 0.113 ms; door 0.002 → 0.002 ms. The same expansion and size line (`maze path 405 cells, 494
expanded; … replans 32 pairs, 4518 expanded; astar 81359 expanded`).

**Memory**: fields + 1 byte per padded cell per distance array (64 × 64 double-buffered 66 → 74.5 KB); searcher and
field blocks + 32 bytes (header alignment); the map's header is unchanged (the 4-byte serial fits its padding, 80 bytes
on 64-bit).

**Checks**: pc-debug and pc-release build with 0 warnings, ctest 273/273 and 259/259 (golden `1C0B5992` and its
checkpoints, AC-2's 512 / 2 048 random maps against the literal A* and the brute-force references, the mini-fuzz, the
`check.nnhs --pc` script, entities `C8FB9A41`, prefabs `7BB7901E`, hierarchy `23DB5C36`, the platformer replay
`E445E8B2`); 3ds-release and 3ds-dev build every target with 0 warnings; the model's results checksum is the same before
and after. `hello`: pc-release 278 856 B and 3ds-release `.3dsx` 246 512 B (stage 0's), `nm` lists identical to a clean
build of `499f433`. `python -I scripts/docs_tool.py check`: 0 broken links.

**Next (console, the maintainer presses Y):** Homebrew Launcher open on the Old 3DS, then on the PC
`build/pc-debug/tools/nnh/nnh run pathfinding --script examples/pathfinding/tests/check.nnhs` (≈ 75 s). Expected: every
logic line as in run 1 (`pathfinding: PASS 1C0B5992`), the bench lines near the estimates above, and the verdict
`pathfinding bench: FAIL (astar, replans, field slices)` if the estimates hold. Then the budgets (spec AC-7,
`bench_scene.cpp`, `docs/api/pathfinding.md`) from that run with ≈ 25 % margin, and T10.2 run 2 (the platformer).
