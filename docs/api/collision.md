# collision — boxes, grids, layers and queries

Header: `<nnh/collision.hpp>` · Types: [math.md](math.md) · Specs: [012](../../specs/012-tilemaps-and-collision/spec.md)
(FR-7, FR-8, FR-9), [019](../../specs/019-collision-layers-and-queries/spec.md) (layers and queries) · Example:
`examples/queries`

Plain functions on values: overlap queries between boxes, moving a box through a grid of solid cells **the game
describes**, and (spec 019) **layers and masks**, **rays**, **box casts** and **point and box queries** over the
game's own arrays. Collision depends only on `<nnh/math.hpp>` (and `<cstdint>`): it needs no tilemap, no
`nnh::init()`, keeps no state and never allocates, so every function is safe to call any number of times per frame.
A tilemap hands out a `collision::Grid` of its own (`Tilemap::collision_grid()`, [tilemap.md](tilemap.md)); a game
can always pass its own array instead. There is no physics world: the game owns its boxes and asks questions.

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

Check the [input reference](input.md) for the exact button queries. `examples/platformer` does the same against a
tilemap's grid (`player.cpp`, tested in `tests/platformer/test_player.cpp`) and picks up coins with a layered
`boxes_overlapping` query (`coin_book.cpp`, below).

## Layers and masks

```cpp
using Layers = std::uint32_t;                 // 32 independent bits
inline constexpr Layers kNoLayers = 0, kAllLayers = 0xFFFFFFFF, kDefaultLayer = 1;
constexpr Layers layer(int n);                // the bit of layer n (0..31)
constexpr bool detects(Layers mask, Layers layers) { return (mask & layers) != 0; }
```

A thing's **layers** say what it is; a querier's **mask** says what it detects. The rule is Godot's: **A detects B
when A's mask shares a bit with B's layers**. It is one-way: the side that reacts asks (the player collects a coin if
the player's mask has the coin layer). Box2D's symmetric rule is one line of game code:
`detects(a.mask, b.layers) && detects(b.mask, a.layers)`.

The engine names no layer. Declare your own, once:

```cpp
namespace layers {
using nnh::collision::Layers;
inline constexpr Layers kWorld = nnh::collision::kDefaultLayer;   // tiles are on the default layer
inline constexpr Layers kPlayer = nnh::collision::layer(1);
inline constexpr Layers kCoin = nnh::collision::layer(2);
inline constexpr Layers kEnemyBullet = nnh::collision::layer(4);
inline constexpr Layers kPlayerMask = kWorld | kCoin;
} // namespace layers
```

- `layer(n)` out of 0..31 is a compile error in a constant expression and an assert otherwise (Release: `kNoLayers`).
- Everything that had no layers before is on **`kDefaultLayer`** (bit 0): tilemaps' tiles and layers, boxes in a view
  without layers (those are on **all** layers, so any mask with a bit detects them).
- A box on **`kNoLayers`** is detected by nothing: the cheap way to disable an entry of an array (a taken coin). A mask
  of `kNoLayers` detects nothing.
- Grids have no layers (a `Grid` the game describes is one purpose: describe one grid per purpose, or read your own
  layer data in its callback). A tilemap's cells do: `Tilemap::collision_grid(mask)` hands out a grid already
  filtered by a mask ([tilemap.md](tilemap.md#collision-layers)).

## Box views

Queries over boxes read the game's own memory through a **view**, never a copy:

```cpp
struct BoxView { const void* first; int count; int stride; const void* layers; int layers_stride; };
BoxView box_view(const Rect* boxes, int count);                        // every box on all layers
BoxView box_view(const Rect* boxes, const Layers* layers, int count);  // a parallel array of layer sets
template <typename T, typename L> BoxView box_view(const T* items, int count, Rect T::*box, L T::*layers = nullptr);
```

```cpp
struct Enemy { nnh::Rect box; nnh::collision::Layers layers; int hp; };
Enemy enemies[32];
const auto view = collision::box_view(enemies, enemy_count, &Enemy::box, &Enemy::layers);
```

A view is a few words, owns nothing and is valid while the game's array is; results name boxes by **index in the
view**. The layer field may be the game's own 32-bit layer enum (it is read by its bits). For any other layout, fill a
`BoxView` by hand: box `i`'s `Rect` is read at `first + i·stride` bytes and its layers at `layers + i·layers_stride`
(`layers` null: all layers).

Every query takes an optional **filter**:

```cpp
struct QueryFilter {
    Layers mask = kAllLayers;      // boxes: only those detects(mask, layers)
    int ignore = -1;               // a box index never reported (the shooter's own box); out of range: none
    bool hit_from_inside = false;  // rays and casts: report shapes containing the start
    bool ignore_one_way = false;   // rays against grids: one-way cells never block (line of sight)
};
```

Boxes without width or height, with non-finite fields, on no layers, outside the mask or at the ignored index are
never reported. The mask filters **boxes**; to filter a tilemap's cells use `Tilemap::collision_grid(mask)`.

## Point and box queries

| Function | Returns |
|----------|---------|
| `int boxes_at(const BoxView&, Vec2 point, int* indices, int capacity, const QueryFilter& = {})` | the boxes containing the point (half-open, as `contains`), in ascending index |
| `int boxes_overlapping(const BoxView&, Rect box, int* indices, int capacity, const QueryFilter& = {})` | the boxes sharing area with `box` (as `overlaps`), ascending; `box` may be one of the view's: skip it with `ignore` |
| `CellHit cell_at(const Grid&, Vec2 point)` | the cell under the point (spec 012's half-open rule), `outside` cells included |

The buffer contract is `cells_touching`'s: up to `capacity` indices are written and the **total** is returned (it may
be larger); `nullptr, 0` counts. Over a grid, `cells_touching` is the box query (on a filtered grid it is layered too).

```cpp
// "What is under this touch": the touch becomes a world point (camera position + touch), then:
const collision::CellHit cell = collision::cell_at(map.collision_grid(), world);
int hits[8];
const int n = collision::boxes_at(view, world, hits, 8, {.mask = kPickable});
```

`examples/queries` shows it on the bottom screen: the room through a `Camera2D` at (40, 0), a touch at screen
`(x, y)` is world `(40 + x, y)`, and the HUD lists the cell, its kind and layers (`Tilemap::collision_layers_at`), the
tile and its properties, and the boxes under the point.

## Rays

```cpp
struct RayHit {
    float fraction;   // along from → to, [0, 1]
    float distance;   // fraction × |to − from|
    Vec2 point;       // where the segment enters the shape; the crossed face's coordinate exact
    Vec2 normal;      // unit axis vector out of the shape ((−1, 0): its left face); (0, 0) when inside
    int box;          // index in the view; −1: a cell
    CellHit cell;     // the cell when box == −1
    bool inside;      // the shape contained the start (only with hit_from_inside)
};
bool raycast(const Grid&, Vec2 from, Vec2 to, RayHit& hit, const QueryFilter& = {});
bool raycast(const BoxView&, Vec2 from, Vec2 to, RayHit& hit, const QueryFilter& = {});
bool raycast(const Grid&, const BoxView&, Vec2 from, Vec2 to, RayHit& hit, const QueryFilter& = {});
int raycast_all(const Grid&, Vec2 from, Vec2 to, RayHit* hits, int capacity, const QueryFilter& = {});
int raycast_all(const BoxView&, Vec2 from, Vec2 to, RayHit* hits, int capacity, const QueryFilter& = {});
int cells_crossed(const Grid&, Vec2 from, Vec2 to, CellCrossing* cells, int capacity);
bool line_of_sight(const Grid&, const BoxView&, Vec2 from, Vec2 to, const QueryFilter& = {});
bool line_of_sight(const Grid&, Vec2 from, Vec2 to, const QueryFilter& = {});
```

```cpp
if (collision::RayHit hit; collision::raycast(grid, enemies_view, turret, player, hit, {.mask = kEnemyMask})) {
    laser_end = hit.point;                         // flush on the wall or the enemy it hit
    if (hit.box >= 0) damage(enemies[hit.box]);
}
```

A ray is the **segment** from → to (Godot's target position): no direction to normalise, no infinite rays (a long ray
is a far end point), and a zero-length ray hits nothing (use `cell_at`/`boxes_at` for point questions). An absent grid
is `collision::Grid{}`, an absent view `collision::BoxView{}` (a bare `{}` is ambiguous between the overloads).

**Against a grid** the cells are visited **in order, every one of them** (Amanatides & Woo's traversal), so no wall
is skipped at any length (10 000 px is fine) and a fast bullet always finds a thin wall:

- **Touching is not hitting.** A segment running along a cell edge, or ending exactly on a face, does not hit that
  cell; a segment lying on a grid line crosses nothing.
- **Corners never leak.** A segment through an exact grid corner checks **both cells beside it** (the x-neighbour,
  then the y-neighbour, then the diagonal, all at that fraction), so line of sight never squeezes between two
  diagonal walls; such a hit reports the corner as its point and the **x face** (ties prefer x, as `move`):

  ```
      +----+----+          the ray from ░ to ▒ passes the corner *;
      | ## | ▒  |          the solid cell beside it (##) is the hit,
      +----*----+          at the corner, with the x face's normal.
      | ░  | ## |
      +----+----+
  ```
- **One-way cells** block only a ray entering their **top face going down** (normal (0, −1)), also through a corner;
  `ignore_one_way` makes them transparent (line of sight usually wants that).
- **A start inside a solid cell** is ignored unless `hit_from_inside` (then: fraction 0, `point = from`, normal
  (0, 0), `inside`); a start inside a one-way cell never blocks. A start flush on a face going in is a hit at
  fraction 0 with that face's normal.
- With `outside` Empty the segment is first clipped to the grid, so a ray from far away costs only the cells inside.

**Against boxes** each box passing the filter gets a slab test: it is hit when the segment crosses its **interior**
(touching an edge or a corner is not hitting); the hit is the entry point with that face's normal, the x face when
entering exactly through a corner. Boxes containing the start are ignored unless `hit_from_inside` (as Godot and
Box2D; Unity's default is the opposite). Note: two boxes touching only at a corner don't stop a ray through that exact
point, while two diagonal grid cells do; overlap boxes by a pixel to seal a corner.

**First hit and all hits.** `raycast` returns the nearest hit: ties at one fraction go to the lowest box index or the
first cell in walk order; over a grid **and** boxes the **cell** wins a tie (a wall hides an enemy standing flush
behind it). `raycast_all` writes every hit along the whole segment (a piercing laser), sorted by fraction, keeping the
nearest `capacity` and returning the total. **`cells_crossed`** is the walk itself: every cell visited, blocking or
not, in order, with the fraction where the segment enters it (fog of war, lasers drawn per tile, debug drawing),
clipped to the grid when `outside` is Empty and listing outside cells otherwise. **`line_of_sight`** is true when
nothing blocks from → to; what contains `from` never counts (exclude the viewer's own box with `ignore` or its layers).

## Box casts

```cpp
bool box_cast(const BoxView&, Rect box, Vec2 to, RayHit& hit, const QueryFilter& = {});
int box_cast_all(const BoxView&, Rect box, Vec2 to, RayHit* hits, int capacity, const QueryFilter& = {});
```

A box cast moves `box` (its top-left corner) to `to` against the view: the fraction, `point` = the box's top-left **at
contact** (flush: `overlaps` is false there and true one float step further), the face's normal and the index. Rules
as rays: touching is not hitting, sliding along a face is not a hit, boxes already overlapped at the start are ignored
unless `hit_from_inside`. **Two moving boxes** A and B met this frame when the cast of A by its motion **relative** to
B hits B: `box_cast(view of B, A, {A.x + moveA.x − moveB.x, A.y + moveA.y − moveB.y}, hit)`. Box casts against grids
are not provided: `move` moves boxes through grids without tunnelling, and rays cover thin projectiles.

## Determinism

Every query gives the same bits on every system (PC compilers and the 3DS): decisions compare exact ratios (cross
products in `double` of differences of the float inputs; exact for coordinates on a 1/256 px lattice within ±2^17 px,
plain IEEE arithmetic beyond), results are rounded once to `float`, nothing is fused (`-ffp-contract=off`), and the
distance uses the IEEE square root, correctly rounded by the standard (one instruction, no `errno`). Inputs of
subnormal size are not guaranteed identical (the 3DS flushes them). `examples/queries` checks it with a self-check of
4096 rays and 512 box casts whose checksum must be the simulator's on the console.

## Misuse

NaN or infinite points or boxes, a negative count or capacity, a null buffer with capacity > 0, a null `first` with a
count, a stride smaller than a `Rect` (or a layer stride smaller than 4) with boxes, cell sizes ≤ 0: assert in
Debug/Development; in Release the query reads nothing and returns no hits (false, 0, an Empty cell at (0, 0));
`line_of_sight` returns false (it never claims a clear view it couldn't check). No log in the normal path.

## Limits and costs

| | |
|---|---|
| Allocation | none, ever (also for the queries: no buffers but the caller's) |
| `move` | one cell lookup per cell crossed (cells in the lines entered × the box's span on the other axis) |
| `contacts` | four tiny `move` probes |
| `cells_touching` | one lookup per overlapped cell (only cells inside the grid when `outside` is Empty) |
| Rays on a grid | one cell lookup per cell visited (inside the grid only when `outside` can't block) |
| Rays, casts, point queries on boxes | one test per box in the view (no broadphase: pass a bucket's view if you keep buckets) |
| Measured (PC simulator, Debug) | 128 first-hit rays of 0–400 px against a 25 × 15 filtered grid + 64 boxes: about 1 ms per frame; point queries 0.05 µs per box tested. Old 3DS: pending (spec 019 stage 10; targets < 2 ms and ≤ 0.1 µs per box) |
| Coordinates | `float`; cell indices saturate at ±2^30. `contacts`' 1/64 px tolerance stays above float spacing for worlds up to 131 072 px |
