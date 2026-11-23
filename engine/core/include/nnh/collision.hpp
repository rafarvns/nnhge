#pragma once
/// @file nnh/collision.hpp
/// Collision on plain values: box queries, and moving a box through a grid of solid cells the game
/// describes (an array, a function, or a map's Tilemap::collision_grid() from <nnh/tilemap.hpp>); layers
/// and masks, rays, box casts and point/box queries over the game's own arrays (spec 019). Depends only on
/// <nnh/math.hpp> and <cstdint>.
/// Nothing here allocates, keeps state or needs nnh::init(): every function is safe per frame.
/// See docs/api/collision.md.
///
/// Edges are half-open everywhere: a box covers [x, x + w) x [y, y + h), so boxes that only touch do not
/// overlap, and a box standing on a floor is flush with it, not inside it.

#include <cstdint>

#include "nnh/math.hpp"

namespace nnh::collision {

/// True when `a` and `b` share some area. Touching edges, and boxes with no width or height, never overlap.
bool overlaps(Rect a, Rect b);

/// True when `p` is inside `r`: `x <= p.x < x + w` and `y <= p.y < y + h`.
bool contains(Rect r, Vec2 p);

/// The area `a` and `b` share; `{}` (all zero) when they don't overlap.
Rect intersection(Rect a, Rect b);

/// The smallest move of `a` that ends its overlap with `b` (along x or y, whichever is shallower; ties
/// prefer x and, for equal centres, the negative direction). After the move the boxes touch. `{}` when
/// they don't overlap.
/// @code
/// const Vec2 push = collision::separation(player, crate);
/// player.x += push.x; player.y += push.y;
/// @endcode
Vec2 separation(Rect a, Rect b);

/// What a grid cell is to a moving box.
enum class Cell : unsigned char {
    Empty,  ///< nothing
    Solid,  ///< blocks from every side
    OneWay, ///< blocks only a box falling onto its top (a platform you jump up through)
};

/// A grid of cells the game describes; no ownership. Cell (0, 0)'s top-left corner is at `origin`; cell
/// (x, y) starts at `origin + (x * cell_w, y * cell_h)` and is `cell_w` x `cell_h` big.
/// Cells come from `cells` (row-major, `width * height`) when set, otherwise from `cell(context, x, y)`,
/// which is called only for 0 <= x < width, 0 <= y < height. Beyond the edges every cell is `outside`.
/// A grid with neither source is all `outside`. `cell_w` and `cell_h` must be > 0.
struct Grid {
    Vec2 origin{};
    float cell_w = 16;
    float cell_h = 16;
    int width = 0;               ///< cells
    int height = 0;              ///< cells
    Cell outside = Cell::Empty;  ///< beyond the edges (Solid walls the level in)
    const Cell* cells = nullptr; ///< row-major array, used when not null
    Cell (*cell)(const void* context, int x, int y) = nullptr; ///< used when `cells` is null
    const void* context = nullptr;                             ///< passed to `cell`
};

/// A grid over a row-major array the game owns (`width * height` cells, kept alive while the grid is used).
/// @code
/// static collision::Cell level[20 * 15];
/// const collision::Grid grid = collision::grid_view(level, 20, 15, 16, 16);
/// @endcode
Grid grid_view(const Cell* cells, int width, int height, float cell_w, float cell_h, Vec2 origin = {});

/// Sides of a box: which ones touched something.
struct Sides {
    bool left = false;
    bool right = false;
    bool ceiling = false;
    bool floor = false;
};

struct MoveOptions {
    bool drop_through = false; ///< ignore one-way cells (e.g. the player pressed down + jump)
};

struct MoveResult {
    Vec2 position;  ///< the box's new top-left corner
    Vec2 moved;     ///< distance travelled; equals `delta` on an axis that was not stopped
    Sides touching; ///< sides stopped by a cell during this move (a zero delta on an axis reports nothing)
};

/// Moves `box` by `delta` through `grid`: x first, then y (with the new x). Each axis stops flush at the
/// first blocking cell the box enters and the other axis still moves, so boxes slide along walls. Every
/// cell between start and target is checked, so nothing tunnels, whatever the speed.
/// - Cells the box already overlaps never block, so a box that starts inside solid can get out.
/// - One-way cells stop only a downward move whose bottom started at or above their top; never sideways or
///   upward; never with `options.drop_through`.
/// - A box with no width or height collides with nothing.
/// Cost: one cell lookup per cell crossed (runs of empty `outside` cells are skipped). No allocation.
/// Asserts (Debug/Development) on a NaN or infinite `delta` and on cell sizes <= 0; in Release such a delta
/// does not move and such a grid collides with nothing.
/// @code
/// const auto r = collision::move(grid, player, velocity * dt, {.drop_through = down && jump});
/// player.x = r.position.x; player.y = r.position.y;
/// if (r.touching.floor || r.touching.ceiling) velocity.y = 0;
/// @endcode
MoveResult move(const Grid& grid, Rect box, Vec2 delta, const MoveOptions& options = {});

/// Sides of `box` flush against blocking cells (within 1/64 px): what `move` would stop at if the box moved
/// a tiny step that way. `floor` also counts one-way cells under the box. Cells the box overlaps are
/// ignored, as in `move`. No allocation.
Sides contacts(const Grid& grid, Rect box);

/// A non-empty cell found by `cells_touching`.
struct CellHit {
    int x = 0; ///< cell column (may be outside the grid when `outside` is not Empty)
    int y = 0; ///< cell row
    Cell kind = Cell::Empty;
};

/// The non-empty cells `box` overlaps (shared area > 0: flush neighbours are not included, see `contacts`),
/// in row-major order. Writes up to `capacity` of them to `out` and returns how many there are in total
/// (may exceed `capacity`; `out` may be null with capacity 0 to count). Non-empty `outside` cells count too.
/// Cost: one lookup per overlapped cell (inside the grid only when `outside` is Empty). No allocation.
int cells_touching(const Grid& grid, Rect box, CellHit* out, int capacity);

// ---- Layers and masks (spec 019) ----

/// 32 independent layer bits. The engine names none: a game declares its own, for example
/// `enum GameLayers : collision::Layers { kWorld = collision::kDefaultLayer, kPlayer = collision::layer(1), ... };`
/// A thing's *layers* say what it is; a querier's *mask* says what it detects.
using Layers = std::uint32_t;
inline constexpr Layers kNoLayers = 0;           ///< on nothing: detected by nothing (a disabled entry)
inline constexpr Layers kAllLayers = 0xFFFFFFFFu; ///< the default mask: detects everything
inline constexpr Layers kDefaultLayer = 1u;      ///< bit 0: everything that had no layers before spec 019

namespace detail {
/// Out-of-range layer number: asserts (Debug/Development) and returns kNoLayers.
Layers bad_layer(int n);
} // namespace detail

/// The bit of layer `n` (0..31). Out of range: a compile error in a constant expression, an assert otherwise
/// (Release: kNoLayers).
constexpr Layers layer(int n) { return n >= 0 && n < 32 ? Layers{1} << n : detail::bad_layer(n); }

/// Godot's rule: something with mask `mask` detects something on `layers` when they share a bit. One-way: the
/// side that reacts asks. Box2D's symmetric rule is `detects(a.mask, b.layers) && detects(b.mask, a.layers)`.
constexpr bool detects(Layers mask, Layers layers) { return (mask & layers) != 0; }

// ---- Box views (FR-3) ----

/// The game's boxes where they are, never copied: box i's Rect is read at `first + i * stride` bytes and its layer
/// set at `layers + i * layers_stride` (layers null: every box is on all layers). Owns nothing; valid while the
/// game's array is. Queries report boxes by index in the view. Build one with box_view().
struct BoxView {
    const void* first = nullptr;
    int count = 0;
    int stride = static_cast<int>(sizeof(Rect));
    const void* layers = nullptr;
    int layers_stride = static_cast<int>(sizeof(Layers));
};

/// A view of a plain Rect array (every box on all layers).
inline BoxView box_view(const Rect* boxes, int count) { return {boxes, count}; }
/// A view of a Rect array with a parallel array of layer sets.
inline BoxView box_view(const Rect* boxes, const Layers* layers, int count) {
    return {boxes, count, static_cast<int>(sizeof(Rect)), layers};
}
/// A view of the game's own structs through member pointers: `box_view(enemies, n, &Enemy::box, &Enemy::layers)`.
/// `layers` may be omitted (all layers); its field may be the game's own 32-bit layer enum (read by its bits).
template <typename T, typename L = Layers>
BoxView box_view(const T* items, int count, Rect T::*box, L T::*layers = nullptr) {
    static_assert(sizeof(L) == sizeof(Layers), "a layer field must be 32 bits");
    if (items == nullptr) return {nullptr, count};
    return {&(items->*box), count, static_cast<int>(sizeof(T)), layers ? &(items->*layers) : nullptr,
            static_cast<int>(sizeof(T))};
}

// ---- Filter and results (FR-4, FR-8) ----

/// What a query looks at. The shortest call passes none: every box, no index ignored, starts inside ignored.
struct QueryFilter {
    Layers mask = kAllLayers;      ///< boxes: only those with detects(mask, layers). Grids: Tilemap::collision_grid(mask)
    int ignore = -1;               ///< a box index never reported (the shooter's own box); out of range: none
    bool hit_from_inside = false;  ///< rays, casts: report shapes containing the start (fraction 0, normal (0, 0))
    bool ignore_one_way = false;   ///< rays against grids: one-way cells never block (line of sight)
};

/// Where a ray (or a box cast) hit.
struct RayHit {
    float fraction = 0;  ///< along from → to, in [0, 1]
    float distance = 0;  ///< fraction × |to − from|
    Vec2 point{};        ///< entry point (casts: the box's top-left at contact); the crossed face's coordinate exact
    Vec2 normal{};       ///< unit axis vector pointing out of the shape ((−1, 0): its left face); (0, 0) when inside
    int box = -1;        ///< index in the view; −1: a cell
    CellHit cell{};      ///< the cell when box == −1 (x, y may be outside the grid when `outside` blocks)
    bool inside = false; ///< the shape contained the start (only with hit_from_inside)
};

/// A cell a segment passes through (cells_crossed).
struct CellCrossing {
    int x = 0;
    int y = 0;
    Cell kind = Cell::Empty;
    float fraction = 0; ///< where the segment enters the cell (0 for the start cell)
};

// ---- Rays (FR-8..FR-12) ----
//
// A ray is the segment from → to (no direction, no infinite rays; a zero-length ray hits nothing). Touching is
// not hitting: a segment along an edge or ending on a face hits nothing there. Shapes containing `from` are
// ignored unless filter.hit_from_inside. Every result is the same bit for bit on every system. No allocation.

/// The first cell of `grid` the segment hits. Cells are visited in order, every one of them (no tunnelling at
/// any length). A segment through an exact grid corner checks both cells beside it (no slipping between two
/// diagonal walls; such a hit reports the x face). One-way cells block only a ray entering their top face going
/// down (normal (0, −1)), never with filter.ignore_one_way. filter.mask is not used: filter a tilemap's grid with
/// Tilemap::collision_grid(mask). Cost: one cell lookup per cell visited; with `outside` Empty the segment is
/// clipped to the grid first.
/// @code
/// if (collision::RayHit hit; collision::raycast(grid, turret, player, hit)) wall = hit.point;
/// @endcode
bool raycast(const Grid& grid, Vec2 from, Vec2 to, RayHit& hit, const QueryFilter& filter = {});
/// The first box of the view whose interior the segment crosses (slab test; a ray through an exact corner reports
/// the x face). Boxes without area, on no layers (or not in filter.mask) and filter.ignore are never hit; ties at
/// one fraction go to the lowest index. Cost: one test per box.
bool raycast(const BoxView& boxes, Vec2 from, Vec2 to, RayHit& hit, const QueryFilter& filter = {});
/// The nearest hit of a grid and a view (Grid{} or an empty view: absent). A cell and a box at the same fraction:
/// the cell (a wall hides an enemy standing flush behind it).
bool raycast(const Grid& grid, const BoxView& boxes, Vec2 from, Vec2 to, RayHit& hit,
             const QueryFilter& filter = {});
/// Every blocking cell along the whole segment (a piercing laser), in order: writes up to `capacity` hits and
/// returns how many there are in total (may exceed `capacity`: the buffer holds the nearest; null + 0 counts).
int raycast_all(const Grid& grid, Vec2 from, Vec2 to, RayHit* hits, int capacity, const QueryFilter& filter = {});
/// Every box the segment crosses, sorted by fraction (ties: lowest index), same buffer contract.
int raycast_all(const BoxView& boxes, Vec2 from, Vec2 to, RayHit* hits, int capacity,
                const QueryFilter& filter = {});
/// Every cell the segment visits, blocking or not, in order, with the fraction where it enters each (fog of war,
/// per-tile lasers, debug drawing); same buffer contract. Clipped to the grid when `outside` is Empty; outside
/// cells are listed otherwise. A segment lying on a grid line crosses nothing.
int cells_crossed(const Grid& grid, Vec2 from, Vec2 to, CellCrossing* cells, int capacity);
/// True when nothing blocks from → to: no cell of `grid`, no box of the view (what contains `from` never counts;
/// filter.hit_from_inside is ignored). Exclude the viewer's own box with filter.ignore or by its layers.
bool line_of_sight(const Grid& grid, const BoxView& boxes, Vec2 from, Vec2 to, const QueryFilter& filter = {});
bool line_of_sight(const Grid& grid, Vec2 from, Vec2 to, const QueryFilter& filter = {});

// ---- Box casts (FR-13) ----

/// Moves `box` (its top-left corner) to `to` against the view and reports the first box it would touch: the
/// fraction, `point` = the box's top-left at contact (flush with the face: overlaps() is false there), the face's
/// normal, the index. Boxes it already overlaps are ignored unless filter.hit_from_inside. Two moving boxes A and B
/// meet this frame when `box_cast(view of B, A, A + (moveA − moveB))` hits. Box casts against grids: use move().
bool box_cast(const BoxView& boxes, Rect box, Vec2 to, RayHit& hit, const QueryFilter& filter = {});
/// Every box the cast would touch, sorted by fraction (ties: lowest index), same buffer contract as raycast_all.
int box_cast_all(const BoxView& boxes, Rect box, Vec2 to, RayHit* hits, int capacity, const QueryFilter& filter = {});

// ---- Point and box queries (FR-14, FR-15) ----

/// The boxes of the view containing `point` (half-open edges, as contains()) that pass `filter`, in ascending
/// index: writes up to `capacity` indices to `indices` and returns how many there are in total (may exceed
/// `capacity`; null with capacity 0 counts). Cost: one test per box. No allocation.
int boxes_at(const BoxView& boxes, Vec2 point, int* indices, int capacity, const QueryFilter& filter = {});
/// The boxes of the view sharing area with `box` (as overlaps()) that pass `filter`, ascending, same buffer
/// contract as boxes_at. `box` may be one of the view's: skip it with `filter.ignore`.
int boxes_overlapping(const BoxView& boxes, Rect box, int* indices, int capacity, const QueryFilter& filter = {});
/// The cell under `point` (spec 012's half-open rule), `outside` cells included beyond the edges.
CellHit cell_at(const Grid& grid, Vec2 point);

} // namespace nnh::collision
