# Levels in Tiled

How to make a level for an NNHGE game in the [Tiled](https://www.mapeditor.org/) map editor, what the build
does with it, and how to build the same level in code instead. Tiled is optional: NNHGE never needs it to build
or run a game, it only reads Tiled's JSON files. Reference: [tilemap.md](../api/tilemap.md) and
[collision.md](../api/collision.md). Working example: [`examples/platformer`](../../examples/platformer/main.cpp)
(level 1 made in Tiled, the bonus level built in code).

## 1. Where the files go

```
mygame/
  CMakeLists.txt          nnh_add_game(mygame SOURCES main.cpp ASSETS assets)
  assets/
    levels/level1.tmj     the map (Tiled: File > Save As > JSON map files, extension .tmj)
    tiles/world.tsj       a tileset saved as JSON (or embedded in the map)
    art/tiles.png         the tileset's image
```

The build converts every `.tmj` under the `ASSETS` folder into an `.nnhmap` asset of the same path
(`levels/level1.tmj` → `levels/level1.nnhmap`) with `nnh-assetc`, checking everything on the way. `.tsj`,
`.tiled-project` and `.tiled-session` files are read but not packed into the game. A map is converted again only
when it, a tileset or an image it uses, or the converter changes (the example's 128×20 level with 5 tile layers
converts in about 0.2 s). Only the `.tmj` extension is converted (a map saved as `.json` would be packed as a
plain file). A `.tmj` and an `.nnhmap` of the same name in one folder stop CMake at configure time.

3DS builds run the converter on the PC too: they need a host C++ compiler, which `scripts/dkp.sh` passes in on
Windows ([cmake.md](../tech/cmake.md), "nnh-assetc").

## 2. Map settings

In **Map > Map Properties** (or when creating the map):

| Setting | Use | Otherwise |
|---------|-----|-----------|
| Orientation | **Orthogonal** | isometric, staggered and hexagonal maps are refused |
| Infinite | **off** (a fixed size, 1–1024 cells per side) | refused: "turn off Map > Map Properties > Infinite" |
| Tile Layer Format | **CSV**, or **Base64 (uncompressed)** | gzip, zlib and Zstandard are refused: "save the layer format as CSV" |
| Tile size | **8 to 64 pixels** per side, square or not | refused; diagonal flips (Tiled's rotate) need square tiles |
| Parallax Origin | the centre of your first view, e.g. (200, 120) for the top screen at camera (0, 0) | (0, 0): parallax layers are shifted at the start, as in Tiled |
| Render order, background colour, compression level | anything | ignored |

A map has at most 16 tile layers and 16 object layers, 8 tilesets, 8191 tile ids, 4096 objects and 4 Mi cells
over all tile layers (the full list: [tilemap.md](../api/tilemap.md#building-a-map)).

## 3. Tilesets

- **One image per tileset**, a PNG **inside the assets folder**, with power-of-two sides from 8 to 1024 (it
  becomes a texture). Image-collection tilesets (one image per tile) are refused.
- **The map's tile size.** Margin and spacing work; NNHGE keeps the margin on **every** side of the image (Tiled
  counts it once), so a tileset with a margin whose last column touches the image's right edge is refused with a
  "columns" message (leave the margin on the right and bottom too, or use margin 0).
- **Save it as JSON** (`.tsj`) or embed it in the map. XML tilesets (`.tsx`) are refused: "export the tileset as
  JSON". Wang sets (terrains) and a tile drawing offset are refused.
- Tile ids: tilesets get consecutive ids from 1 in first-gid order, so with one tileset at first gid 1 an NNHGE
  tile id is Tiled's gid.

## 4. Collision

Collision comes from two kinds of boolean custom properties; both become collision flags, not properties:

- **On a tile** (tileset editor, select the tile, add a `bool` property): `solid` = blocks from every side;
  `one_way` = blocks only a box falling onto its top (a plank you jump up through). A tile can't be both.
- **On a tile layer**: `collision` = every non-empty tile of that layer is solid, decorations included.

A cell is solid when a `collision` layer has a tile there or any tile there is `solid`; otherwise one-way when a
tile there is `one_way`. Tiled's per-tile collision shapes (the tile collision editor) are ignored: NNHGE collides
on whole cells. The game moves boxes through `map.collision_grid()` with
[`collision::move`](../api/collision.md#moving-a-box).

Tip: put the solid ground on a layer with `collision`, and one-way planks on another layer as tiles marked
`one_way` (a one-way tile on a `collision` layer is solid).

### Collision layers

A level can collide differently for different movers (spec 019): water that stops boats but lets fish through, planks
only the player stands on. Each collision layer and each `solid`/`one_way` tile carries a set of **collision
layers**; the game asks for a grid that sees only some of them with `map.collision_grid(mask)`
([tilemap.md](../api/tilemap.md#collision-layers)). Without anything set, everything is on the default layer (bit 0)
and the level behaves as before. Set them in Tiled with a custom enum, once per project:

1. **View > Custom Types Editor > Add Enum**, name it `CollisionLayers`. Add one value per layer, **in the order of
   your game's bits**: the first value is bit 0, the second bit 1, … (for example `world`, `water`, `plank`, `boat`).
   Keep `world` first: bit 0 is the engine's default layer, the one every tile is on until you say otherwise.
2. Tick **Allow multiple values (flags)** and choose **Save as: Number**. Tiled then stores the ticked values as bits
   (`world` + `boat` = 1 + 8 = 9), which is exactly what NNHGE reads. Tiled allows bits 0–30.
3. On a **tile layer** with `collision`, add a property `collision_layers` of type `CollisionLayers` and tick the layers
   its tiles are solid on (a `water` layer: `water`).
4. On a **tile** marked `solid` or `one_way` (tileset editor), add `collision_layers` the same way (a plank only the
   player stands on: `plank`).
5. In the game, declare the same bits (`layer(0)` = `world`, `layer(1)` = `water`, …) and ask each mover's grid:
   `map.collision_grid(kWorld | kBoat)` for a boat, `map.collision_grid(kWorld)` for a fish.

A plain `int` property named `collision_layers` works too (without a Tiled project). The build refuses a
`collision_layers` that isn't an integer (an enum saved as a string) or is 0 (`level1.tmj: layer "water": property
"collision_layers" must not be 0: untick collision instead`). On a layer without `collision`, or a tile without
`solid`/`one_way`, it is accepted and does nothing. It also stays readable as an ordinary property.

## 5. Layers, parallax and flips

- **Tile layers** keep their order, name, offset, opacity, visibility, parallax factor and flipped tiles
  (Tiled's X, Y and Z keys: all 8 orientations). Offset and parallax only affect drawing, never cells or
  collision.
- **Parallax** follows Tiled's preview: factor 1 scrolls with the camera, 0 stays still on screen, 0.5 moves at
  half speed. With the map's parallax origin set as in §2, NNHGE shows the same picture Tiled does.
- **Image layers** and **group layers** are refused (draw an image as a sprite; move a group's layers out of it).
  A layer **tint colour** is refused.
- **Which layers go over the player** is the game's choice, not the map's: it draws each map layer with
  `gfx::draw_tilemap(..., {.map_layer = i, .layer = n})` on the draw layer it wants. The platformer draws parallax
  layers behind, a layer named `front` over the player, the rest as ground.

## 6. Animated tiles

Tiled's **Tile Animation Editor** (tileset editor): frames are tiles of the same tileset, each 1–65535 ms, 1–32
frames per tile; at most 256 animated tiles and 4096 frames in a map. Every cell of that tile animates in step,
on a clock that counts frames at 60 Hz, so the simulator and the console show the same frame on the same frame
number ([tilemap.md](../api/tilemap.md#animated-tiles)). Collision and properties are the tile's own.

## 7. Objects and properties

- **Objects**: rectangles and points, with their id, name, type (Tiled's "Class" field; `type` or `class` in the
  JSON) and custom properties. Visibility and the object layer's offset are not used. Refused: ellipses,
  polygons, polylines, text, tile objects, templates (detach them) and rotated objects. Each refusal says what to
  use instead (a rectangle, or a rectangle with a property).
- **Properties** on the map, tile layers, object layers, tiles and objects: `bool`, `int`, `float`, `string`;
  `color` and `file` become strings (as written), `object` becomes an int (the object's id). `class` properties
  are refused; names are 1–255 bytes and unique per owner; strings are at most 255 bytes.

### Placing prefabs in Tiled

With [prefabs](../api/prefabs.md) (spec 021), a level's objects name the things the game spawns, so a designer places
a new kind of thing without asking for code. `prefabs.spawn_map_objects(world, level)` walks the objects in map order:

- **The type names the prefab.** Give an object the type (class) `coin` and the game's `coin` prefab is spawned at it.
- **Or the `prefab` property does.** A string property `prefab` = `flying_walker` names the prefab whatever the type
  says (the type stays free for your own use). Use it to say "this must be a prefab".
- **Where it goes.** A point object gives a position; a rectangle object gives its bounds: the prefab's placement member
  gets them (a `Rect` member takes the rectangle's size too, unless the prefab keeps its own).
- **Properties reach the prefab's hook.** The hook gets the object (`info.object->properties.get_int("value", 1)`):
  a gold coin is a `coin` with an int property `value` of 5.
- **Skipped.** An object whose type names no prefab — a door, a spawn point, an object with no type — is skipped:
  the game reads those itself (`level.find_object("spawn")`). The summary counts them.
- **Errors.** A `prefab` property naming no prefab is a level error, logged with the object's id, name and layer —
  `object 12 "coin" (layer things): no prefab "coinn"` — and asserting in Debug/Development builds, so a misspelt name
  is found on the PC before the console. A `prefab` property that is not a string is the same error. The build can't
  check these names (prefabs are registered in code at run time): `prefabs.check_map_objects(level)` answers the same
  report without spawning, for a test or a loading screen.

A level restart is `world.clear()` plus `spawn_map_objects` again: the same objects get the slots of the first load.

## 8. Using the level in the game

```cpp
nnh::init({.title = "My Game", .gfx = {.max_tiles = 2048}});      // tile quads per frame; 0 = no tilemaps

auto loaded = nnh::load_tilemap("levels/level1");                 // the converted assets/levels/level1.tmj
if (!loaded) { NNH_LOG_ERROR(game, "%s", loaded.error().message); return; }
nnh::Tilemap level = std::move(loaded.value());

nnh::collision::Grid grid = level.collision_grid();
grid.outside = nnh::collision::Cell::Solid;                       // wall the level in
const nnh::MapObject spawn = level.object(level.find_object("spawn"));
const char* title = level.properties().get_string("title", "");

nnh::CameraRig camera{{.dead_zone = nnh::CameraSettings::centered_zone(32, 48)}};   // <nnh/camera.hpp>
camera.set_limits(level.bounds());                                // never show past the map's edges
camera.snap(player);

// each frame
const auto moved = nnh::collision::move(grid, player, velocity * dt);
camera.update(player);                                            // after moving the player
camera.apply();
nnh::gfx::draw_tilemap(nnh::Screen::Top, level);                  // every visible layer, draw layer 0
```

The map owns its tileset textures (loaded from the image names it carries) and frees them with it. A loaded map
has no spare room: to add objects or properties in code later, ask for it with `LoadTilemapOptions`
([tilemap.md](../api/tilemap.md#loading-a-converted-map)). `max_tiles` must cover every layer drawn in a frame,
about 416 per full layer of 16×16 tiles on the top screen (about four times that at camera zoom 0.5); an Old 3DS
draws 4 such layers plus 300 sprites at 60 Hz. The camera (dead zone, smoothing, limits from `level.bounds()`,
shake, zoom, both screens as one tall world): [camera.md](../api/camera.md).

## 9. What the build refuses

A map that breaks a rule stops the build with `file: reason` (the map, the tileset or the image the problem is
in) and no output. Examples, from the converter's tests (`tests/data/maps/bad_*.tmj`, ctests
`map_convert_explains_*`):

| Message | Fix |
|---------|-----|
| `infinite maps aren't supported: turn off Map > Map Properties > Infinite` | a fixed-size map |
| `isometric maps aren't supported: NNHGE draws orthogonal maps (Map > Map Properties > Orientation)` | orientation Orthogonal |
| `layer "ground": compressed layer data (zlib) isn't supported: save the layer format as CSV` | Tile Layer Format CSV |
| `layer "sky": image layers aren't supported: draw the image as a sprite` | draw it from code |
| `layer "level": group layers aren't supported: move its layers out of it` | ungroup |
| `wang sets (terrains) aren't supported: remove them from the tileset` | remove the terrain sets |
| `tileset image ../images/ok_16x8.png is outside the assets folder ...: move it inside` | move the image |
| `tileset image tiles/npot.png is 100x60: tileset images need power-of-two sides from 8 to 1024` | pad the image |
| `tiles are 4x4: NNHGE tiles are 8 to 64 pixels per side` | 8–64 px tiles |
| `layer "ground": cell (1, 0) is flipped diagonally, which needs square tiles` | square tiles, or no rotation |
| `object 1 in layer "things": text objects aren't supported: use a rectangle with a string property` | as it says |
| `map: property "stats": class properties aren't supported` | plain properties |
| `tile 1: animation frame 1: "tileid" must be a whole number from 0 to 7` | frames from the same tileset |
| `more than 16 tile layers`, `more than 8 tilesets`, `the map has more than 4096 objects` | split the level |
| `line 3, column 19: expected ',' or '}'` | the file is not valid JSON |

The complete list is in [tilemap.md](../api/tilemap.md#maps-made-in-tiled). Every file the converter writes is
checked again with the validator `load_tilemap` runs, so a map that converts always loads.

## 10. The same level in code

Everything above can be built without Tiled, with the same `Tilemap` (a unit test builds a map both ways and
compares every query: `tests/tilemap/test_code_vs_file.cpp`). The platformer's bonus level is built this way
([`bonus_level.cpp`](../../examples/platformer/bonus_level.cpp)):

```cpp
auto made = nnh::Tilemap::create({.width = 100, .height = 15,            // Map > Map Properties: size
                                  .max_tile_layers = 4, .max_tilesets = 1,
                                  .max_animations = 1, .max_animation_frames = 2});
if (!made) return made.error();
nnh::Tilemap map = std::move(made.value());

const int first = map.add_tileset({.texture = &tiles}).value();         // the tileset (first id 1)
const int clouds = map.add_tile_layer({.name = "clouds", .parallax = {0.25f, 1}}).value();
const int ground = map.add_tile_layer({.name = "ground", .collision = true}).value();  // layer property
map.set_parallax_origin({200, 120});                                     // Map Properties: Parallax Origin

const int plank = first + 3, water = first + 5;                          // tiles of the sheet
map.set_tile_collision(plank, nnh::collision::Cell::OneWay);             // tile property one_way
const nnh::Tilemap::AnimationFrame ripple[] = {
    {.tile_id = static_cast<std::uint16_t>(water), .ms = 400},
    {.tile_id = static_cast<std::uint16_t>(water + 1), .ms = 400}};
(void)map.set_tile_animation(water, ripple, 2);                          // Tile Animation Editor

map.fill(ground, 0, 12, 20, 3, {.id = static_cast<std::uint16_t>(first)});
map.set_tile(clouds, 4, 2, {.id = static_cast<std::uint16_t>(first + 10), .flip_x = true});

const int things = map.add_object_layer("things").value();              // object layer
(void)map.add_object(things, {.name = "spawn", .shape = nnh::ObjectShape::Point, .bounds = {40, 192}});
(void)map.set_string(nnh::PropertyOwner::map(), "title", "BONUS LEVEL");  // map property
```

A map built in code sizes its capacities itself (`TilemapDesc`): there is no file to take them from. Choose
Tiled for levels you draw by hand, code for levels you generate or for tests; a game can mix both, and edit a
loaded map in code too.

## Map objects that spawn hierarchies

Tiled has no object parenting: a map object names one prefab, and if that prefab has children (spec 022,
[prefabs.md](../api/prefabs.md#children)) the object spawns the whole hierarchy, its bounds placing the root and every
child's spawn hook seeing the same map object.
