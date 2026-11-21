#pragma once
/// @file nnh/collision.hpp
/// Collision on plain values: box queries, and moving a box through a grid of solid cells the game
/// describes (an array, a function, or later a tilemap's collision grid). Depends only on <nnh/math.hpp>.
/// Nothing here allocates, keeps state or needs nnh::init(): every function is safe per frame.
/// See docs/api/collision.md.
///
/// Edges are half-open everywhere: a box covers [x, x + w) x [y, y + h), so boxes that only touch do not
/// overlap, and a box standing on a floor is flush with it, not inside it.

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

} // namespace nnh::collision
