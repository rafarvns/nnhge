# tilemap — maps of tiles, objects and properties

Header: `<nnh/tilemap.hpp>` · Uses: [collision.md](collision.md), [texture.md](texture.md), [result.md](result.md),
[memory.md](memory.md) · Spec: [012](../../specs/012-tilemaps-and-collision/spec.md) (FR-0, FR-2, FR-4…FR-7, FR-11)

> Status: spec 012 **stage 3** — maps are **built in code**, queried, edited, collided against and **drawn**
> (`gfx::draw_tilemap`, PC and 3DS; the Old 3DS benchmark gate is pending). Loading maps converted from Tiled
> (`load_tilemap`, stage 4) and animated tiles (stage 6) come later; they will use the same `Tilemap` object, so
> code written against this page keeps working. Example: [`examples/platformer`](../../examples/platformer/main.cpp)
> (a level built in code, a tile benchmark, a flip test).

A `Tilemap` is a grid of cells in **tile layers**, cut from **tilesets** (textures), plus **object layers**
(rectangles and points with a name and a type) and **properties** (named bool/int/float/string values on the map,
its layers, its tiles and its objects). It also hands out a `collision::Grid`, so `collision::move` works on it
directly. Tilemaps are opt-in: they need no tags, entities or scenes, and a game that never creates one pays
nothing.

## Rules that hold everywhere

- **One block, fixed capacities.** `Tilemap::create` allocates once (memory tag `tilemap`, heap). Every capacity —
  layers, tilesets, tile ids, objects, properties, string bytes — is fixed then; adding past one returns
  `LimitReached` and never grows anything. Queries and edits never allocate and are safe per frame.
- **Coordinates.** Cell `(0, 0)` starts at world `(0, 0)`; cell `(x, y)` covers
  `[x·tile_width, (x+1)·tile_width) × [y·tile_height, (y+1)·tile_height)` (half-open, as in collision). Tiles may be
  8–64 px per side, square or not. Layer offsets and parallax affect **drawing only**, not cells or collision.
- **Move-only RAII.** A `Tilemap` frees its block when destroyed or `reset()`. Moving it keeps the block where it is,
  so a collision grid taken before the move stays valid. Views (`Properties`, `MapObject` strings) and grids are
  valid while the map lives.
- **Misuse.** Functions returning `Result` report `InvalidArgument` / `LimitReached`. `set_tile` /
  `set_tile_collision` with an invalid layer, an unknown tile id or a diagonal flip on non-square tiles assert in
  Debug/Development and do nothing in Release. Every call on an empty (default or reset) map is harmless.

## Building a map

```cpp
#include <nnh/tilemap.hpp>

auto made = nnh::Tilemap::create({.width = 40, .height = 15, .max_tile_layers = 2});
if (!made) { NNH_LOG_ERROR(game, "%s", made.error().message); return; }
nnh::Tilemap map = std::move(made.value());

const int first = map.add_tileset({.texture = &tiles}).value();        // ids first .. first + n - 1
const int back = map.add_tile_layer({.name = "back"}).value();
const int ground = map.add_tile_layer({.name = "ground", .collision = true}).value();

const auto brick = nnh::Tile{.id = static_cast<std::uint16_t>(first)};
map.fill(ground, 0, 14, 40, 1, brick);                                   // the floor
map.set_tile(back, 3, 10, {.id = static_cast<std::uint16_t>(first + 1), .flip_x = true});
map.set_tile_collision(first + 2, nnh::collision::Cell::OneWay);         // a plank
```

| `TilemapDesc` field | Default | Limit |
|---------------------|---------|-------|
| `width`, `height` (cells) | — | 1–1024 each |
| `tile_width`, `tile_height` (px) | 16 | 8–64 each |
| `max_tile_layers` | 4 | 16, and `width·height·max_tile_layers` ≤ 4 Mi cells |
| `max_tilesets` | 2 | 8 |
| `max_tile_ids` (over all tilesets) | 256 | 8191 (`kMaxTileId`) |
| `max_object_layers` | 1 | 16 |
| `max_objects` | 64 | 4096 |
| `max_properties` (all owners) | 64 | 16 384 |
| `string_bytes` (names, string values) | 2048 | 64 KiB; each string ≤ 255 bytes; equal strings are stored once; byte 0 is `""` |

Errors of `create`: `InvalidArgument` (size < 1, tile size outside 8–64, negative capacities), `LimitReached`
(over a limit above), `OutOfMemory`.

### Tilesets

`add_tileset({.texture, .margin, .spacing, .tile_count, .name})` cuts the texture into tiles of the **map's** tile
size, left to right then top to bottom, skipping `margin` pixels around and `spacing` between tiles (as Tiled),
and returns the id of its first tile; the next tileset continues after it. `tile_count = 0` takes every tile that
fits (`(size − 2·margin + spacing) / (tile + spacing)` per side). The map stores each tile's source position, so
drawing needs no division.

The texture is **borrowed**: keep it alive while the map is drawn. The map remembers the texture's handle, not the
object; if the texture is destroyed, tiles, collision and properties keep working and drawing skips that tileset
(and asserts). A texture loaded later into the same slot is not mistaken for it.

Errors: `InvalidArgument` (no or unloaded texture, nothing fits, `tile_count` more than fits, negative numbers, name
over 255 bytes), `LimitReached` (`max_tilesets`, `max_tile_ids` or `string_bytes`).

### Tile layers

`add_tile_layer({.name, .offset, .parallax, .opacity, .visible, .collision})` adds an empty layer on top and returns
its index. Settings change later with `set_layer_offset/parallax/opacity/visible/collision` and are read with
`layer_offset()` … `layer_collision()`; `find_tile_layer(name)` (−1 if none), `tile_layer_name(i)`. Offset,
parallax, opacity and visibility affect [drawing](#drawing) only; `set_parallax_origin(Vec2)` /
`parallax_origin()` set the map's parallax origin (Tiled's map property of the same name).

## Tiles

| Function | Does |
|----------|------|
| `Tile tile(layer, x, y)` | the tile at a cell; empty outside the map or for an invalid layer |
| `Tile tile_at(layer, Vec2 world)` | the tile under a world position |
| `void set_tile(layer, x, y, Tile)` | changes one cell; seen by the next draw and collision query; outside the map: ignored |
| `void fill(layer, x, y, w, h, Tile)` | `set_tile` over a block, clipped to the map |
| `Rect cell_rect(x, y)` | the world rectangle of any cell (also outside) |
| `bool world_to_cell(Vec2, int& x, int& y)` | floor of world / tile size (negative positions go down: −0.5 → −1); true inside the map. `x`, `y` are set even outside (clamped to ±2^30; 0 for NaN) |

`Tile{id, flip_x, flip_y, flip_diagonal}`: id 0 is empty (its flips are dropped); flips follow Tiled (diagonal swaps
x and y and is applied first) and need square tiles for the diagonal one. In memory a cell is 16 bits: id in bits
0–12, diagonal 13, flip y 14, flip x 15.

## Drawing

```cpp
nnh::init({.title = "My Game", .gfx = {.max_tiles = 2048}});        // 0 by default: no tile drawing
...
nnh::gfx::set_camera(nnh::Screen::Top, {{camera_x, 0}});
nnh::gfx::draw_tilemap(nnh::Screen::Top, map, {.map_layer = back});                 // draw layer 0
nnh::gfx::draw(nnh::Screen::Top, hero, hero_pos, {.layer = 1});                     // the player
nnh::gfx::draw_tilemap(nnh::Screen::Top, map, {.map_layer = front, .layer = 2});    // over the player
```

`gfx::draw_tilemap(screen, map, options)` draws tile layers at the end of the frame, sorted with sprites by draw
layer and then call order (as [gfx.md](gfx.md#order-layers)), so sprites can sit between map layers.

| `TilemapDrawOptions` field | Default | Meaning |
|----------------------------|---------|---------|
| `map_layer` | −1 | one tile layer, or −1 = every visible tile layer, in map order (an unknown index asserts) |
| `layer` | 0 | draw layer 0–15, like sprites |
| `opacity` | 1 | 0–1, times each map layer's opacity |
| `offset` | (0, 0) | added to each map layer's offset (pixels) |
| `ignore_camera` | false | screen pixels (HUD maps); parallax then uses camera (0, 0) |

- **When it reads the map.** The call records one batch per (map layer, tileset that layer uses); the cells are read
  when the frame ends, with the camera of that moment. A `set_tile` after the call is drawn in the same frame; a
  map destroyed after the call draws nothing (moving the `Tilemap` object is fine).
- **Only what the camera sees** is expanded: a layer of 16×16 tiles costs at most 26 × 16 = 416 tiles on the top
  screen, whatever the map's size. Empty cells, hidden layers, empty layers and opacity 0 cost nothing.
- **Parallax** follows Tiled's preview, which scrolls around the view's centre: a layer's position on screen is
  `offset + (camera + screen_size / 2 − parallax_origin) · (1 − parallax) − camera`. Parallax 1 scrolls with the
  camera, 0 stays fixed on screen, 0.5 moves at half speed; at the parallax origin every factor lines up with the
  map (set it to the view centre of your first screen, e.g. `(200, 120)` on the top screen with camera (0, 0)).
- **No seams.** The layer's position is rounded to whole pixels once (halves up, like sprites) and every tile is an
  exact multiple of the tile size from it, with nearest sampling: PC and 3DS show the same pixels.
- **Flips** follow Tiled (diagonal first, then x and y); the PC backend is checked pixel by pixel, and the
  platformer example's flip test shows all 8.
- **Costs and limits.** `AppConfig::gfx.max_tiles` caps tiles per frame on every system ([gfx.md](gfx.md#capacities)):
  past it tiles are dropped (assert in Debug/Development, a warning per frame in Release), and `max_tiles = 0`
  asserts with the fix. Each batch also takes one of `max_draws`; at most 64 batches per frame. No allocation.
  A tileset whose texture was destroyed is skipped (assert). `gfx::stats()` has `tiles_last_frame`, `max_tiles`,
  `dropped_tiles`.

## Collision

```cpp
map.set_tile_collision(spike_id, nnh::collision::Cell::Solid);      // Tiled: tile property `solid`
collision::Grid grid = map.collision_grid();
grid.outside = collision::Cell::Solid;                              // wall the level in (default: Empty)
const auto r = collision::move(grid, player, velocity * dt, {.drop_through = down && jump});
```

A cell is **Solid** when a layer with `collision` set has any tile there, or any layer's tile there is marked solid;
otherwise **OneWay** when a tile there is marked one-way; otherwise **Empty** (also outside the map). Only layers that
can contribute are visited: collision layers and layers that received a marked tile (a bit mask kept up to date by
`set_tile`, `fill`, `set_layer_collision` and `set_tile_collision`; it may stay wider than needed, which only costs
time). `set_tile_collision` on tiles already placed scans the layers that use that tileset once: call it while
building. `collision_at(x, y)` answers for one cell; `collision_grid()` gives a grid (origin 0, tile-sized cells,
`cell` callback over the map) that reads the map live, so a `set_tile` is seen at once. The tilemap never calls
collision itself: it only describes cells.

## Objects

```cpp
const int things = map.add_object_layer("things").value();
const int coin = map.add_object(things, {.name = "coin", .type = "coin", .bounds = {64, 96, 16, 16}}).value();
map.add_object(things, {.name = "spawn", .shape = nnh::ObjectShape::Point, .bounds = {24, 200}});
(void)map.set_int(nnh::PropertyOwner::object(coin), "value", 10);

int coins[32];
const int n = map.find_objects("coin", coins, 32);          // total found (may exceed 32)
const nnh::MapObject spawn = map.object(map.find_object("spawn"));
```

Objects keep their creation order (the index `add_object` returns is stable) and their object layer. `id` is Tiled's
object id or the one given in `ObjectDesc::id`; 0 means "highest so far + 1". A `Point` has `w = h = 0`.
`object(i)` returns an object with id 0 for an invalid index. Errors of `add_object`: `InvalidArgument` (unknown
layer, negative size or id, long names), `LimitReached` (`max_objects`, `string_bytes`).

## Properties

```cpp
(void)map.set_string({}, "music", "music/level1");                    // the map's own
(void)map.set_int(nnh::PropertyOwner::tile(spike_id), "damage", 2);
const char* music = map.properties().get_string("music", "music/default");
const int damage = map.properties(nnh::PropertyOwner::tile(id)).get_int("damage");
```

Owners: `PropertyOwner::map()`, `tile_layer(i)`, `object_layer(i)`, `tile(id)`, `object(i)`. Setting an existing name
replaces its value and type. Getters return the fallback when the name is missing **or holds another type**, except
`get_float`, which also reads ints. `count()` and `name(i)` list them. A `Properties` view stays valid while the map
lives and until properties are set again on it (setting may move them): take a new view after setting. Errors of
the setters: `InvalidArgument` (unknown owner, empty name, strings over 255 bytes), `LimitReached`
(`max_properties`, `string_bytes`, or too many tiles with properties — at most `min(max_properties, max_tile_ids)`).

## Memory

The block is exactly (sections 4-byte aligned; `ids = max_tile_ids + 1`, `tp = min(max_properties, max_tile_ids)`):

```
bytes = 128 + 24·max_tilesets + 4·ids + align4(ids) + align4(6·tp)
      + 40·max_tile_layers + align4(2·width·height·max_tile_layers)
      + 12·max_object_layers + 36·max_objects + 12·max_properties + align4(string_bytes)
```

Cells dominate: 2 bytes per cell per layer **of capacity** (layers you never add still cost their cells).

| Map | Bytes |
|-----|-------|
| 40×15, default desc | 11 940 (≈ 11.7 KiB) |
| 240×30, 3 layers, 1 tileset of 128 ids, 40 objects, 60 properties, 1 KiB strings | 47 676 (≈ 46.6 KiB) |
| 512×512, 4 layers, 2 tilesets, 1024 ids, other defaults | 2 108 132 (≈ 2.01 MiB) |
| Most cells allowed: 4 Mi (e.g. 4 × 1024×1024) | > 8 MiB |

Tileset textures are separate (tag `textures`, `Linear`): a 256×256 RGBA8 sheet is 256 KiB. The Old 3DS has about
64 MB for the whole game, so keep maps to what a level needs.

## Pitfalls

- Capacities are fixed: size `max_*` for everything the level will ever hold, including objects or properties added
  while playing.
- `set_tile` with a tile id that no tileset has yet is a bug (assert): add tilesets first.
- A `collision` layer makes **every** non-empty tile solid, including decorative ones; mark individual tiles with
  `set_tile_collision` instead when only some should block (a one-way plank on a `collision` layer is solid).
- `draw_tilemap` needs `AppConfig::gfx.max_tiles` sized for every layer you draw on both screens (≈ 416 per full
  16×16 top-screen layer, 336 on the bottom screen); check `gfx::stats().tiles_last_frame` or the debug overlay.
- A parallax layer with the default parallax origin (0, 0) is shifted at camera (0, 0) (Tiled does the same): set
  the origin to your first view's centre so layers line up where the level starts.
