# collision — boxes and grids

Header: `<nnh/collision.hpp>` · Types: [math.md](math.md) · Spec: [012](../../specs/012-tilemaps-and-collision/spec.md)
(FR-7, FR-8, FR-9)

Plain functions on values: overlap queries between boxes, and moving a box through a grid of solid cells **the game
describes**. Collision depends only on `<nnh/math.hpp>`: it needs no tilemap, no `nnh::init()`, keeps no state and
never allocates, so every function is safe to call any number of times per frame. Tilemaps (spec 012, later stages)
will hand out a `collision::Grid` of their own; until then (and always, if you prefer) a game passes its own array.

## Rules that hold everywhere

- **Half-open edges.** A box covers `[x, x + w) × [y, y + h)`. Boxes that only touch do not overlap, a point on the
  right or bottom edge is outside, and a box standing on a floor is flush with it, not inside it.
- **A box without width or height** overlaps nothing and collides with nothing.
- **Same result everywhere.** Only `+ − × ÷`, comparisons and floor/ceil on `float`, with no FP contraction: the
  simulator and the 3DS compute the same positions.

## Box queries

| Function | Returns |
|----------|---------|
| `bool overlaps(Rect a, Rect b)` | the boxes share some area (touching edges don't count) |
| `bool contains(Rect r, Vec2 p)` | `x <= p.x < x + w` and `y <= p.y < y + h` |
| `Rect intersection(Rect a, Rect b)` | the shared area; `{}` when they don't overlap |
| `Vec2 separation(Rect a, Rect b)` | the smallest move of `a` that ends the overlap, along the shallower axis, pointing away from `b`; after it the boxes touch. Ties prefer x, and equal centres push towards negative. `{}` when they don't overlap |

```cpp
if (collision::overlaps(player, coin)) collect(coin);
const Vec2 push = collision::separation(player, crate);   // push the player out of a crate
player.x += push.x;
player.y += push.y;
```

## Grids

```cpp
enum class Cell : unsigned char { Empty, Solid, OneWay };   // OneWay: solid only from above

struct Grid {
    Vec2 origin{};                    // world position of cell (0, 0)'s top-left corner
    float cell_w = 16, cell_h = 16;   // > 0, need not be square
    int width = 0, height = 0;        // in cells
    Cell outside = Cell::Empty;       // every cell beyond the edges (Solid walls the level in)
    const Cell* cells = nullptr;      // row-major width * height, used when set...
    Cell (*cell)(const void* context, int x, int y) = nullptr;   // ...otherwise asked per cell
    const void* context = nullptr;
};
Grid grid_view(const Cell* cells, int width, int height, float cell_w, float cell_h, Vec2 origin = {});
```

A `Grid` owns nothing: it points at cells the game keeps alive. Two ways to describe them:

- **An array**: `grid_view(cells, w, h, cell_w, cell_h, origin)` (row-major, cell `(x, y)` is `cells[y * w + x]`).
- **A function**: set `cell` and `context`; it is called only for cells inside the grid, so it needs no bounds
  checks. Useful when the game stores its level differently (bits, a tile array of its own, several layers).

Cell `(x, y)` covers `[origin.x + x·cell_w, origin.x + (x+1)·cell_w) × [origin.y + y·cell_h, …)`. The origin may be
negative or fractional. A grid with zero size, or with neither `cells` nor `cell`, is all `outside`.

## Moving a box

```cpp
struct Sides { bool left, right, ceiling, floor; };
struct MoveOptions { bool drop_through = false; };      // ignore one-way cells
struct MoveResult { Vec2 position; Vec2 moved; Sides touching; };

MoveResult move(const Grid& grid, Rect box, Vec2 delta, const MoveOptions& options = {});
Sides contacts(const Grid& grid, Rect box);
struct CellHit { int x, y; Cell kind; };
int cells_touching(const Grid& grid, Rect box, CellHit* out, int capacity);
```

**`move`** moves `box` by `delta`, **x first, then y** (with the new x):

- Each axis stops **flush** at the first blocking cell the box enters, and the other axis still moves: boxes slide
  along walls and floors. Moving diagonally into a lone corner cell, x passes (the cell isn't in the box's rows yet)
  and y lands on it.
- **Every cell line between start and target is checked**, so a fast box never passes through a wall, however
  large the delta (10 000 px is fine). Runs of empty `outside` cells are skipped, so huge deltas stay cheap.
- **Cells the box already overlaps never block.** A box that starts inside solid can get out; it is stopped only by
  solid cells it newly enters.
- **One-way cells** stop only a **downward** move whose bottom **started at or above their top**. They never block
  sideways or upward moves (you jump up through them), and a box that is partly inside one falls on through.
  `options.drop_through` ignores them entirely (down + jump to drop off a plank).
- `position` is the new top-left corner; `moved` is the distance travelled (exactly `delta` on an axis that was not
  stopped); `touching` lists the sides stopped **during this move**. An axis with a zero delta reports nothing:
  use `contacts` for "am I standing on something" without moving.
- After a stop the position is snapped so the blocking cell stays outside the box even when float sums round the
  wrong way, so a box resting on a floor keeps resting on it frame after frame.
- **Misuse**: a NaN or infinite `delta`, or cell sizes `<= 0`, assert in Debug/Development; in Release such a delta
  does not move and such a grid collides with nothing.

**`contacts`** reports the sides of `box` that are flush (within 1/64 px) against something `move` would stop at:
the same as moving a tiny step each way. `floor` also counts one-way cells under the box; cells the box overlaps are
ignored, as in `move`.

**`cells_touching`** lists the non-empty cells the box **overlaps** (shared area > 0; flush neighbours are not
included) in row-major order. It writes up to `capacity` hits and returns the total, which may be larger; pass
`nullptr, 0` to count only. Non-empty `outside` cells count too (their `x`/`y` lie outside the grid). Use it for
spikes, water, ladders: cells the game gives meaning to.

## Example: platformer movement with the game's own level

```cpp
#include <nnh/collision.hpp>
#include <nnh/input.hpp>

namespace collision = nnh::collision;
using collision::Cell;

constexpr int kW = 20, kH = 15;
Cell level[kW * kH];                          // filled by the game: Solid ground, OneWay planks
const collision::Grid grid = [] {
    collision::Grid g = collision::grid_view(level, kW, kH, 16, 16);
    g.outside = Cell::Solid;                  // the screen edges are walls
    return g;
}();

nnh::Rect player{40, 40, 12, 14};
nnh::Vec2 velocity{};
bool on_ground = false;

void update_player(float dt) {
    velocity.x = (nnh::input::held(nnh::Button::Right) - nnh::input::held(nnh::Button::Left)) * 90.0f;
    velocity.y += 600.0f * dt;                                   // gravity
    const bool down = nnh::input::held(nnh::Button::Down);
    if (on_ground && nnh::input::pressed(nnh::Button::A) && !down) velocity.y = -260.0f;

    const auto r = collision::move(grid, player, velocity * dt,
                                   {.drop_through = on_ground && down && nnh::input::pressed(nnh::Button::A)});
    player.x = r.position.x;
    player.y = r.position.y;
    if (r.touching.floor || r.touching.ceiling) velocity.y = 0;
    on_ground = r.touching.floor;
}
```

Check the [input reference](input.md) for the exact button queries. `examples/platformer` (spec 012, later stage)
does the same against a tilemap's grid.

## Limits and costs

| | |
|---|---|
| Allocation | none, ever |
| `move` | one cell lookup per cell crossed (cells in the lines entered × the box's span on the other axis) |
| `contacts` | four tiny `move` probes |
| `cells_touching` | one lookup per overlapped cell (only cells inside the grid when `outside` is Empty) |
| Coordinates | `float`; cell indices saturate at ±2^30. `contacts`' 1/64 px tolerance stays above float spacing for worlds up to 131 072 px |
