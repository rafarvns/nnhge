# Plan 025: Pathfinding on grids

Status: approved 2026-11-24 (overnight pre-approval)
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
| `DistanceField`, double | 8 B × P + 8 B × W·H | 66 KB | 260 KB | 16.0 MiB | front, back, heap |
| `DistanceField`, single | 4 B × P + 8 B × W·H | 49 KB | 194 KB | 12.0 MiB | |
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
17. **Field heap order** `(d, cell index)`; `next` reads neighbours (no stored directions).
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
