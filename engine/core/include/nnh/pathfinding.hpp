#pragma once
/// @file nnh/pathfinding.hpp
/// Pathfinding on grids (spec 025, decision D40): a path map baked from any collision::Grid (blocked cells and
/// integer weights), bounded and time-sliceable A* searches that never allocate, partial paths, smoothing into
/// straight waypoints, distance fields toward many goals for crowds, and a small path follower. Integer costs and a
/// fixed total order: the same calls give the same paths, bit for bit, on PC and 3DS. See docs/api/pathfinding.md.
///
/// Optional (constitution XI): a game that doesn't call these links none of the module. Light: <cstdint>,
/// <nnh/math.hpp>, <nnh/collision.hpp>, <nnh/result.hpp>; no templates, no standard containers.
/// Nothing allocates after create(); nothing calls game code except sync()'s weight callback; nothing logs in the
/// normal path; nothing needs a frame. Misuse asserts in Debug/Development and does nothing harmful in Release.
///
/// @code
/// auto map = nnh::PathMap::create(nnh::path_map_desc(level.collision_grid())).value();
/// map.sync(level.collision_grid(), {&path_cost, &level});          // walls blocked, swamp slower
/// auto searcher = nnh::PathSearcher::create(map.width(), map.height()).value();
/// const nnh::PathResult r = searcher.find(map, map.cell_at(hero), map.cell_at(goal), {.partial = true});
/// nnh::CellPos cells[64];
/// const int n = nnh::smooth_path(map, cells, searcher.path(cells, 64), cells, 64);
/// @endcode

#include <cstdint>

#include "nnh/collision.hpp"
#include "nnh/math.hpp"
#include "nnh/result.hpp"

namespace nnh {

struct MemTag; // <nnh/memory.hpp>
class PathSearcher;
class DistanceField;

namespace detail {
struct PathMapBlock;
struct PathSearchBlock;
struct FieldBlock;
struct PathAccess;
} // namespace detail

/// A cell of a path map: (0, 0) is the map's first cell (grid cell (grid_x, grid_y) of the grid it mirrors).
struct CellPos {
    int x = 0;
    int y = 0;
    friend constexpr bool operator==(CellPos, CellPos) = default;
};

/// A rectangle of cells in map coordinates.
struct CellRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

inline constexpr int kPathBlocked = -1;                    ///< a cell value: not walkable
inline constexpr std::uint32_t kUnreachable = 0xFFFFFFFFu; ///< a cost or distance nobody reached

/// Which steps a search takes (Godot's diagonal modes). Never: 4 neighbours (N, E, S, W). The others: 8, a diagonal
/// step past the two orthogonal cells beside it allowed always / when one of them is walkable / when both are (no
/// corner cutting: spec 019's "corners never leak", the default).
enum class Diagonals : std::uint8_t { Never, Always, AtLeastOneWalkable, OnlyIfNoObstacles };

/// How to create a PathMap. Every field has a default except the size; path_map_desc() fills one from a grid.
struct PathMapDesc {
    int width = 0;                      ///< cells, 1..1024
    int height = 0;                     ///< cells, 1..1024
    Vec2 origin{};                      ///< the grid's origin: world position of grid cell (0, 0)'s top-left
    float cell_w = 16;                  ///< pixels, finite and > 0
    float cell_h = 16;                  ///< pixels, finite and > 0
    int grid_x = 0;                     ///< the grid cell that is map cell (0, 0) (a region of a bigger level)
    int grid_y = 0;
    Diagonals diagonals = Diagonals::OnlyIfNoObstacles;
    int straight_cost = 10;             ///< >= 1
    int diagonal_cost = 14;             ///< straight..2 x straight; width x height x diagonal x 259 < 2^32
    int agent_size = 1;                 ///< 1..4: a k x k agent, anchored at its top-left cell
    bool one_way_walkable = false;      ///< sync(): OneWay cells walkable (a flyer's map) instead of blocked
    MemTag* tag = nullptr;              ///< default: the engine's "path" tag (Heap)
};

/// A desc mirroring `region` of `grid` (default: the whole grid; a negative w or h means "to the grid's edge") with
/// the grid's origin and cell size; the rest at its defaults. Pure.
PathMapDesc path_map_desc(const collision::Grid& grid, CellRect region = {0, 0, -1, -1});

/// Weights from the game's own data, called by sync() only, once per walkable grid cell (row-major), with grid
/// coordinates: return 1..255 or kPathBlocked. The documented pattern reads a per-tile-id table built from a
/// `path_cost` tile property (docs/api/pathfinding.md#weights-from-tiled).
struct PathWeights {
    int (*weight)(const void* context, int grid_x, int grid_y) = nullptr;
    const void* context = nullptr;
};

/// The searchable cells (FR-1..FR-6): one byte per cell, 0 = blocked, 1..255 = the weight of entering it. One block
/// of tagged memory; movable (also while searches run over it), not copyable. Cells (0, 0)..(width-1, height-1);
/// everything outside is blocked. Every sync/set_cell/fill/set_diagonals call increments version().
/// Memory: (width + 2) x (height + 2) bytes + a small header (+ width x height with an agent size > 1).
class PathMap {
public:
    /// InvalidArgument (sizes, costs, the 32-bit bound, agent size, cell size), OutOfMemory. A new map is all
    /// walkable, weight 1. Allocates (once).
    static Result<PathMap> create(const PathMapDesc& desc);

    PathMap() = default;
    PathMap(PathMap&& other) noexcept;
    PathMap& operator=(PathMap&& other) noexcept;
    PathMap(const PathMap&) = delete;
    PathMap& operator=(const PathMap&) = delete;
    /// Destroying a map while a sliced search or field build runs over it asserts; in Release that search ends
    /// with PathStatus::Invalid at its next step() (the memory is kept until then).
    ~PathMap();

    /// Reads every cell of the map from grid cell (grid_x + x, grid_y + y): Empty walkable (weight from `weights`,
    /// else 1), Solid blocked, OneWay blocked unless one_way_walkable; beyond the grid `grid.outside`. Load time:
    /// one grid read (and one weight call) per cell.
    void sync(const collision::Grid& grid, const PathWeights& weights = {});
    /// The same for the map cells of `region` only (a door that opened), clipped to the map.
    void sync(const collision::Grid& grid, CellRect region, const PathWeights& weights = {});
    /// Sets one cell: 1..255 or kPathBlocked (agent size > 1: the source cell; the footprint follows).
    void set_cell(int x, int y, int weight);
    /// Sets every cell of `region` (clipped to the map).
    void fill(CellRect region, int weight);
    /// Changes the neighbour mode of later searches and field builds.
    void set_diagonals(Diagonals diagonals);

    bool blocked(int x, int y) const;  ///< what searches see (after the footprint)
    int weight(int x, int y) const;    ///< 1..255, or kPathBlocked
    std::uint32_t version() const;     ///< +1 per change
    int width() const;
    int height() const;
    int agent_size() const;
    Diagonals diagonals() const;
    int straight_cost() const;
    int diagonal_cost() const;
    bool contains(CellPos cell) const;
    /// The centre of the agent's k x k footprint anchored at `cell`, in world pixels.
    Vec2 cell_center(CellPos cell) const;
    /// The anchor of an agent centred at `world` (spec 012's half-open cell rule); may be outside: see contains().
    /// A NaN or infinite point asserts; Release: {-1, -1}.
    CellPos cell_at(Vec2 world) const;
    bool valid() const; ///< false for a default-constructed or moved-from map

private:
    friend struct detail::PathAccess;
    detail::PathMapBlock* block_ = nullptr;
};

enum class PathStatus : std::uint8_t {
    None,         ///< no search yet (or cancelled)
    Running,      ///< a sliced search between begin() and the step() that finishes it
    Found,        ///< a path to `to`; its cost is the minimum (heuristic scale 16)
    Partial,      ///< options.partial: a path to the reached cell closest to `to`
    NoPath,       ///< `to` can't be reached (blocked, or every reachable cell expanded)
    LimitReached, ///< options.max_expanded cells expanded first
    Invalid,      ///< misuse (asserts in Debug/Development) or the map destroyed during a sliced search
};

struct PathOptions {
    int max_expanded = 0;     ///< cells taken from the open set (the goal's included); 0: the map's cell count
    bool partial = false;     ///< unreachable goal, blocked goal or the limit: a path to the closest reached cell
    int heuristic_scale = 16; ///< sixteenths, 16..64: cost <= scale/16 x the minimum, usually fewer expansions
};

struct PathResult {
    PathStatus status = PathStatus::None;
    bool stale = false;     ///< the map's version changed between begin() and the end
    std::uint32_t cost = 0; ///< sum of step x weight of the entered cells (the start's weight never counts)
    int cells = 0;          ///< path length, both ends included (0 without a path)
    int expanded = 0;       ///< cells taken from the open set
};

/// One search at a time over any map no larger than the searcher (FR-7..FR-14, FR-20). One block of tagged memory:
/// 8 bytes per padded cell ((width + 2) x (height + 2)) + 16 bytes per cell (the open set); nothing is cleared per
/// search. Movable, not copyable. Several searchers may search one map at once.
/// Order (fixed forever, FR-10): f = g + h, then lower h, then the most recent push; neighbours N, E, S, W, NE, SE,
/// SW, NW; a parent changes only on a strictly lower g. Integer-only: the same result on every system.
class PathSearcher {
public:
    /// For maps up to width x height (1..1024 each). InvalidArgument, OutOfMemory. Allocates (once).
    static Result<PathSearcher> create(int width, int height, MemTag* tag = nullptr);

    PathSearcher() = default;
    PathSearcher(PathSearcher&& other) noexcept;
    PathSearcher& operator=(PathSearcher&& other) noexcept;
    PathSearcher(const PathSearcher&) = delete;
    PathSearcher& operator=(const PathSearcher&) = delete;
    ~PathSearcher();

    /// begin() then step() until finished. Cost: bounded by options.max_expanded. No allocation.
    PathResult find(const PathMap& map, CellPos from, CellPos to, const PathOptions& options = {});
    /// Starts a sliced search (cancelling a running one). from == to: Found at once (1 cell, cost 0); a blocked
    /// goal without options.partial: NoPath at once; a blocked start is fine (only entered cells count).
    void begin(const PathMap& map, CellPos from, CellPos to, const PathOptions& options = {});
    /// Expands at most `max_expanded` (>= 0) cells; true when the search is finished (also when it already was).
    /// Any slice sizes give exactly find()'s result. A map edited meanwhile: the search goes on over the current
    /// cells and the result is flagged stale.
    bool step(int max_expanded);
    void cancel();        ///< result() becomes None
    bool running() const; ///< between begin() and the step() that finishes it
    PathResult result() const;

    /// The last search's path, start first: writes min(total, capacity) cells (the first ones), returns the total
    /// (null + 0 counts). Readable until the next begin()/find(). No path: 0.
    int path(CellPos* out, int capacity) const;
    /// The same as map.cell_center() points (`map`: the one searched).
    int path_points(const PathMap& map, Vec2* out, int capacity) const;

    bool reached(CellPos cell) const;          ///< given a cost by the last (or running) search
    bool expanded(CellPos cell) const;         ///< taken from the open set
    std::uint32_t cost_to(CellPos cell) const; ///< its g, kUnreachable when not reached
    int width() const;
    int height() const;
    bool valid() const;

private:
    friend struct detail::PathAccess;
    detail::PathSearchBlock* block_ = nullptr;
};

/// Waypoints of `path` (FR-15): the first and last cells and, after each kept cell, the last cell of the run of path
/// cells a straight centre-to-centre segment reaches crossing only walkable cells no heavier than the heaviest path
/// cell it replaces, under the map's corner rule. Same buffer contract as PathSearcher::path(); `out` may be `path`
/// (in place). Cost: bounded by the cells the segments cross. Safe for a point (or a box sliding with
/// collision::move); see docs/api/pathfinding.md#smoothing.
int smooth_path(const PathMap& map, const CellPos* path, int count, CellPos* out, int capacity);
/// `cells` as map.cell_center() points; same buffer contract.
int cells_to_points(const PathMap& map, const CellPos* cells, int count, Vec2* out, int capacity);

struct DistanceFieldDesc {
    int width = 0;               ///< the largest map it is built over, 1..1024
    int height = 0;              ///< 1..1024
    bool double_buffered = true; ///< queries answer from the last finished build while the next one runs (FR-17)
    MemTag* tag = nullptr;       ///< default: the engine's "path" tag
};

/// Dijkstra from goal cells over a map (FR-16..FR-18): every cell's minimum cost to reach the nearest goal (what a
/// search from that cell would cost), for crowds that share one goal. One block of tagged memory: 8 bytes per padded
/// cell (4 single-buffered) + 8 per cell (the open set). Movable, not copyable.
class DistanceField {
public:
    static Result<DistanceField> create(const DistanceFieldDesc& desc);

    DistanceField() = default;
    DistanceField(DistanceField&& other) noexcept;
    DistanceField& operator=(DistanceField&& other) noexcept;
    DistanceField(const DistanceField&) = delete;
    DistanceField& operator=(const DistanceField&) = delete;
    ~DistanceField();

    /// Builds in one call. Goals: map cells (outside: misuse; blocked: ignored; duplicates harmless). Cells costing
    /// more than `max_cost` stay unreachable, as do blocked cells.
    void build(const PathMap& map, const CellPos* goals, int count, std::uint32_t max_cost = kUnreachable);
    /// Starts a sliced build (cancelling a running one).
    void begin(const PathMap& map, const CellPos* goals, int count, std::uint32_t max_cost = kUnreachable);
    /// Settles at most `max_cells` (>= 0) cells; true when the build is finished (also when none runs).
    bool step(int max_cells);
    void cancel();
    bool building() const;
    bool ready() const;  ///< a finished build answers queries (single-buffered: false while building)
    bool stale() const;  ///< the answering build saw its map's version change
    int settled() const; ///< cells settled by the running (or last) build: progress

    /// kUnreachable: blocked, too far, not ready.
    std::uint32_t distance(CellPos cell) const;
    /// The neighbour an agent at `at` should step into (the one minimising step x weight + its distance, ties in
    /// neighbour order, under the map's diagonal rule); false at a goal, an unreachable cell or not ready.
    bool next(const PathMap& map, CellPos at, CellPos& out) const;
    /// next() through map.cell_at()/cell_center(): the point to walk toward.
    bool next_point(const PathMap& map, Vec2 at, Vec2& out) const;
    int width() const;
    int height() const;
    bool valid() const;

private:
    friend struct detail::PathAccess;
    detail::FieldBlock* block_ = nullptr;
};

/// Follows world waypoints the game owns (FR-19): + - x and compares on squared distances only (the same bits
/// everywhere). A few words; no allocation; never moves anything: the game turns the point into a velocity.
class PathFollower {
public:
    PathFollower() = default;
    PathFollower(const Vec2* points, int count, float arrival_radius);
    void reset(const Vec2* points, int count, float arrival_radius);
    /// Advances past every waypoint within the radius of `position` (the last one only arrives) and returns the
    /// point to walk toward (`position` itself for an empty list). Keep the radius >= the agent's speed per frame.
    Vec2 target(Vec2 position);
    bool arrived() const; ///< within the radius of the last point (or an empty list)
    int index() const;    ///< the waypoint walked toward

private:
    const Vec2* points_ = nullptr;
    int count_ = 0;
    int next_ = 0;
    float radius2_ = 0;
    bool arrived_ = true;
};

} // namespace nnh
