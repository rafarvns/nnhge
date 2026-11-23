# tilemap — maps of tiles, objects and properties

Header: `<nnh/tilemap.hpp>` · Uses: [collision.md](collision.md), [texture.md](texture.md), [result.md](result.md),
[memory.md](memory.md), [assets.md](assets.md) · Spec: [012](../../specs/012-tilemaps-and-collision/spec.md)
(FR-0…FR-7, FR-11)

> Status: **implemented** (spec 012) — maps are **built in code**, queried, edited, collided against and **drawn**
> (`gfx::draw_tilemap`, PC and 3DS; an Old 3DS XL draws 4 full 16×16 layers + 300 sprites at 60 Hz), with
> **animated tiles**, and **made in Tiled**: the build converts `.tmj` files into `.nnhmap` assets (`nnh-assetc`)
> that `load_tilemap` loads. Guide: [Levels in Tiled](../guides/levels.md). Example:
> [`examples/platformer`](../../examples/platformer/main.cpp): level 1 made in Tiled
> (`assets/levels/level1.tmj` + `assets/tiles/world.tsj`, written by `art/make_level.py`; parallax backdrops, a
> `collision` layer, one-way planks, animated water, coins, a spawn point and a door as objects, `title`/`music`
> properties), a bonus level built in code, a replay that checks PC and console play the same frames, a tile
> benchmark and a flip test.

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
| `max_animations` (animated tiles) | 0 | 256 |
| `max_animation_frames` (over all animations) | 0 | 4096; each animation 1–32 frames |

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

### Animated tiles

```cpp
auto made = nnh::Tilemap::create({.width = 100, .height = 15, .max_animations = 4, .max_animation_frames = 16});
...
const nnh::Tilemap::AnimationFrame ripple[] = {{.tile_id = water, .ms = 400}, {.tile_id = water + 1, .ms = 400}};
(void)map.set_tile_animation(water, ripple, 2);     // every `water` cell now ripples
```

`set_tile_animation(tile_id, frames, count)` animates a **tile id** (Tiled's tile animations): wherever that tile is
placed, drawing shows the frames in turn, each for its `ms`, looping; every cell of it shows the same frame. Frames
are tiles **of the same tileset** (1–65535 ms each, 1–32 frames); the animated tile need not be one of them. Calling
it again replaces the animation, `count = 0` removes it. Only drawing changes: `tile()`, `collision_at()`,
`tile_collision()` and the tile's properties are those of the tile id itself.

| Function | Does |
|----------|------|
| `Result<void> set_tile_animation(tile_id, frames, count)` | sets, replaces or (`count` 0) removes a tile's animation; never allocates (build time: it shifts the frame table) |
| `int tile_animation(tile_id, frames, capacity)` | copies up to `capacity` frames, returns how many there are (0: not animated) |
| `int animated_tile_id(tile_id, Duration time)` | the tile shown at `time` (negative counts as 0), or `tile_id` when not animated |

Errors of `set_tile_animation`: `InvalidArgument` (unknown tile id, a frame of another tileset, unknown or 0 ms, null
frames, negative count), `LimitReached` (over 32 frames, `max_animations` or `max_animation_frames` full). A loaded
map has room for its own animations only: it can replace one with as many frames or fewer, or remove it.

**The clock** is `TilemapDrawOptions::animation_time` ([drawing](#drawing)). By default it is the frame count at
60 Hz (`frame_count() · 1000 / 60` ms), so the simulator and the console show the same frame on the same frame
number; pass your own `Duration` to pause or speed animations. Cost per frame: each batch resolves the current frame
of its tileset's animations once (≤ 256 short loops), then one flag test per tile; animated tiles look their frame up
in a sorted table (a binary search over ≤ 256 entries). No per-cell state, no allocation.

## Loading a converted map

```cpp
auto loaded = nnh::load_tilemap("levels/level1");                    // assets/levels/level1.nnhmap
if (!loaded) { NNH_LOG_ERROR(game, "%s", loaded.error().message); return; }
nnh::Tilemap level = std::move(loaded.value());

// Room to add things in code later (a file has no spare capacity of its own):
auto roomy = nnh::load_tilemap("levels/level2", {.extra_objects = 8, .extra_properties = 8,
                                                 .extra_string_bytes = 128});
```

`load_tilemap(path, options)` reads the asset `<path>.nnhmap` (the path has no extension, like `load_texture`), a
map the build converted from a Tiled `.tmj` ([below](#maps-made-in-tiled)). The result is a `Tilemap` like one
built in code: every query, edit, `collision_grid()` and `draw_tilemap` works on it (a unit test builds the same map
both ways and compares every query: `tests/tilemap/test_code_vs_file.cpp`).

- **Checked before use.** The whole file is validated first: header, format version, size, CRC-32, every limit of
  [the table above](#building-a-map), section offsets, strings (terminated, ≤ 255 bytes), tilesets against their
  image size, every cell's tile id, the animation table (sorted, frames in the animated tile's tileset, 1–32 frames
  of ≥ 1 ms), objects, property types and ranges. A damaged file is an error, never a crash
  (the validator is fuzzed: [security.md](../architecture/security.md#bounded-parsers-and-fuzzing-spec-002)).
- **Tileset images** are loaded like `load_texture(name)`, with the image names the file carries (asset paths
  without `.png`). Two tilesets with the same image share one texture. The map **owns** these textures: they are
  freed with it (`reset()` or the destructor); keep the map alive while it is drawn. Each counts in
  `AppConfig::gfx.max_textures`.
- **Runtime data is rebuilt**: the collision-layer mask is worked out from the layers and tiles, so
  `collision_at` and `collision::move` behave as if the map had been built in code.
- **Extra room** (`LoadTilemapOptions`): `extra_tile_layers`, `extra_objects`, `extra_properties` (each can also give
  one more tile its first property) and `extra_string_bytes`, all ≥ 0. Object layers and tilesets can't be added to
  a loaded map. Without options a loaded map is full: `add_*` and new properties return `LimitReached` (replacing a
  property's value still works).
- **Load time only.** The file is read into a temporary buffer (tag `assets`), checked (one pass, plus a bitwise
  CRC — about 8 shifts per byte), copied into the map's block and freed before the images load, so the peak is
  file + map.

| Error | When |
|-------|------|
| `InvalidArgument` | null or bad path, a negative extra |
| `NotFound` | no such map; a tileset image missing (the message names the image) |
| `Corrupted` | damaged or truncated file (CRC, sizes, offsets, content); a tileset image of another size than when the map was converted |
| `Unsupported` | before `nnh::init()`; a map made by a newer engine (format version) |
| `LimitReached` | over a map limit (also with the extras); `AppConfig::gfx.max_textures` full |
| `OutOfMemory` | no memory for the map |

The engine logs the full reason on the `tilemap` channel (`load_tilemap("levels/level1"): ...`); the error message
is short. The binary format is `engine/core/src/tilemap/map_format.hpp`: the file *is* the map block, a 128-byte
header and fixed-size sections, little-endian.

### Maps made in Tiled

Put the map in the game's `ASSETS` folder as a **`.tmj`** (Tiled's JSON map format) with its tilesets (`.tsj` or
embedded) and their PNG images. The build converts `levels/level1.tmj` into the asset `levels/level1.nnhmap` with
`nnh-assetc` ([cmake.md](../tech/cmake.md)); `.tsj`, `.tiled-project` and `.tiled-session` files are read but not
packed. A map is converted again only when it, a tileset or an image it uses, or the converter changes.

| In Tiled | In the `Tilemap` |
|----------|------------------|
| map size, tile size (8–64 px), parallax origin, map properties | `width()` …, `parallax_origin()`, `properties()` |
| tilesets in first-gid order (gaps in gids are fine) | tilesets with consecutive ids from 1: gid `g` of a tileset with first gid `f` is id `first + g − f` |
| tile property `solid` / `one_way` (bool) | `tile_collision(id)` = `Solid` / `OneWay` (not kept as properties) |
| other tile properties | `properties(PropertyOwner::tile(id))` |
| tile animation (frames of the same tileset, durations in ms) | `tile_animation(id, ...)`; drawn animated |
| tile layers in order: name, offset, parallax, opacity, visible, flips | tile layers in the same order |
| layer property `collision` (bool) | `layer_collision(layer)` (not kept as a property) |
| layer or tile property `collision_layers` (int, ≠ 0: a flag enum saved as a number) | `layer_collision_layers(layer)` / `tile_collision_layers(id)` (also kept as a property) — see [collision layers](#collision-layers) |
| object layers; rectangles and points with id, name, type (or class), properties | object layers and objects in the same order |
| property types bool, int, float, string; `color` (`#AARRGGBB`), `file` (path as written), `object` (its id) | bool, int, float, string, string, string, int |

The build **refuses**, naming the file and the reason (`level1.tmj: infinite maps aren't supported: turn off Map >
Map Properties > Infinite`): isometric, staggered and hexagonal maps; infinite maps; image and group layers;
compressed layer data (save the layer format as **CSV**; uncompressed base64 also works); XML tilesets (`.tsx`);
image-collection tilesets, wang sets, tile offsets; animations with more than 32 frames, a frame outside its
tileset or of 0 ms (more than 256 animated tiles or 4096 frames in a map are over the limits); tilesets whose tile size is not
the map's; images that are not a PNG inside the assets folder, whose size differs from what Tiled recorded or that
are not textures (power-of-two sides from 8 to 1024); a tileset's columns or tile count that don't match its image
(NNHGE keeps the margin on every side of the image); ellipse, polygon, polyline, text and tile objects, templates
and rotated objects; `class` properties; layer tint colours; diagonal flips on non-square tiles; a
`collision_layers` property that isn't an integer or is 0 (`level1.tmj: layer "water": property "collision_layers"
must not be 0: untick collision instead`); strings over 255
bytes and anything over the [limits](#building-a-map). The converter checks the file it writes with the same
validator `load_tilemap` runs, so a converted map always passes the loader's checks.

## Tiles

| Function | Does |
|----------|------|
| `Tile tile(layer, x, y)` | the tile at a cell; empty outside the map or for an invalid layer |
| `Tile tile_at(layer, Vec2 world)` | the tile under a world position |
| `void set_tile(layer, x, y, Tile)` | changes one cell; seen by the next draw and collision query; outside the map: ignored |
| `void fill(layer, x, y, w, h, Tile)` | `set_tile` over a block, clipped to the map |
| `Rect cell_rect(x, y)` | the world rectangle of any cell (also outside) |
| `Rect bounds()` | the map's world rectangle `{0, 0, width() × tile_width(), height() × tile_height()}` (`{}` for an empty map): camera limits in one line, `rig.set_limits(map.bounds())` ([camera.md](camera.md#limits)) |
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
| `animation_time` | −1 ns | [animated tiles](#animated-tiles)' clock; negative = `frame_count()` at 60 Hz |

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
- **Measured on an Old 3DS XL** (2026-11-21, Development build, the platformer's bench and `tests/gate.nnhs`):
  4 full layers of 16×16 tiles on the top screen + 300 sprites run at avg 16.7 ms, worst 17.4 ms, 0 missed frames
  of 600. The limits at 60 Hz: **6 layers** with 300 sprites (7 miss frames), **600 sprites** with 4 layers (Release
  is expected to be the same or faster; not measured separately).

### Zoom

With a camera zoom (`Camera2D::zoom`, 0.5–4, [gfx.md](gfx.md#zoom); decision D33) tile layers stay seamless: the
layer's origin is rounded once (`round((world origin − camera) × zoom)`) and every tile's **edges** are rounded from
it, `edge(i) = origin + round(i × tile × zoom)` (computed in 16.16 fixed point, the same on every system), so tiles
never gap or overlap. A tile is `floor(tile × zoom)` pixels or one more; at integer zooms (2, 3, 4) every tile is
exactly `tile × zoom` and each texel an exact square, at fractional zooms texel sizes vary by one pixel. A parallax-1
layer stays on sprites at the same world position (exactly at integer zooms, within one pixel otherwise). Parallax
uses the zoomed view's centre (`camera + screen_size / zoom / 2`), so a fixed layer (parallax 0) zooms about the
view's centre too.

The visible cells follow the zoomed view: zoom 0.5 shows about four times the tiles of zoom 1 (a full 16×16 layer on
the top screen: 51 × 31 = 1581 tiles), zoom 2 about a quarter (at most 14 × 9). Size `AppConfig::gfx.max_tiles` for the smallest
zoom your game uses. A layer larger than the engine's 1600-tile scratch (only below zoom 1) is drawn in several
calls, row by row, in order; nothing else changes. How many layers fit at 60 Hz at zooms 0.5 and 2 on an Old 3DS:
measured by `examples/platformer/tests/zoom_gate.nnhs` (B in the platformer's bench cycles the zoom): at zoom 0.5, **2
full 16×16 layers** with 300 sprites (4 layers don't fit); at zoom 2, **12 layers** (the most the bench has) with 300
sprites, or 900 sprites with 4 layers; zoom 1 is unchanged (6 layers / 600 sprites). Details in
[plan 018, stage 9](../../specs/018-camera-helpers/plan.md). At zoom 0.5 the 3DS and the PC pick different texel
columns when minifying (no gaps; thin tile borders may show in fragments on the console).

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

### Collision layers

Spec 019 gives a map's collision [layers](collision.md#layers-and-masks), so one map serves different kinds of movers
(water that stops boats but not fish, planks only the player stands on):

```cpp
constexpr nnh::collision::Layers kWorld = nnh::collision::kDefaultLayer, kBoat = nnh::collision::layer(1);
const int water = map.add_tile_layer({.name = "water", .collision = true, .collision_layers = kBoat}).value();
map.set_tile_collision_layers(plank_id, kWorld);              // a one-way plank on the world layer only
const collision::Grid boat_grid = map.collision_grid(kBoat);  // the water is solid here...
const collision::Grid fish_grid = map.collision_grid(kWorld); // ...and not here
```

| Function | Does |
|----------|------|
| `set_layer_collision_layers(layer, set)` / `layer_collision_layers(layer)` | the set a `collision` tile layer's tiles are solid on (`TileLayerDesc::collision_layers` at creation); `kNoLayers` for an invalid layer |
| `set_tile_collision_layers(id, set)` / `tile_collision_layers(id)` | the set a solid or one-way tile is solid / one-way on; `kNoLayers` for an unknown id |
| `collision_layers_at(x, y)` | the union of the sets of every contribution to the cell (`collision` layers with a tile there, solid and one-way tiles there); `kNoLayers` for an empty cell or outside |
| `collision_grid(mask)` | a grid seeing only the contributions on `mask`'s layers |

- **Every set starts on `kDefaultLayer`**, so maps without layers and `collision_grid()` behave exactly as before (a
  regression test pins it). A set is never 0: `add_tile_layer` refuses `collision_layers = kNoLayers`
  (InvalidArgument) and the setters assert and keep the old set (untick `collision` instead).
- **`collision_grid(mask)`**: a cell is **Solid** when a `collision` layer with a tile there, or a solid tile there,
  is on a layer of `mask`; else **OneWay** when a one-way tile there is; else **Empty**. It reads the map live like
  `collision_grid()` (edits and set changes are seen at once) and costs one more mask test per contribution. With
  `kAllLayers` it **is** `collision_grid()`.
- **8 masks per map.** The first call with a new mask registers it in the map (a const method writing the map's
  runtime area: single-threaded, as the whole engine); a grid handed out earlier stays valid (slots are never
  reused). A 9th distinct mask asserts; in Release it logs one error per map and returns the unfiltered grid (walls
  stay walls). Masks are game constants (player, enemy, line of sight): building them from bits at run time can run
  out.
- **From Tiled**: an int property `collision_layers` on a tile layer or a tile. Make it a custom enum with "allow
  multiple values" saved as a number for a checkbox per layer ([levels guide](../guides/levels.md#collision-layers));
  Tiled sets bits 0–30. The converter refuses a non-integer or 0 value; a loaded map whose file holds one anyway
  keeps the default set with one warning. `collision_layers` on a layer without `collision`, or on a tile without
  `solid`/`one_way`, is accepted and does nothing.

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
      + 8·max_animations + 4·max_animation_frames
      + 40·max_tile_layers + align4(2·width·height·max_tile_layers)
      + 12·max_object_layers + 36·max_objects + 12·max_properties + align4(string_bytes)
```

plus, after those bytes, a **runtime area** of `132 + 4·ids` bytes (collision layer sets and filtered-grid slots,
spec 019, never written to a file). Cells dominate: 2 bytes per cell per layer **of capacity** (layers you never add
still cost their cells).

A **loaded** map's block is the `.nnhmap` file's size (its capacities are what it holds) plus the runtime area plus its
`LoadTilemapOptions`: 40 + 2·width·height bytes per extra tile layer, 36 per extra object, 12 per extra property (+ 6
per extra tile that may get properties, up to the tile ids) and the extra string bytes, each section 4-byte aligned.

| Map | Bytes |
|-----|-------|
| 40×15, default desc | 13 100 (≈ 12.8 KiB; 11 940 + the runtime area's 1160) |
| 240×30, 3 layers, 1 tileset of 128 ids, 40 objects, 60 properties, 1 KiB strings | 48 324 (≈ 47.2 KiB) |
| 512×512, 4 layers, 2 tilesets, 1024 ids, other defaults | 2 112 364 (≈ 2.01 MiB) |
| Most cells allowed: 4 Mi (e.g. 4 × 1024×1024) | > 8 MiB |

Tileset textures are separate (tag `textures`, `Linear`): a 256×256 RGBA8 sheet is 256 KiB. The Old 3DS has about
64 MB for the whole game, so keep maps to what a level needs.

## Pitfalls

- Capacities are fixed: size `max_*` for everything the level will ever hold, including objects or properties added
  while playing. For a loaded map, ask for that room with `LoadTilemapOptions`.
- A loaded map's tileset textures are its own (a separate `load_texture` of the same image is another texture):
  they count in `max_textures` until the map is freed.
- `set_tile` with a tile id that no tileset has yet is a bug (assert): add tilesets first.
- `max_animations` and `max_animation_frames` are 0 by default: a map built in code needs them set before
  `set_tile_animation` works (it returns `LimitReached` otherwise).
- Animations are per tile id, not per cell: two cells that should animate out of step need two tile ids.
- A `collision` layer makes **every** non-empty tile solid, including decorative ones; mark individual tiles with
  `set_tile_collision` instead when only some should block (a one-way plank on a `collision` layer is solid).
- Setting a `collision_layers` **property** in code (`set_int`) changes nothing (as `solid` isn't a property after
  loading): use `set_layer_collision_layers` / `set_tile_collision_layers`.
- `collision_grid(mask)` with more than 8 distinct masks on one map runs out of slots: keep masks constant.
- `draw_tilemap` needs `AppConfig::gfx.max_tiles` sized for every layer you draw on both screens (≈ 416 per full
  16×16 top-screen layer, 336 on the bottom screen); check `gfx::stats().tiles_last_frame` or the debug overlay.
- A parallax layer with the default parallax origin (0, 0) is shifted at camera (0, 0) (Tiled does the same): set
  the origin to your first view's centre so layers line up where the level starts.
