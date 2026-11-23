# Plan 019: Collision layers and queries

Status: approved (2026-11-22, maintainer pre-approved); **implemented 2026-11-23** (stages 0–8, 11), console T10.2 and
Linux T9.1 pending
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decision D34 ([decisions.md](../../docs/architecture/decisions.md))
Uses spec 002 (asserts, logs, profiler), spec 012 (`<nnh/collision.hpp>`: `Rect` rules, `Grid`, `Cell`, `move`,
`cells_touching`; `<nnh/tilemap.hpp>`: the map block, `collision_grid()`, properties, the Tiled converter, D26/D27),
spec 013 (scripts), spec 015 (determinism rules: `-ffp-contract=off`, the 3DS FPSCR), spec 018 (the platformer's
two-step adoption and golden-line procedure; the bottom-screen conversion shown in the example)

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ⚠️→✅ | Pressure point: rays must give the same bits on PC and 3DS. All of it is portable core code with no platform part. Decisions (order, corners, ties, touching) use cross products in IEEE `double` from the float inputs, outputs are rounded once to `float`; `-ffp-contract=off` on every new file; the distance uses the IEEE square root only (`-fno-math-errno`, checked to be one instruction) — the spec's one bend of "no libm" (FR-16). Golden checksum on every compiler and on the console (AC-2, AC-6) |
| II 3DS constraint | ✅ | No memory but the caller's buffers; a layered map costs 132 B + 4 B per tile id; costs bounded by cells visited and boxes in the view, estimated below and measured on an Old 3DS (AC-7). One AC-7 target is unreachable as written and is recast per box (decision 17) |
| III Build speed | ⚠️→✅ | `<nnh/collision.hpp>` gains `<cstdint>` and one tiny member-pointer template; no `<cmath>`, no `<cstring>` (box reads use `memcpy` inside the `.cpp` files). Preprocessed lines and compile time measured before/after (T0.1, T11.4); `hello` doesn't include it |
| IV Event-driven | ✅ | Queries only, nothing published (spec non-goal); enter/exit is the game's comparison of two frames |
| V No hidden costs | ✅ | No allocation anywhere (not even the first call), no `virtual`, no RTTI, no exceptions, no state; one indirect call per grid cell (the existing `Grid` callback), none per box |
| VI Spec + test first | ✅ | Pure logic tested first on plain values; the traversal is checked cell for cell against an exact integer reference and by sampling; `collision_grid()` is pinned by a regression test **before** the tilemap changes (T0.2); the platformer swaps first with unchanged golden lines (stage 6) |
| VII Small files | ✅ | Fourteen collision files and four tilemap files of one concept each (all planned ≤ 200 lines); still one public header per module (collision grows to ≈ 300 lines with comments, as `camera.hpp` did) |
| VIII Documented API | ✅ | `docs/api/collision.md`, `tilemap.md`, the levels guide, header comments, `examples/queries`, the platformer |
| IX Clean and legal | ✅ | Own code from Amanatides & Woo's paper and the textbook slab method; Godot/Unity/Box2D/Tiled cited for ideas only |
| X Safe by default | ✅ | The converter refuses a bad `collision_layers` with `file: reason`; the loader treats one as absent (warning) and stays fuzzed; query misuse asserts in Debug/Development and returns no hits in Release |
| XI Opt-in, never in the way | ⚠️→✅ | Pressure point: FR-6 touches the hot lookup of `move` through a map. Answered by D34: `Grid` unchanged, `collision_grid()` returns today's grid with today's callback (its source file is not edited), filtered grids are a separate overload and file; query code is linked only when called |

## Design

### Overview

```
game data (its own arrays)                       engine (pure functions, no state)
  Enemy { Rect box; Layers layers; ... }[n]
  collision::box_view(enemies, n, &Enemy::box, &Enemy::layers)   → BoxView {first, count, stride, layers, stride}
  map.collision_grid(kPlayerMask)                → Grid {cell = filtered callback, context = the mask's slot}

  raycast(grid, boxes, from, to, hit, filter)    → grid walk (Amanatides & Woo, exact) ─┐ nearest; cell wins ties
                                                   slab test per box passing the filter ─┘
  raycast_all / cells_crossed / box_cast[_all]   → the same walks, hits into the caller's buffer (sorted, total)
  boxes_at / boxes_overlapping / cell_at         → one scan of the view / one cell
```

### Decision D34: layers beside the map, filtered grids by context, strided views, exact predicates

**`Grid` is not changed.** Its callback is `Cell (*)(const void* context, int x, int y)`; it can't see a mask unless the
signature changes (every game's callback breaks) or `Grid` grows a second callback and every `cell_at` takes one more
branch. Instead the tilemap hands out a **context** that carries the mask:

- A map block keeps its `.nnhmap` bytes (version 1, `header.file_size` bytes, unchanged) and gains a **runtime area**
  right after them (4-aligned, never in a file):

  ```cpp
  // tilemap/map_runtime.hpp (engine only, not shared with nnh-assetc)
  struct GridMaskSlot {
      std::uint32_t mask;         ///< kAllLayers in an unused slot
      std::uint32_t block_offset; ///< bytes from the block's start to this slot: the callback finds the map
  };
  struct MapRuntime {                              // at block + header.file_size
      std::uint32_t layer_sets[kMaxTileLayers];   // 64 B: collision layer set of each tile layer
      GridMaskSlot grid_masks[kMaxGridMasks];      // 64 B: kMaxGridMasks = 8
      std::uint32_t flags;                         // 4 B: bit 0 = "out of grid masks" already logged
      // followed by std::uint32_t tile_sets[tile_ids_cap + 1]: collision layer set of each tile id
  };
  ```

  `detail::map_block_bytes(caps) = map_layout(caps).total + 132 + 4 × (tile_ids + 1)` replaces `map_layout(caps).total`
  in `Tilemap::create` and `tilemap_from_file`; `detail::init_map_runtime(block)` sets every set to `kDefaultLayer` and
  every slot unused. `copy_map`, `init_map_block`, `map_layout` and the validator (shared with `nnh-assetc`) are not
  changed, so the converter and the format know nothing of it.
- **`collision_grid()`** (no argument): today's function, today's file (`map_collision.cpp` is **not edited**), today's
  callback `grid_cell` with `context = block_`. Same struct, same code, same cost — by construction, and checked by
  comparing the disassembly of `map_collision.o` before and after (T4.6) and by the pinned regression test (T0.2).
- **`collision_grid(mask)`** (new overload, its own file): `mask == kAllLayers` → returns `collision_grid()`. Otherwise
  it finds the slot holding `mask` or takes the first unused one (≤ 8 compares; a slot is never reused, so a grid
  handed out earlier stays valid) and returns a grid with `cell = grid_cell_masked`, `context = &slot`. The callback
  recovers the block (`slot − block_offset`) and runs the masked cell rule:

  ```
  for each layer in header.collision_layers (as today):
      id = cell's tile id; skip 0
      if layer has kLayerCollision and (layer_sets[layer] & mask) → Solid
      kind = tile flags[id]; if kind != Empty and (tile_sets[id] & mask) == 0 → this contribution doesn't count
      Solid → Solid; OneWay → remember OneWay
  → OneWay or Empty
  ```

  One more mask test per contribution, as FR-6 asks. With all contributions on the default layer and a mask holding
  bit 0 it gives `collision_grid()`'s cells exactly (tested on random maps, T4.3, and in a real game: stage 6).
  Writing the slot from a `const` method is a cache write through the block pointer (single-threaded engine,
  documented: "the first call with a new mask registers it in the map; a map holds 8 masks").
- **Out of slots** (a 9th distinct mask): assert in Debug/Development; Release logs one error per map (`tilemap`
  channel, the `flags` bit) and returns the unfiltered grid — walls stay walls rather than vanish.

**Strided views, not copies.** A `BoxView` is the address of box 0's `Rect`, a count, a byte stride and optionally the
address and stride of box 0's layer set. Box `i` is read with `memcpy` (no aliasing assumption about the game's struct;
compiles to plain loads). Helpers build it from a `Rect` array, a `Rect` array + parallel `Layers` array, and an array
of structs through member pointers (`&(items->*box)`, stride `sizeof(T)`; no `offsetof`).

**Exact predicates.** Every decision the spec makes about order and touching — which boundary a ray crosses first, a
corner, entering at fraction 0, a box's entry face, sorting ties — compares ratios `a/b` against `c/d` as cross
products `a·d` vs `c·b` in `double`, where `a`, `c` are differences of two (or three) float coordinates and `b`, `d`
the ray's `|dx|`, `|dy|`. For coordinates that are multiples of 1/256 px within ±2^17 px, each difference fits 27 bits
and each product 53 bits, so the comparison is **exact**; beyond that it is still IEEE-deterministic. Reported values
are computed once and rounded once to `float` (below). A float DDA accumulating `tMax += tDelta` was rejected: its
corner decisions drift with the ray's length, and no reference could check it cell for cell.

### Public API (sketch)

```cpp
// <nnh/collision.hpp> (changed): includes <cstdint> and "nnh/math.hpp"
namespace nnh::collision {

// ---- Layers (FR-1, FR-2) ----
/// 32 independent layer bits. The engine names none: declare your own,
/// `enum : collision::Layers { kWorld = collision::kDefaultLayer, kPlayer = collision::layer(1), ... };`
using Layers = std::uint32_t;
inline constexpr Layers kNoLayers = 0;              ///< on nothing: detected by nothing (a disabled entry)
inline constexpr Layers kAllLayers = 0xFFFFFFFFu;
inline constexpr Layers kDefaultLayer = 1u;         ///< bit 0: everything that had no layers before
namespace detail { Layers bad_layer(int n); }       ///< asserts (Debug/Development); kNoLayers
/// The bit of layer `n` (0..31). Out of range: a compile error in a constant expression, an assert otherwise.
constexpr Layers layer(int n) { return n >= 0 && n < 32 ? Layers{1} << n : detail::bad_layer(n); }
/// Godot's rule: A detects B when A's mask shares a bit with B's layers. Symmetric (Box2D):
/// `detects(a.mask, b.layers) && detects(b.mask, a.layers)`.
constexpr bool detects(Layers mask, Layers layers) { return (mask & layers) != 0; }

// ---- Box views (FR-3) ----
/// The game's boxes where they are: box i's Rect at `first + i * stride` bytes, its layer set at
/// `layers + i * layers_stride` (null: every box on all layers). Owns nothing; valid while the array is.
struct BoxView {
    const void* first = nullptr;
    int count = 0;
    int stride = static_cast<int>(sizeof(Rect));
    const void* layers = nullptr;
    int layers_stride = static_cast<int>(sizeof(Layers));
};
inline BoxView box_view(const Rect* boxes, int count) { return {boxes, count}; }
inline BoxView box_view(const Rect* boxes, const Layers* layers, int count) {
    return {boxes, count, static_cast<int>(sizeof(Rect)), layers};
}
/// `box_view(enemies, n, &Enemy::box, &Enemy::layers)`. `layers` may be null (all layers); its type may be the
/// game's own 32-bit layer enum (read by its bits).
template <typename T, typename L = Layers>
BoxView box_view(const T* items, int count, Rect T::*box, L T::*layers = nullptr) {
    static_assert(sizeof(L) == sizeof(Layers), "a layer field must be 32 bits");
    if (items == nullptr) return {nullptr, count};
    return {&(items->*box), count, static_cast<int>(sizeof(T)), layers ? &(items->*layers) : nullptr,
            static_cast<int>(sizeof(T))};
}

// ---- Filter and results (FR-4, FR-8) ----
struct QueryFilter {
    Layers mask = kAllLayers;      ///< boxes: only those detects(mask, layers). Grids: use Tilemap::collision_grid(mask)
    int ignore = -1;               ///< a box index never reported (the shooter's own box)
    bool hit_from_inside = false;  ///< rays, casts: report shapes containing the start (fraction 0, normal (0, 0))
    bool ignore_one_way = false;   ///< rays against grids: one-way cells never block (line of sight)
};
struct RayHit {
    float fraction = 0;  ///< along from → to, [0, 1]
    float distance = 0;  ///< fraction × |to − from|
    Vec2 point{};        ///< entry point (casts: the box's top-left at contact); the crossed face's coordinate exact
    Vec2 normal{};       ///< unit axis vector out of the shape; (0, 0) when inside
    int box = -1;        ///< index in the view; -1: a cell
    CellHit cell{};      ///< the cell when box == -1
    bool inside = false; ///< the shape contained the start (only with hit_from_inside)
};
struct CellCrossing {
    int x = 0, y = 0;
    Cell kind = Cell::Empty;
    float fraction = 0;  ///< where the segment enters the cell
};

// ---- Rays (FR-8..FR-12) ----
bool raycast(const Grid& grid, Vec2 from, Vec2 to, RayHit& hit, const QueryFilter& filter = {});
bool raycast(const BoxView& boxes, Vec2 from, Vec2 to, RayHit& hit, const QueryFilter& filter = {});
/// Nearest of both (Grid{} or an empty view = absent); a cell and a box at the same fraction: the cell.
bool raycast(const Grid& grid, const BoxView& boxes, Vec2 from, Vec2 to, RayHit& hit,
             const QueryFilter& filter = {});
int raycast_all(const Grid& grid, Vec2 from, Vec2 to, RayHit* hits, int capacity, const QueryFilter& filter = {});
int raycast_all(const BoxView& boxes, Vec2 from, Vec2 to, RayHit* hits, int capacity,
                const QueryFilter& filter = {});
int cells_crossed(const Grid& grid, Vec2 from, Vec2 to, CellCrossing* cells, int capacity);
/// Nothing blocks from → to (what contains `from` never counts: hit_from_inside is ignored).
bool line_of_sight(const Grid& grid, const BoxView& boxes, Vec2 from, Vec2 to, const QueryFilter& filter = {});
bool line_of_sight(const Grid& grid, Vec2 from, Vec2 to, const QueryFilter& filter = {});

// ---- Box casts (FR-13) ----
/// Moves `box` (its top-left) to `to` against the view.
bool box_cast(const BoxView& boxes, Rect box, Vec2 to, RayHit& hit, const QueryFilter& filter = {});
int box_cast_all(const BoxView& boxes, Rect box, Vec2 to, RayHit* hits, int capacity, const QueryFilter& filter = {});

// ---- Point and box queries (FR-14, FR-15) ----
int boxes_at(const BoxView& boxes, Vec2 point, int* indices, int capacity, const QueryFilter& filter = {});
int boxes_overlapping(const BoxView& boxes, Rect box, int* indices, int capacity, const QueryFilter& filter = {});
CellHit cell_at(const Grid& grid, Vec2 point); ///< the cell under the point, `outside` cells included
} // namespace nnh::collision
```

```cpp
// <nnh/tilemap.hpp> (changed)
struct TileLayerDesc {
    // ... unchanged fields ...
    collision::Layers collision_layers = collision::kDefaultLayer; ///< used while `collision` is on; ≠ 0
};
class Tilemap {
    // ...
    void set_layer_collision_layers(int layer, collision::Layers layers); ///< Tiled: layer property collision_layers
    collision::Layers layer_collision_layers(int layer) const;            ///< kNoLayers for an invalid layer
    void set_tile_collision_layers(int tile_id, collision::Layers layers); ///< Tiled: tile property collision_layers
    collision::Layers tile_collision_layers(int tile_id) const;           ///< kNoLayers for an unknown id
    /// Union of the sets of every contribution of cell (x, y) (collision layers with a tile there, solid and
    /// one-way tiles there); kNoLayers for an empty cell or outside the map.
    collision::Layers collision_layers_at(int x, int y) const;
    collision::Grid collision_grid() const;                    ///< unchanged
    /// A grid seeing only contributions on `mask`'s layers (FR-6). Live like collision_grid(); kAllLayers gives
    /// collision_grid(). The first call with a new mask registers it in the map (8 masks per map).
    collision::Grid collision_grid(collision::Layers mask) const;
};
```

Exact names may still change while building; stage notes in this file record any change.

### Events introduced

None (spec non-goal, constitution IV): queries return values; enter/exit is the game comparing two frames.

### Semantics fixed by the plan

**Segments.** `d = to − from` (float). A zero `d` hits nothing and crosses no cell (FR-8). `length = sqrt(d.x² + d.y²)`
in float, computed only when a hit is reported (`distance = fraction × length`). Every query reads its inputs once.

**Cell boundaries.** Along x, line `i` of a grid is at `edge_x(i) = origin.x + float(i) × cell_w` (as `move`), so a
cell's interior is `(edge_x(i), edge_x(i + 1)) × (edge_y(j), edge_y(j + 1))`. A point's cell index starts from
`detail::first_cell` and is corrected by at most a step against `edge(i)`, so the walk agrees with its own edges even
when `(v − origin) / size` rounds across one. `cell_at(grid, point)` uses `first_cell` unchanged (spec 012's rule).
Indices saturate at ±2^30.

**The grid walk (FR-9, FR-11)** — one internal iterator (`grid_walk`) used by `raycast`, `raycast_all` and
`cells_crossed`:

1. **Axis on a line.** If `d.x == 0` and `from.x` lies on a vertical line (or the same for y), the segment runs along
   cell edges: it crosses no interior, visits no cell, hits nothing ("touching is not hitting").
2. **Clip.** When `outside` can't block (`Empty`, or `OneWay` for a ray not going down or with `ignore_one_way`), the
   segment is clipped to the grid's rectangle with the same exact comparisons; a segment that misses it visits
   nothing. Fractions stay relative to the whole segment. `cells_crossed` clips only when `outside` is `Empty`
   (outside cells of another kind are listed, as `cells_touching` lists them).
3. **Start cell** = the cell containing `start + ε·d` (on a line, the side the segment goes to). It is **entered at
   its fraction through a face** when the start lies on that cell's back face along a moving axis (x preferred when
   on both), otherwise the start is **inside** it.
4. **Step.** Next lines `nx`, `ny` in the direction of travel; `tx = (edge_x(nx) − from.x) / d.x`, the same for y,
   compared exactly. The walk stops before a line with `t ≥ 1` (a segment ending on a face doesn't enter it).
   `tx < ty`: step x (entered through an x face); `ty < tx`: step y; **`tx == ty` (a corner)**: the x-neighbour, then
   the y-neighbour, then the diagonal cell are visited, all at that fraction — so two diagonal solid cells can't let a
   ray slip between them; the diagonal cell is entered through its x face.
5. **Cost.** One lookup per visited cell; outside cells are looked up only when `outside` can block (they are all
   `outside`).

**Blocking (raycast on grids).** `Solid` blocks when entered. `OneWay` blocks only when entered through its **top
face going down** (`d.y > 0`, entry along y, normal (0, −1)); at a corner the y entry ties with the x entry, so a
one-way cell entered through a corner going down blocks with normal (0, −1) (decision 7); with `ignore_one_way` it
never blocks. A start **inside** a `Solid` cell is ignored unless `hit_from_inside` (then it is the hit: fraction 0,
`point = from`, normal (0, 0), `inside = true`); a start inside a `OneWay` cell never blocks (decision 8).
`raycast_all` reports every blocking cell in walk order; `raycast` stops at the first.

**Rays against boxes (FR-10).** Each box passing the filter (`detects(mask, layers)`, index ≠ `ignore`, `w > 0` and
`h > 0`) gets a slab test: an axis with `d == 0` needs `x < from.x < x + w` strictly (else miss); otherwise entry/exit
parameters per axis; the segment crosses the **interior** when `max(entries) < min(exits)`, `max(entries) < 1` and
`min(exits) > 0`. Entry `≥ 0` → a hit at that fraction through the face of the later entry (x on a tie: a corner);
`max(entries) == 0` is a hit at fraction 0 with a face normal (a ray starting flush on a face, going in). Entry `< 0`
→ the start is inside: ignored unless `hit_from_inside`. A ray that only touches a corner point or runs along an edge
doesn't hit (note: two boxes touching only at a corner don't block a ray through that exact point; grid cells do).

**Box casts (FR-13).** The cast box `B` (top-left `P = (B.x, B.y)`, moving to `to`) against target `T` is a ray of
`P` against `T` grown by `B`'s size (`T.x − B.w … T.x + T.w`), with every numerator computed in `double` from the
three floats (no rounded grown box). Same rules: touching isn't hitting, overlapping at the start (shared area > 0) is
inside, a cast box without area hits nothing (as `overlaps`). The reported `point` is `B`'s top-left at contact: on
the hit axis the largest (moving +) or smallest (moving −) float `p` that keeps `B` flush (`p + B.w ≤ T.x`, found by
one-ulp steps on the bit pattern, as `move`'s `place_before`, without `nextafter`); on the other axis `P + t·d` rounded.
Two movers: `box_cast(view of B, A, A + (dA − dB))` (documented recipe).

**Reported values.** `fraction` = the exact entry ratio computed in `double` and rounded to `float` (monotone, so walk
order and fraction order agree). On the crossed face's axis `point` is that face's float coordinate **exactly**; the
other coordinate is `from + ratio × d` in `double`, rounded, and clamped to the face's span. `normal` is (±1, 0) or
(0, ±1). `distance = fraction × length` (float).

**Ordering and buffers (FR-11).** All-hit queries keep the `capacity` nearest in the caller's buffer by insertion
(stable: equal fractions keep scan order — lowest box index, or walk order for cells) and return the total (may
exceed `capacity`); null buffer + capacity 0 counts. `raycast` over grid and boxes: nearest; equal fractions → the
cell. Point/box queries list indices ascending.

**Misuse (FR-17).** Non-finite `from`/`to`/`point`/`box`, negative count or capacity, a null buffer with capacity > 0,
`first == nullptr` with count > 0, `stride < sizeof(Rect)` with boxes, `layers_stride < 4` with layers, cell sizes
≤ 0: assert in Debug/Development; Release returns false / 0 hits / an `Empty` cell at (0, 0) and never reads the
memory. No log in the normal path. Boxes with NaN fields are never hit (game data, not misuse).

### Determinism (FR-16)

- Spec 015's rules: IEEE `+ − × ÷`, comparisons, float↔int and float↔double conversions; every float operation in the
  module's `.cpp` files, compiled with `-ffp-contract=off` (GCC/Clang). Decisions use `double` (VFPv2 on the 3DS, SSE2
  on x86-64: both IEEE binary64, round to nearest — libctru sets FPSCR so, plan 015). Inputs of subnormal magnitude
  are not guaranteed bit-identical (the 3DS flushes them; same note as tweens).
- `floor` comes only from `detail::first_cell` (spec 012's, exact). The square root lives in one file,
  `ray_distance.cpp`, compiled with `-fno-math-errno` too, so `std::sqrt(float)` is one `vsqrt.f32` (3DS) / `sqrtss`
  (PC) instruction; T2.4 checks the disassembly on both (no call to `sqrtf`). IEEE 754 requires it correctly rounded.
- AC-2's golden checksum covers fractions, distances, points, normals, indices, cells and order; it runs in
  `nnh_tests` on WinLibs GCC, GCC 13/14 and clang 18 (VM 120) and in the console self-check (AC-6).
- Spec 012's collision files keep their flags (no `-ffp-contract=off` added: FR-18 wants them byte-identical).

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/collision.hpp` | changed | the API above (≈ 300 lines with comments) |
| `engine/core/src/collision/layers.cpp` | new | `detail::bad_layer` (assert, kNoLayers) |
| `engine/core/src/collision/query_check.hpp` / `.cpp` | new | misuse checks shared by every query (finite inputs, views, buffers, grids) |
| `engine/core/src/collision/box_view.hpp` | new | internal: read box/layers `i` (`memcpy`), `passes(filter, i)` |
| `engine/core/src/collision/hit_list.hpp` / `.cpp` | new | capped sorted insertion of hits/indices, total count |
| `engine/core/src/collision/exact.hpp` / `.cpp` | new | segment setup, exact ratio compare (cross products in `double`), fraction/point rounding, one-ulp float steps on the bit pattern |
| `engine/core/src/collision/ray_box.hpp` / `.cpp` | new | slab test of one box / one grown box: entry, face, inside |
| `engine/core/src/collision/raycast_boxes.cpp` | new | `raycast`, `raycast_all` on a view (FR-10, FR-11) |
| `engine/core/src/collision/ray_distance.cpp` | new | `detail::ray_length` (the only `sqrt`; `-fno-math-errno`) |
| `engine/core/src/collision/box_cast.cpp` | new | `box_cast`, `box_cast_all`, flush placement (FR-13) |
| `engine/core/src/collision/grid_walk.hpp` / `.cpp` | new | the exact Amanatides & Woo iterator: line-on-axis, clip, start cell, steps, corners |
| `engine/core/src/collision/cells_crossed.cpp` | new | `cells_crossed` (FR-11) |
| `engine/core/src/collision/raycast_grid.cpp` | new | `raycast`, `raycast_all` on a grid: blocking, one-way, inside (FR-9) |
| `engine/core/src/collision/raycast_world.cpp` | new | grid + boxes nearest, `line_of_sight` (FR-12) |
| `engine/core/src/collision/point_queries.cpp` | new | `boxes_at`, `boxes_overlapping`, `cell_at` (FR-14, FR-15) |
| `engine/core/include/nnh/tilemap.hpp` | changed | `TileLayerDesc::collision_layers`, the five methods, `collision_grid(mask)` |
| `engine/core/src/tilemap/map_runtime.hpp` / `.cpp` | new | D34's runtime area: layout, `map_block_bytes`, `init_map_runtime`, accessors |
| `engine/core/src/tilemap/map_collision_layers.cpp` | new | set/get layer and tile sets, `collision_layers_at` |
| `engine/core/src/tilemap/map_filtered_grid.cpp` | new | `collision_grid(mask)`, slots, `grid_cell_masked` |
| `engine/core/src/tilemap/map_load_layers.cpp` | new | at load: `collision_layers` properties of tile layers and tiles → runtime sets (invalid → warning, default) |
| `engine/core/src/tilemap/tilemap.cpp`, `map_load.cpp`, `map_build.cpp` | changed | allocate `map_block_bytes`, init the runtime area; read properties after `copy_map`; `add_tile_layer` stores `desc.collision_layers` |
| `engine/core/src/tilemap/map_collision.cpp` | **unchanged** | `collision_grid()`, `collision_at`, `cell_kind` (the proof of FR-18) |
| `engine/core/CMakeLists.txt` | changed | sources; `-ffp-contract=off` on the new collision files (`_nnh_query_sources`); `-fno-math-errno` on `ray_distance.cpp` |
| `tools/assetc/src/tiled_properties.hpp` / `.cpp` | changed | `check_collision_layers(props, where, error)`: int and ≠ 0, kept as a property |
| `tools/assetc/src/tiled_layer_data.cpp`, `tiled_tileset.cpp` | changed | call it for tile layers and tiles |
| `tools/assetc/CMakeLists.txt` | changed | version 0.4.0 |
| `tests/framework/test_main.cpp` | changed | `kMaxTests` 2048 → 4096 (199 were left) |
| `tests/collision/test_*.cpp`, `ray_reference.hpp` | new | see Test strategy |
| `tests/tilemap/test_collision_grid_regression.cpp` | new (stage 0) | pinned hash of `collision_at`/`collision_grid()` cells over loaded, code-built and edited maps |
| `tests/tilemap/test_map_collision_layers.cpp`, `test_filtered_grid.cpp`, `test_map_load_collision_layers.cpp` | new | FR-6, AC-3 |
| `tests/data/maps/make_fixtures.py` + `good_collision_layers.tmj`, `bad_collision_layers_type.tmj`, `bad_collision_layers_zero.tmj` | new | FR-7 fixtures |
| `tools/assetc/tests/test_tiled_collision_layers.cpp` | new | converter checks |
| `tests/queries_example/test_self_check.cpp` | new | AC-2 on the example's `self_check.cpp` |
| `tests/platformer/test_coin_book.cpp`, `test_replay_run.cpp` | changed | layered coins (step 1); the turret word (step 2) |
| `tests/platformer/test_turrets.cpp` | new | step 2's pure turret logic |
| `tests/CMakeLists.txt` | changed | the new tests, `examples/queries/self_check.cpp`, `examples/platformer/turrets.cpp` |
| `examples/queries/` | new | `main.cpp`, `room.{hpp,cpp}` (tilemap built in code, boxes on three layers), `queries_scene.{hpp,cpp}` (emitter, target, modes, toggles), `query_draw.{hpp,cpp}` (the copyable debug-drawing helper), `hud.{hpp,cpp}` (mask, mode, "under" list), `demos.hpp` (`QUERIES_DEMO`), `self_check.{hpp,cpp}`, `self_check_scene.{hpp,cpp}`, `bench_scene.{hpp,cpp}`, `art/make_art.py` → `assets/art/tiles.png`, `CMakeLists.txt`, `tests/check.nnhs` |
| `examples/CMakeLists.txt` | changed | `add_subdirectory(queries)` |
| `examples/platformer/layers.hpp` | new | the game's layer enum and masks |
| `examples/platformer/coin_book.{hpp,cpp}` | changed | coins carry `Layers`; `collect` = `boxes_overlapping` over a struct view; taken = `kNoLayers` |
| `examples/platformer/level_play.{hpp,cpp}` | changed | player grid `collision_grid(kPlayerMask)`; step 2: turrets, bullets, hit flash, sample word |
| `examples/platformer/turrets.{hpp,cpp}` | new (step 2) | turrets and bullets, pure (only collision queries) |
| `examples/platformer/replay_run.{hpp,cpp}`, `hud.{hpp,cpp}` | changed (step 2) | `ReplaySample::turrets` in the checksum; hits on the HUD |
| `examples/platformer/art/make_level.py`, `assets/levels/level1.tmj` | changed (step 2) | two `turret` objects |
| `examples/platformer/tests/level.nnhs` | changed (step 2) | regenerated golden lines |
| docs (stage 11) | changed | `docs/api/collision.md`, `tilemap.md` (sets, filtered grids, memory formula), `docs/guides/levels.md`, `docs/REFERENCES.md`, `decisions.md` (D34 note), `CLAUDE.md`, `docs/ROADMAP.md`, `specs/README.md` |

### Data and memory

| Item | Size (3DS / PC) | Notes |
|------|-----------------|-------|
| `Layers` | 4 B | the game's arrays hold them |
| `BoxView` | 20 B / 32 B | by value or `const&`; owns nothing |
| `QueryFilter` | 12 B | |
| `RayHit` | 44 B | caller's buffers |
| `CellCrossing` | 16 B | |
| map runtime area | 132 B + 4 B × (`max_tile_ids` + 1) | every map: default desc (256 ids) 1160 B; 8191 ids 32.9 KB; tag `tilemap`, same block |
| query stack | ≤ 96 B | no heap, no static state |

No allocation anywhere; nothing at the first call. `TilemapDesc` defaults unchanged, so an existing game's map grows
by its runtime area only (`docs/api/tilemap.md#memory` gets the term).

### Platform work

- **pc / 3ds:** none. All of it is portable core code; no contract change. The 3DS runs the self-check, the touch
  queries and the bench (stage 10).
- **fake:** none. Queries need no `init()`; tilemap tests that add tilesets use the fake platform's textures as today.

### Performance (AC-7)

Old 3DS estimates (268 MHz ARM11, VFPv2: double multiply ≈ 2 cycles throughput, `vcmp`+`vmrs` ≈ 4 cycles):

- **Box slab test:** filter (one load, `and`), a float bounding-box reject of the segment, then about 8 double
  operations when the boxes overlap: ≈ 15 cycles rejected, ≈ 60 tested.
- **Grid step:** two edge numerators, two products, one compare, the callback; a filtered tilemap lookup over 2–3
  collision layers ≈ 60 cycles; ≈ 100–120 cycles per visited cell.
- **AC-7 bench** (128 rays of 0–400 px, mean ≈ 200 px, about 17 cells each through 16×16 tiles, plus 64 boxes): grid
  ≈ 2200 cells × 110 ≈ 0.9 ms, boxes 8192 × ≈ 25 ≈ 0.75 ms → **≈ 1.7 ms against the 2 ms target**: the target is
  confirmed, with little margin (risk 1).
- **Point queries:** ≈ 20–25 cycles per box (four loads, layers, four compares). 256 queries × 256 boxes = 65 536
  tests ≈ 5–6 ms. The spec's "under 0.3 ms" would be 1.2 cycles per box — unreachable without a broadphase (a
  non-goal). The bench keeps the workload and reports it; the target becomes **≤ 0.1 µs per box tested** (one touch
  query over 256 boxes ≤ 0.026 ms) (decision 17, flagged in the report).
- **`move` through `collision_grid()`:** same code (D34), so same cost; the bench also logs `move` through the
  unfiltered and a filtered grid (the filtered overhead is documented, not judged). Spec 012's gate is re-run on the
  console (stage 10).
- **`hello` (AC-8):** no change expected at all (it includes neither header; no core file it links changes). Checked
  with `size` and `nm` (T3.7, T8.4).

## Test strategy

All unit tests run in `nnh_tests` on the fake platform (Windows and VM 120). Queries need no `init()`; tests that use
tilemaps with tilesets use the fake platform's `init`. `tests/collision/ascii_grid.hpp` draws test grids.

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `tests/collision/test_layers.cpp`: constants, `layer(0..31)`, `static_assert`s in constant expressions, out of range asserts (assert hook) and returns 0, bit operators on a game enum |
| FR-2 | `test_layers.cpp`: `detects` table, no-layers entries never detected, empty mask detects nothing, the symmetric recipe |
| FR-3 | `test_box_view.cpp`: plain array, parallel arrays, structs with stride (padding, layers before/after the box, an enum-typed layer field read via its bits), absent layers = all layers, count 0, null `items` |
| FR-4 | `test_box_view.cpp` + each query's tests: mask, ignore index (also out of range: ignored), hit from inside, ignore one-way |
| FR-5 | spec 012's `tests/collision/*` unchanged and passing; `test_collision_grid_regression.cpp` (pinned in T0.2) |
| FR-6 | `tests/tilemap/test_map_collision_layers.cpp` (defaults, setters/getters, invalid layer/id/0 assert and keep, `TileLayerDesc::collision_layers`, `collision_layers_at` unions); `test_filtered_grid.cpp` (masks that keep, drop, or turn a cell one-way; `collision_grid(kAllLayers)` == `collision_grid()`; masked vs unmasked on 200 random maps with every set = default; live edits; `move` through a filtered grid; same mask twice = same context; 9th mask asserts / Release unfiltered + one log; grids survive moving the `Tilemap`) |
| FR-7 | `tools/assetc/tests/test_tiled_collision_layers.cpp` (each refusal prints `file: reason`, flag enum with `propertytype` accepted, layer and tile); `tests/tilemap/test_map_load_collision_layers.cpp` (fixture loads with the sets; a string/0 `collision_layers` in a hand-made file → default + warning; `good.tmj` and the platformer's maps give the regression grid); T0.3/T5.3 byte-identical conversion of maps without the property |
| FR-8 | `test_raycast_boxes.cpp`, `test_raycast_grid.cpp`: zero-length ray, fraction/distance, flush point (face coordinate exact), normals, cell/box fields |
| FR-9 | `test_grid_walk.cpp`: 8 octants + 4 axis cases, start/end on edges, along an edge (nothing), ending on a face, corners (both neighbours, x first, diagonal), clip from far outside, 10 000-px rays, non-square cells, negative and fractional origins, saturation; **exact reference** (`ray_reference.hpp`: lattice coordinates in `int64`, cell interiors tested exactly, sorted by exact ratio) equal cell for cell on 5000 seeded rays, plus a sampling check (every sampled point's cell appears, in order); `test_raycast_grid.cpp`: solid, one-way from above/below/side/corner, `ignore_one_way`, inside with and without `hit_from_inside`, `outside` Solid/Empty/OneWay, cost (lookup counter in the callback: one per visited cell; a far ray with Empty outside looks up only inside cells) |
| FR-10 | `tests/collision/test_ray_box.cpp`: slab entry per face, grazing edges, corner touch (miss), corner entry (x face), zero-size, NaN box, start inside, start flush on a face going in (fraction 0, face normal) and going out (miss) |
| FR-11 | `test_hit_list.cpp` (sorting, stability, overflow keeps the nearest, total, null + 0 counts); `test_raycast_all_grid.cpp` (piercing laser: every blocking cell in order); `test_raycast_boxes.cpp` (all hits, ties lowest index); `test_cells_crossed.cpp` (equals the walk, fractions non-decreasing, first fraction 0 or the clip fraction, buffer contract, outside non-empty listed); first hit == first blocking cell of `cells_crossed` over the 5000 seeded rays |
| FR-12 | `test_raycast_world.cpp`: nearest of both, cell wins a tie (box flush behind a wall), absent grid/boxes, `line_of_sight` (clear, blocked by a cell, by a box, viewer's own box excluded by `ignore` and by layers, start inside ignored) |
| FR-13 | `test_box_cast.cpp`: each face, flush position (`overlaps` false after placing, true one ulp further), corner tie, sliding along a face (miss), overlapping at the start (ignored / inside), zero-size cast box, relative motion of two movers, all contacts sorted |
| FR-14, FR-15 | `test_point_queries.cpp`: half-open edges, ascending order, ignore, mask, buffer contract; `cell_at` inside, outside each kind, negative coordinates; `cells_touching` on a filtered grid |
| FR-16 | `tests/queries_example/test_self_check.cpp` (AC-2 golden); T2.4 disassembly check (no `sqrtf` call, no fused ops via `objdump` as plan 015); VM 120 compilers (stage 9) |
| FR-17 | `test_query_misuse.cpp`: every misuse asserts (assert hook) and returns false/0 in the Release path; no allocation (`mem::live_allocations`, alloc counter) over 10 000 mixed queries; no `init()` needed |
| FR-18 | T0.1/T3.7/T8.4: `hello` sizes and `nm` (no query, no `grid_cell_masked` symbol); objects of `map_collision.cpp` identical (T4.6); spec 012 tests and the regression test pass; stage 6 replay lines unchanged |
| AC-4 | `test_coin_book.cpp`, `test_turrets.cpp`; ctest `platformer_replay_matches_script` (step 1 unchanged, step 2 regenerated); `level.nnhs --pc`; screenshot |
| AC-5 | PC screenshots `QUERIES_DEMO=first|all|cast|water|touch`, read back |
| AC-6 | `examples/queries/tests/check.nnhs` with `--pc`, then on the console |
| AC-7 | `examples/queries` bench lines on the console; `gate.nnhs` on the console |
| AC-8 | stage 0 reference screenshots (`sprites`, `scenes`, `camera`, platformer bench) compared byte for byte after stages 4, 6 and 8; `hello` sizes |

## Risks and alternatives

1. **The 2 ms bench target is close** (≈ 1.7 ms estimated). Mitigations in order, if the console says so: a float
   pre-test that settles clear cases and falls back to the exact `double` comparison only near a tie (still
   deterministic: both paths are IEEE); reject boxes by the segment's bounding box before any `double`; inline the
   filtered tilemap lookup for the walk. The measured number is documented either way.
2. **The point-query target as written is impossible** (decision 17). The maintainer may prefer another workload
   (for example 32 point queries over 256 boxes ≈ 0.8 ms); the plan's per-box target is the honest number.
3. **Two copies of the cell rule** (`cell_kind` in `map_collision.cpp`, the masked one in `map_filtered_grid.cpp`).
   Accepted to keep the unfiltered file untouched; `test_filtered_grid.cpp` compares them cell by cell on random maps,
   so they can't drift.
4. **8 masks per map.** Masks are game constants (player, enemy, line of sight); a game that builds masks at run
   time from bits could run out. Loud in Debug/Development, walls kept in Release; raising the count costs 8 B per
   mask per map.
5. **`double` on the 3DS.** VFPv2 has it in hardware (no soft-float); cost included in the estimates. If the bench
   shows it dominates, risk 1's float pre-test applies.
6. **Corner asymmetry between cells and boxes** (a ray through the exact shared corner of two diagonal boxes passes,
   between two diagonal cells it doesn't). Required by FR-9/FR-10 as written; documented, and a game that needs
   sealed corners overlaps its boxes by a pixel.
7. **A `collision_layers` property set in code** (`set_int`) does not change collision (as `solid` is not a property
   after loading): documented; the setters are the API.
8. **Step 2 golden lines on the console.** The turrets' rays are exactly what could diverge; the console runs the
   queries self-check (AC-6) first, so a mismatch in `level.nnhs` would be a game bug, not an engine one. The `x`,
   `y` and `coins` columns of every line must stay as they are (bullets don't move the player): an independent check.
9. **Test runner capacity**: `kMaxTests` raised to 4096 in T0.5 (the spec adds ≈ 200 tests).

## Decisions taken (pre-approved)

Small decisions the plan would normally ask about; each takes the recommended option.

1. **Names:** `collision::Layers`, `kNoLayers`, `kAllLayers`, `kDefaultLayer`, `layer(n)`, `detects`; `BoxView` +
   `box_view(...)`; `QueryFilter`; `RayHit`, `CellCrossing`; `raycast`, `raycast_all`, `cells_crossed`,
   `line_of_sight`, `box_cast`, `box_cast_all`, `boxes_at`, `boxes_overlapping`, `cell_at`; tilemap
   `set/layer_collision_layers`, `set/tile_collision_layers`, `collision_layers_at`, `collision_grid(mask)`,
   `TileLayerDesc::collision_layers`.
2. **Results by out-parameter + `bool`/count** (no `<optional>` in the header): `if (RayHit hit; raycast(...))`.
3. **"Absent" grid** = `Grid{}` (nothing inside, outside Empty); absent boxes = an empty view. No pointer overloads.
4. **The mask filters boxes only**; grids are filtered where layers are known (`collision_grid(mask)`), per FR-5.
5. **`ignore_one_way` lives in `QueryFilter`** (one small value for every query).
6. **Layer fields read by bits** (`memcpy`), so a game may type them as its own 32-bit enum through the member-pointer
   helper (`static_assert` on the size).
7. **A one-way cell entered through a corner going down blocks** (normal (0, −1)); the x-face tie rule is for faces
   that block from that side.
8. **A ray starting inside a one-way cell is never blocked by it**, even with `hit_from_inside` (it blocks only
   through its top).
9. **`cells_crossed` follows `cells_touching`:** clipped to the grid when `outside` is Empty, outside cells listed
   otherwise; a segment lying on a grid line crosses nothing.
10. **Exact predicates in `double`**, outputs rounded once to `float` (D34).
11. **`collision_grid(kAllLayers)` returns `collision_grid()`**; layer sets are never 0 (setters assert and keep the
    previous value; the converter refuses 0), so "all layers" filtering and no filtering agree.
12. **Runtime area after the format bytes**, 8 grid-mask slots, never reused; out of slots → unfiltered grid + one
    error log per map in Release.
13. **The loader treats a non-int or 0 `collision_layers` as absent** with a warning (old maps may hold a user's own
    property of that name); the converter refuses them. `collision_layers` on a layer without `collision` or a tile
    without `solid`/`one_way` is accepted and has no effect (documented).
14. **`nnh-assetc` 0.4.0** (a new property rule).
15. **Example camera:** the bottom screen shows the room through a hand-set `Camera2D` (position (40, 0)), and a
    touch becomes `position + touch` — the one-line conversion, no camera module (XI).
16. **Platformer layers:** `kWorld = kDefaultLayer` (the maps' sets stay default, so step 1's grid is identical),
    `kPlayer`, `kCoin`, `kEnemy`, `kEnemyBullet`; the player moves through `collision_grid(kPlayerMask)`; bullets are
    points moved by `raycast(grid, bodies, ...)` with 8 fixed directions of short binary fractions (no `sqrt`, exact
    sums), bodies = the player and the turrets in one struct view (turrets filtered out by the bullets' mask). The
    replay line format is unchanged (the turret state is in the sum only), so the ctest's regex holds.
17. **AC-7's point-query target recast** to ≤ 0.1 µs per box tested (above); the ray target (2 ms) confirmed.
    **Confirmed by the maintainer 2026-11-23**: the old target (256 queries × 256 boxes < 0.3 ms) is dropped, no
    broadphase; the bench still runs the 256 × 256 workload and judges it per box tested. The spec's AC-7 says so.
18. **Console stage:** three scripted runs, each started with Homebrew Launcher → Y (queries `check.nnhs`, platformer
    `level.nnhs`, platformer `gate.nnhs`); nothing else is asked of the maintainer.

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | Portable core only; exact `double` predicates, outputs rounded once, `-ffp-contract=off`, one inlined IEEE `sqrt`; golden on every compiler and the console |
| II | ✅ | No heap; 132 B + 4 B per tile id per map; bench target confirmed, point target recast per box with the arithmetic (decision 17) |
| III | ✅ | `<cstdint>` and one small template in `collision.hpp`, measured; nothing in `hello` |
| IV | ✅ | Queries only |
| V | ✅ | No allocation, `virtual`, RTTI or exceptions; misuse never UB |
| VI | ✅ | Tests first, an exact reference for the traversal, the grid regression pinned before the tilemap change, the platformer swap with unchanged lines |
| VII | ✅ | Small files of one concept; one public header per module |
| VIII | ✅ | API pages, guide, header comments, new example, the platformer, D34 |
| IX | ✅ | Own code, prior art cited |
| X | ✅ | Converter refusals with fixtures, tolerant loader still fuzzed, loud misuse |
| XI | ✅ | D34: `Grid`, `collision_grid()` and the format unchanged; query code linked only when used; the benchmark keeps plain code |

No constitution exception is requested. D34 is new (see [decisions.md](../../docs/architecture/decisions.md)); one
acceptance target (AC-7's point queries) is recast and reported.

## Stage 0 results

Measured 2026-11-23 on the maintainer's Windows machine (WinLibs GCC, devkitARM), before any spec 019 change.

- `hello` sizes (`size`, text / data / bss): pc-release `hello.exe` 130 732 / 22 020 / 211 696; pc-debug 198 816 /
  26 756 / 214 352; 3ds-release `hello.elf` 201 188 / 28 352 / 106 680 (`.3dsx` 246 576 bytes); 3ds-dev `hello.elf`
  217 992 / 32 872 / 109 288. (The 3DS text is smaller than plan 018's 216 748: the devkitARM on this machine was
  updated since; every later number here is compared with these.)
- Build times: edit `examples/hello/main.cpp` → simulator (pc-debug) 1.94, 1.50, 1.49 s; → `.3dsx` (3ds-release)
  3.05, 2.71 s; clean `nnh_core` (pc-debug, `ninja -t clean nnh_core`, parallel) 5.34, 5.27, 5.26 s.
- Header cost (`g++ -std=c++20 -fsyntax-only` of a one-line TU, 3 runs): `<nnh/collision.hpp>` 151 preprocessed
  lines, 0.27–0.32 s; `<nnh/tilemap.hpp>` 8331 lines, 0.41–0.45 s.
- `objdump -d` of `map_collision.cpp` (pc-release `.obj`, 3ds-release `.o`) saved as
  `build/ref-019/map_collision.{pc,3ds}.before.dis` (360 and 320 lines; `build/ref-019/baseline.sh <suffix>` redoes it).
- T0.2: `tests/tilemap/test_collision_grid_regression.cpp`, pinned on the unchanged code. Cells (every cell of the
  map and a one-cell ring, `collision_at` checked equal to the grid's own lookup) and moves (400 boxes from an LCG
  walk, each moved with outside Empty and Solid, plus `contacts` at the end and `cells_touching` at the start) over
  `good.tmj` (cells `0x698EB725`, moves `0xE419AD3C`), the platformer's `level1.tmj` (converted by the tests' build,
  `0x852ED0F6` / `0xA6D13AF0`), a 20 × 12 map built in code with four layers and two tilesets (`0x2E9EDC46` /
  `0x8761516F`) and the same map after `set_tile`/`fill`/`set_layer_collision`/`set_tile_collision` edits
  (`0x6EA3F8C6` / `0xB2975DBA`). Each case also checks that the map has solid (and, but for `good.tmj`, one-way) cells.
- T0.3: converted with `nnh-assetc` 0.3.0: `good.nnhmap` SHA-256
  `97d36d4c9a5b57194bdb93a0914e24af25474fd2ee65eeac4d2a78dac25299de`, `level1.nnhmap`
  `d4199d18b278062d3792af150784bf2eeb25d3d9cd3e1d321234ad6102c792d1` (copies in `build/ref-019/`).
- T0.4: reference screenshots in `build/ref-019/ref/` (not committed): `hello`, `sprites`, `scenes` at frame 30,
  `camera` (`CAMERA_DEMO=zoom1`) at 30, platformer `PLATFORMER_DEMO=bench` (top screen only) and level 1 at 90.
  `build/ref-019/compare.sh` takes them again into `now/` and compares the pixels.
- T0.5: `kMaxTests` 4096; `nnh_tests` 1853 tests.

## Stage 1–3 refinements and results

- **The segment is held in `double`** (`detail::Segment`): `d = to − from` is the exact difference of the two
  floats, not a rounded float `d`; a zero `d` is still exactly `to == from`. Every ratio is `n / |d|` with `n` a
  difference of floats in `double` (`detail::Ratio`), compared by cross products (`detail::compare`).
- **Ordering between hits uses the reported float fraction**, stable: the first hit of a view is the lowest index
  among the smallest fractions, `raycast_all`/`box_cast_all` sort by fraction keeping scan order on ties, and
  `raycast(grid, boxes)` lets the cell win when the fractions are equal. The exact ratios decide everything inside
  one box test and the grid walk (entry face, corners, start cell, touching); since rounding is monotone, two hits
  ordered by exact ratio never come out in the opposite order. This keeps `raycast` equal to `raycast_all(...)[0]`.
- **The grid walk** (`grid_walk.{hpp,cpp}`) is a small iterator with a queue of three cells (a corner pushes the
  x-neighbour, the y-neighbour and the diagonal). A start or clip cell is found from a float guess corrected by
  exact comparisons (at most a few steps), so it always agrees with the walk's own edges. A clip that touches the
  grid's rectangle **only at one of its corners** still visits the grid's corner cell beside it (the corner rule):
  the exact reference found this case (a ray passing diagonally outside the grid's corner), and the clip accepts
  `enter == exit` for it. The walk takes at most `4 + |dx|/w + |dy|/h` lines (≤ 2^26) and stops at the ±2^30
  index saturation, so absurd inputs end and never overflow.
- **One-way outside**: the walk clips whenever the outside can't block (Empty; OneWay for a ray not going down or
  with `ignore_one_way`). A downward ray with a OneWay outside walks the outside cells one by one (no lookup: they
  are all `outside`) instead of skipping runs; documented as the cost.
- **Hit points**: a cell entered exactly at a grid corner reports the corner itself (both coordinates exact), not a
  rounded `from + t·d`; this covers the corner's side cells and the diagonal.
- **Box casts**: the contact position starts from `T.x − B.w` (float) and steps down by growing ulps until
  `p + B.w ≤ T.x` in float (as `move`'s `place_before`), then is kept between the cast's start and end; moving
  the other way it is exactly `T.x + T.w` as `overlaps()` sums it. It is flush (`overlaps()` false) and one ulp
  further overlaps (tested on non-lattice values); for lattice values it is the exact contact, not the largest float
  that would still be flush. A zero-length cast touches nothing, even from inside (as a zero-length ray).
- **Misuse**: `raycast(grid, boxes)` checks both inputs before either query (no half answer); `line_of_sight`
  returns **false** on misuse in Release (it never claims a clear view it couldn't check). An `ignore` index out of
  range ignores nothing. Boxes with non-finite fields are never candidates (`detail::passes`).
- **API note**: `raycast(Grid, ...)` and `raycast(BoxView, ...)` overloads make a bare `{}` first argument ambiguous;
  callers write `collision::Grid{}` or `collision::BoxView{}` (documented).
- **Fused operations**: GCC uses `vmla.f32` on the 3DS even with `-ffp-contract=off`; VFPv2's `vmla`/`vmls` are
  *chained* (the product is rounded before the add), so they give the bits of `vmul` + `vadd`. The check
  (`build/ref-019/fp_check.sh`) looks for the fused `vfma`/`vfms`/`vfnma`/`vfnms` (3DS) and `vfmadd`/`vfmsub` (PC):
  none in any query object. T2.4: `ray_distance.cpp` is `sqrtss` on pc-release and `vsqrt.f32` on 3ds-release, no
  call.
- **Tests** (stage 1–3): `test_layers`, `test_box_view`, `test_hit_list`, `test_point_queries`, `test_exact`,
  `test_ray_box`, `test_raycast_boxes`, `test_box_cast`, `test_grid_walk` (with `ray_reference.hpp`: the walk equals
  the integer reference cell for cell and fraction for fraction on 5000 seeded rays clipped and 5000 with outside
  cells, over four cell layouts with negative and fractional origins, ≥ 300 rays through corners each),
  `test_cells_crossed`, `test_raycast_grid` (first hit == first solid cell of `cells_crossed` on 5000 rays),
  `test_raycast_all_grid`, `test_raycast_world`, `test_query_misuse` (22 misuse cases; 10 000 mixed queries with no
  allocation and no `init()`). `nnh_tests` 1967 tests.
- T3.7: `hello` Release unchanged (PC text 130 732, 3DS text 201 188) and no query symbol in either (`nm`). Each
  query object needs only its own helpers (`nm -u`): `point_queries` → `query_check`, `aabb`, `grid`;
  `raycast_boxes`/`box_cast` → `exact`, `ray_box`, `hit_list`, `ray_distance`; `raycast_grid`/`cells_crossed` →
  `grid_walk`, `exact`, `grid`; `raycast_world` → the ray files.

## Stage 4 refinements and results

- As planned (D34): `map_runtime.{hpp,cpp}` (layout, `map_block_bytes`, `init_map_runtime`, accessors),
  `map_collision_layers.cpp`, `map_filtered_grid.cpp`, `map_load_layers.cpp`; `create`/`tilemap_from_file`
  allocate `map_block_bytes` and initialise the area; `add_tile_layer` stores `desc.collision_layers`.
- **An unused slot is one whose mask is kAllLayers** (that mask never takes a slot: it returns
  `collision_grid()`); `block_offset` is written when the slot is first taken.
- **`add_tile_layer` refuses `collision_layers = kNoLayers`** with InvalidArgument ("collision_layers can't be
  empty"): it returns a `Result`, so a refusal is the natural misuse path there (the setters assert and keep).
- **The loader** reads `collision_layers` with the public `Properties` view after the copy: layers first, then every
  tile with properties; a non-int or 0 value keeps the default with one warning per map
  (`load_tilemap("<name>"): a collision_layers property isn't a non-zero int: ignored`). Its fuzz target now also
  reads every set, checks `collision_layers_at` against `collision_at` (no layers ⇔ Empty) and moves through a
  filtered grid; a fourth seed (`map_file::layered()`) carries the property on a layer, on a tile and a bad one.
- The memory formula gains `132 + 4 × (max_tile_ids + 1)` bytes per map (default desc: 11 940 → 13 100); the two
  tests that pinned the old block size (`test_map_build`, `test_map_edit_after_load`) now pin the new one.
- T4.5/T4.6: `test_collision_grid_regression.cpp` passes unchanged; `map_collision.cpp` is not in the diff and its
  `objdump -d` is identical to stage 0's on pc-release and 3ds-release (`build/ref-019/map_collision.*.stage4.dis`);
  `compare.sh`: every reference screenshot identical. `nnh_tests` 1991 tests; ctest pc-debug 258/258, pc-release
  252/252; 3ds-release builds without a warning.

## Stage 5 refinements and results

- `check_collision_layers(props, where, zero_hint, error)` (`tiled_properties.{hpp,cpp}`), called after
  `collision`/`solid`/`one_way` are taken from tile layers and tiles. The refusals:
  `<file>: layer "water": property "collision_layers" must be an integer (the layer bits; in Tiled an enum with
  "allow multiple values" saved as a number)`, `<file>: layer "water": property "collision_layers" must not be 0:
  untick collision instead`, and for a tile `<file>: tile 1: ... must not be 0: remove solid or one_way instead` (a
  tile has no `collision` flag to untick). A flag enum saved as a number is an `int` with a `propertytype`: accepted;
  saved as a string (an enum without "save as number") it is refused as not an integer.
- **Fixtures are written by hand** like the other Tiled fixtures (`make_fixtures.py` only makes PNGs):
  `good_collision_layers.tmj` (bits world 1, water 2, plank 4, boat 8: a "water" layer on boat, a solid tile on
  water and a one-way tile on world | plank, as Tiled writes a flag enum), `bad_collision_layers_type.tmj`,
  `bad_collision_layers_zero.tmj`; ctests `map_convert_accepts_good_collision_layers` and
  `map_convert_{rejects,explains}_collision_layers_{type,zero}`.
- T5.2: the good fixture is converted by the tests' build (`good_collision_layers.nnhmap`) and loaded by
  `test_map_load_collision_layers.cpp`: the expected layer and tile sets, `collision_layers_at` unions, filtered
  grids for world, boat and water, the property still readable.
- T5.3: `nnh-assetc` 0.4.0 converts `good.tmj` and `level1.tmj` to the same bytes as 0.3.0 (SHA-256 as in stage 0).
  The converter's map fuzz seed (`kMapSeed`/`kSetSeed` in `tools/assetc/tests/test_fuzz_assetc.cpp`, also the seed
  for the libFuzzer `fuzz_assetc_map`) carries `collision_layers` on a layer and on a tile.

## Stage 6 refinements and results

- `examples/platformer/layers.hpp` puts the layers in a namespace (`layers::kWorld`, `kPlayer`, `kCoin`, `kEnemy`,
  `kEnemyBullet`, `kPlayerMask = kWorld | kCoin`, `kBulletMask = kWorld | kPlayer`): `kPlayer` alone would clash
  with the platformer's draw-layer enum.
- `CoinBook::Coin` gained `layers` (`kCoin`; `kNoLayers` once taken: the `taken_` array is gone, `taken(i)` reads the
  layers); `collect(box, mask, taken, capacity)` is one `boxes_overlapping` over `box_view(coins_, count_,
  &Coin::bounds, &Coin::layers)` into a local buffer of `kCapacity` indices, so every overlapped coin is taken even
  past the caller's capacity, as before. `LevelPlay` moves the player through `collision_grid(layers::kPlayerMask)`
  (outside Solid as before) and collects with the same mask.
- T6.2: ctest `platformer_replay_matches_script` passes and `nnh run platformer --pc --no-build --script
  examples/platformer/tests/level.nnhs` → `nnh: PASS (74 lines, frames 1–1553)`, both with the **unchanged**
  checksum lines; `compare.sh`: level 1 and the bench identical.

## Stage 7 refinements and results

- **Files** as planned (`main.cpp`, `demos.hpp`, `room.{hpp,cpp}`, `queries_scene.{hpp,cpp}`, `query_draw.{hpp,cpp}`,
  `hud.{hpp,cpp}`, `self_check{,_scene}.{hpp,cpp}`, `bench_scene.{hpp,cpp}`, `art/make_art.py` → `assets/art/tiles.png`
  (64 × 16: wall, water, plank, floor), `tests/check.nnhs`). The room is 25 × 15 cells of 16 px (one top screen):
  a `walls` collision layer on the default layer (the example's `bits::kWorld`), a `water` collision layer on
  `kWater`, one-way planks (tile 3, default layer) on a plain layer, a floor layer; tiles carry a `name` property.
  12 boxes on red / green / blue layers live in the game's own `Thing` array, read through
  `box_view(things, 12, &Thing::box, &Thing::layers)`. The room's grid for a mask is
  `collision_grid(mask & (kWorld | kWater))` (at most 4 masks on the map), outside Solid.
- **Modes and logs**: X cycles first / all / cast (`queries: mode first|all|cast`); A / B / L / R toggle the water,
  red, green, blue bits (`queries: mask 0x1D`); Y `queries: one-way ignored|blocks`; the first hit is logged whenever
  its line changes (`queries: first hit cell 6,9 solid fraction 0.5909 normal 0,-1`, `... box 6 green ...`,
  `queries: first hit none`), so a moving target logs a line per frame; a touch logs
  `under: cell 7,9 solid layers 0x2 tile 2 boxes 0` (the cell's kind on the unfiltered map, `collision_layers_at`,
  the topmost tile, the boxes under the point that pass the current mask). The HUD shows a shorter form.
- **Cast mode** casts a 12 × 12 box centred on the emitter against the boxes only (box casts against grids are a
  non-goal): the screenshot shows it flush against a box with the wall between ignored.
- **Self-check**: 4096 rays and 512 casts as planned, against a 24 × 16 grid of 16 × 12 cells at (−8.5, 4.25)
  (18 % solid, 10 % one-way), clipped (outside Empty) on even rays and walled (outside Solid) on odd ones, 64 boxes
  (every 13th without width, every 11th on no layers), points on the 1/16 lattice, exactly on grid lines and
  corners, off the lattice (/97, /89) and far away (±4000 px); a tenth of the rays vertical, horizontal, exactly
  diagonal through corners or of zero length. It counts what it reaches (`Stats`): 2839 grid hits, 2176 box hits,
  849 inside hits, 60 one-way hits, 3108 blocked sights, 391 cast hits. Golden `0x4ACAA177` (checkpoints
  `36C6AD9D`, `6A48E776`, `F9B05D2F`, `C8724E5B`), pc-debug and pc-release; ctest `queries_self_check_pc`. On the
  3DS `std::uint32_t` is `unsigned long`: the checksum's `mix` overloads got distinct names (`mix_float`, `mix_hit`)
  so `mix(1u)` isn't ambiguous there.
- **Bench** (PC simulator, Debug, `check.nnhs` run): `queries bench: rays avg 1.087 ms worst 1.937 ms (target < 2
  ms) PASS`, `points avg 3.019 ms, 0.0461 us per box (target <= 0.1 us) PASS`, `move unfiltered 0.98 us filtered
  1.04 us`; a second run 1.112 / 2.708 ms, 0.0452 µs, 0.87 / 0.81 µs. Unoptimised PC numbers: the console's are the
  ones that count (stage 10).
- **Screenshots read back** (`QUERIES_DEMO=first|all|cast|water|touch`, and the script's): the first hit flush on a
  wall's left face with its normal; all hits with the crossed cells shaded, the wall and three boxes marked; the cast
  box flush against a blue box; with the water bit off the ray goes over the pool to the target (`first hit none`);
  the touch marked on both screens with `cell 7,9 solid, layers 0x2` / `tile 2 "water"`.
- `nnh run queries --pc --no-build --script examples/queries/tests/check.nnhs` → `nnh: PASS (96 lines, frames
  1–1561)`, twice.

## Stage 8 refinements and results

- **Turrets** (`examples/platformer/turrets.{hpp,cpp}`, `TurretBook`): up to 4 turrets and 16 bullets; sight within a
  range box of ±192 × ±120 px by `line_of_sight` (one-way ignored) from the turret's centre to the player's; a shot
  when the cooldown is 0 while seen, then every 90 frames; 8 directions (`aim`: horizontal or vertical when the other
  axis is under 2/5 of it, else a diagonal of 1.375 px per axis per frame); bullets moved by
  `raycast(grid, bodies, pos, pos + vel, hit, {.mask = kBulletMask, .hit_from_inside = true})`, the bodies being the
  player (index 0) and the turrets in one struct view. **`hit_from_inside` is on**: both the player and the bullet
  move, and a player running into a bullet put it inside his box before the bullet's next step, which the default
  filter ignores (the first run counted no hit at all); this is the two-movers case of FR-13, and a start inside the
  player is a hit. A public `shoot(from, velocity)` lets the tests fire fast bullets. Tests:
  `tests/platformer/test_turrets.cpp` (sight through a gap, a wall hides, planks don't, out of range, the 90-frame
  period, 8 directions, a 100 px/frame bullet stops flush at a one-cell wall, a bullet falling on a plank stops on it,
  a hit and the flash, a player running into a bullet, bullets pass turrets, the state word).
- **Level 1**: turrets at cells (31, 14) and (40, 15), on the ground on both sides of the second pit (`make_level.py`
  `TURRETS`, `level1.tmj` regenerated: only the two objects added). `LevelPlay` reads objects of type `turret`,
  updates the book after the player (and the coins), publishes `PlayerHit{hits}` (the HUD shows "hits N"), draws the
  turrets (a red eye and the sight line while seeing), the bullets (3 × 3, a stuck one just outside its wall along
  the normal) and a red blink on the player while flashing. `ReplaySample::turrets` (= `TurretBook::state()`) is the
  checksum's seventh word, in the bonus level too (its book is empty). Two placements were tried first ((56, 15),
  then on the stone steps): their bullets all ended in the player, so no screenshot showed one stopped at a wall.
- **New replay lines** (`examples/platformer/tests/level.nnhs`, in the same commit): `60 … sum 523AEFC6`,
  `120 … 22B6BC70`, `180 … 19701BA9`, `240 … 26C330BF`, `300 … BF913379`, `360 … FEAD39C1`, `420 … 66CE9E1D`,
  `480 … 1FD5F55A`, `540 … 3C20AAFF`, `600 … ED6F8521`, `660 bonus … F183ED9C`, `720 … 37D753F0`,
  `780 … CF176580`, `840 … 01FDC889`, `replay: done 900 frames sum 96FE8943: 60 Hz PASS`. **The x, y and coins
  columns are unchanged on every line.** The turrets see the player from about frame 170, four bullets hit him in
  level 1 (`hits 4` on the HUD), and two stop on walls: at (560, 248) on the ground's right face (normal (1, 0)) at
  frame 309, and at (928, 248) on the stone steps' left face at frame 585.
- **Screenshot** (`PLATFORMER_DEMO=replay NNH_EXIT_AFTER_FRAMES=312`, read back): both turrets' sight lines to the
  player in mid-jump over the pit, and a bullet stopped flush on the right face of the ground at the pit's edge.
- T8.2/T8.4: ctest `platformer_replay_matches_script` passes; every example script passes with `--pc`
  (`input`, `animation`, `actions`, `tweens`, `data`, `camera`, `queries`, platformer `level` (76 lines) and `gate`:
  `bench: PASS 4 layers + 300 sprites, avg 16.7 ms, worst 16.9 ms, missed 0 of 600 frames`); `compare.sh`: every
  reference screenshot identical (level 1 at frame 90 too: the turrets are off screen); `hello` Release unchanged
  (PC text 130 732, 3DS text 201 188) with no query or `grid_cell_masked` symbol; `nnh_tests` 2011 tests, ctest
  pc-debug 264/264, pc-release 257/257, 3ds-release without a warning.

## Mutation testing

86 hand mutants (`build/ref-019/mutants.txt`, applied one at a time by `build/ref-019/mutate.py`: rebuild the test
program — `nnh_tests`, or `nnh_assetc_tests` for the converter —, run all of it, restore the file) of every new
collision file (layers, views, misuse checks, hit list, point queries, exact predicates, slab test, rays on boxes,
distance, box casts, the grid walk, rays on grids, cells crossed, grid + boxes and line of sight), the collision
header's `detects`, the tilemap's masked rule, slots, sets, loader, runtime area and `add_tile_layer`, the converter's
check, the coin book, the turrets and the replay's checksum word: **70/86 killed** on the first run (pc-debug).

- **Release-path** (2): `bad_layer` returning `kAllLayers` and the out-of-masks log not remembered once per map are
  only observable where asserts are compiled out; both are killed by the same tests in pc-release
  (`build/ref-019/mutants_release.txt`: 2/2).
- **Test gaps, closed** (6): a box with a NaN y passing the filter (`boxes_without_area_or_with_nan_fields_never_pass`
  got the case); the box cast's step-down to flush removed (`the_contact_stays_flush_when_the_subtraction_rounds_up`:
  37.3f − 0.2507f rounds to a float that overlaps by one ulp); a clipped walk not stopping when it leaves the grid
  (`a_clipped_walk_stops_when_it_leaves_the_grid`, through `GridWalk::steps_left()`); the turrets' range check on the
  left, the horizontal/diagonal threshold of `aim`, and line of sight not ignoring planks (the first version of
  `planks_never_hide_the_player_below_either` looked straight down a grid line, where nothing is ever crossed: moved
  2 px). All six are killed now (`mutants2.txt`, `mutants3.txt`).
- **Equivalent** (8): loosening the rays' bounding-box prefilter (it only skips work; two mutants that *tighten* it
  are killed); the clamp of a box hit's other coordinate and of a cast's contact position to their spans (defensive:
  rounding of in-range floats can't leave the span); the backward correction of the walk's start cell (`>` → `>=`:
  the forward correction takes it back); `start()` reporting "more cells" after a corner-only clip (the step guard
  stops the walk one step later); the clip choice for a horizontal ray with a one-way outside (cost only: such cells
  never block it); a corner hit's point through `along()` instead of the corner (equal on exact corners).

Final: **78/86 killed**, 8 equivalent, plus 2 tightened prefilter mutants and the re-runs killed; `nnh_tests` 2014
tests.

## Stage 9: Linux checks (left to the lead)

Not run in this session (VM 120 is the lead's). What to compare on GCC 13, GCC 14 and clang 18 (ASan/UBSan): `nnh_tests`,
`nnh_assetc_tests` and ctest pass; these tests hold their pinned values unchanged:
`collision_grid_regression_*` (T0.2: `0x698EB725`/`0xE419AD3C`, `0x852ED0F6`/`0xA6D13AF0`, `0x2E9EDC46`/`0x8761516F`,
`0x6EA3F8C6`/`0xB2975DBA`), `the_walk_equals_the_exact_reference_on_5000_rays_*` (zero mismatches) and
`the_queries_self_check_reaches_the_golden_checksum` (`0x4ACAA177`, checkpoints `36C6AD9D`, `6A48E776`, `F9B05D2F`,
`C8724E5B`); ctest `queries_self_check_pc` and `platformer_replay_matches_script`. libFuzzer: `fuzz_tilemap` (its
corpus can start from `map_file::layered()`, the seed with `collision_layers`) and `fuzz_assetc_map` (seed: `kMapSeed`
'\0' `kSetSeed` of `tools/assetc/tests/test_fuzz_assetc.cpp`, which now carries `collision_layers`), 10 minutes each.

## Stage 10: console run (prepared)

T10.1 on the PC: `scripts/dkp.sh cmake --build --preset 3ds-release` builds without a warning (every stage end), and
so do the Development (`3ds-dev`) builds of `queries`, `platformer` and `hello` (3ds-dev `hello.elf` text 217 992, as in
stage 0; `queries.elf` text 289 568);
`ray_distance.o` on 3ds-release is `vsqrt.f32` with no call, and no query object has a fused `vfma`/`vfms`
(`build/ref-019/fp_check.sh`; VFPv2's chained `vmla` is allowed); 3ds-release `hello.elf` text 201 188 / data 28 352 /
bss 106 680, as in stage 0.

T10.2, on an Old 3DS, nobody touching it after Y. Three runs, each: open the Homebrew Launcher, press **Y**, then on the
PC (`nnh` builds the Development `.3dsx`, sends it and plays the script; logs and screenshots in `build/logs/`):

1. `build/pc-debug/tools/nnh/nnh run queries --script examples/queries/tests/check.nnhs` (about 30 s). Expected, as on
   the PC: `queries: first hit cell 6,9 solid fraction 0.5909 normal 0,-1` (after the stick moves), `queries: first
   hit none` (water bit off), the same first-hit line again (water back on), `queries: mode all`, `queries: mode
   cast`, `queries: mode first`, `queries: one-way ignored`, `queries: one-way blocks`, `under: cell 7,9 solid layers
   0x2 tile 2 boxes 0`, `under: cell 10,7 empty layers 0x0 tile 4 boxes 1`, `under: cell 15,5 one-way layers 0x1 tile
   3 boxes 0`, `queries: self-check started`, `queries: 1024 sum 36C6AD9D`, `queries: 2048 sum 6A48E776`,
   `queries: 3072 sum F9B05D2F`, `queries: 4096 sum C8724E5B`, **`queries: PASS 4ACAA177`**, then `queries bench:
   rays avg X ms worst Y ms (target < 2 ms) PASS`, `queries bench: points avg X ms, Z us per box (target <= 0.1 us)
   PASS`, `queries bench: move unfiltered X us filtered Y us`; `nnh: PASS (96 lines, …)`. Record X, Y, Z and the move
   numbers here and in `docs/api/collision.md#limits-and-costs`. Then read the screenshots `first`, `pool`, `water`,
   `all`, `cast`, `touch` (top and bottom): the same pictures as the PC's (`build/logs/queries-*` from a `--pc` run).
   If the rays line says FAIL, apply risk 1's mitigations in order.
2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` (about 40 s). Expected:
   `replay: 60 level1 x 121 y 242 coins 1 sum 523AEFC6`, `120 … sum 22B6BC70`, `180 … sum 19701BA9`,
   `240 … sum 26C330BF`, `300 … sum BF913379`, `360 … sum FEAD39C1`, `420 … sum 66CE9E1D`, `480 … sum 1FD5F55A`,
   `540 … sum 3C20AAFF`, `600 … sum ED6F8521`, `660 bonus … sum F183ED9C`, `720 … sum 37D753F0`,
   `780 … sum CF176580`, `840 … sum 01FDC889`, `replay: done 900 frames sum 96FE8943: 60 Hz PASS`; `nnh: PASS (76
   lines, …)`. A mismatch with the self-check of run 1 passing would be a game bug, not an engine one (risk 8).
3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs` (about 2.5 min).
   Expected: `bench: PASS 4 layers + 300 sprites, …` (spec 012's gate still passes); `nnh: PASS`.

## Stage 11 results

- Header cost (`g++ -std=c++20 -fsyntax-only` of a one-line TU, 3 runs): `<nnh/collision.hpp>` 691 preprocessed lines
  (stage 0: 151; `<cstdint>` is most of the growth), 0.28–0.34 s (0.27–0.32 s); `<nnh/tilemap.hpp>` 8477 lines
  (8331), 0.42–0.46 s (0.41–0.45 s).
- Build times: edit `hello` → simulator 1.53, 1.51, 1.56 s (stage 0: 1.49–1.94); → `.3dsx` 2.82 s (a first run that
  reconfigured 6.5 s; stage 0: 2.71–3.05); clean `nnh_core` (pc-debug, parallel) 5.78, 5.64, 5.65 s (5.26–5.34: 16 new
  collision files and 5 tilemap files).
- `hello` sizes unchanged in every preset: pc-release 130 732 / 22 020 / 211 696, pc-debug 198 816 / 26 756 / 214 352,
  3ds-release 201 188 / 28 352 / 106 680, 3ds-dev 217 992 / 32 872 / 109 288; no query, `grid_cell_masked` or
  collision-layer symbol. The queries a game uses cost about 14 KB of 3DS text when it uses all of them
  (`queries.elf`, Release).
- Final: `nnh_tests` 2014 tests, `nnh_assetc_tests` 265; ctest pc-debug 264/264, pc-release 257/257; 3ds-release and
  3ds-dev without a warning; `docs_tool.py check` 0 broken links.
