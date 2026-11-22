# Spec 012: Tilemaps and collision

Status: implemented (2026-11-22; T7.4, the console run of `level.nnhs`, is still pending with the
maintainer — see [tasks.md](tasks.md)). Approved 2026-11-21, with every proposed answer
below; amended the same day: code first, Tiled optional — NNHGE has no editor, so everything works through the C++ API
Author: NNHGE maintainers · Date: 2026-11-21
Depends on: [spec 004](../004-2d-graphics/spec.md) (textures, draws, layers, cameras),
[spec 007](../007-assets-and-romfs/spec.md) (assets and build-time conversion), [spec 002](../002-core-foundation/spec.md)
(memory tags, `Result`, logs). Implements the first part of ROADMAP M7 (tilemaps and collision; entities, tweens and
scene transitions are later specs).

## Problem and goal

Today a game can draw sprites but has no notion of a level: every platformer or top-down game must invent its own
map format, draw thousands of tiles by hand and write its own collision code, which is where most homebrew games
spend (and lose) their time. After this spec a game builds a level **in code** — size, tilesets, layers, tiles,
solid and one-way tiles, objects — or, optionally, draws it in a common map editor and loads the file with one call;
either way it draws it scrolling with the camera at 60 Hz on an Old 3DS, reads the
objects placed in it (spawn points, coins, doors), and moves its characters against the solid tiles and against each
other with a few calls that never let them pass through walls. The same code behaves identically in the simulator and
on the 3DS.

## User stories

- As a game developer, I want to build a level entirely from code (generated, hand-written arrays, my own format),
  because NNHGE has no editor and I may not want one.
- As a game developer, I want, optionally, to draw my levels in a map editor I already know and drop the files into
  my assets folder, so that I don't have to write a level editor or a file format.
- As a game developer, I want to load a level and draw it, with several layers (background, ground, foreground) and
  the camera following my player, without thinking about which tiles are on screen.
- As a game developer, I want to place spawn points, enemies and items in the editor and read them in my game, with
  their names and custom properties.
- As a game developer, I want to move my player against the level's walls and floors and know whether it is standing
  on the ground or hit a ceiling, so that a platformer's movement takes a few lines.
- As a game developer, I want to check whether two things overlap (the player and a coin, a bullet and an enemy).
- As a player, I want levels to scroll smoothly at 60 Hz on my Old 3DS.
- As a contributor, I want map reading and collision tested without a console, and the map reader fuzzed, because a
  map file is external data.

## Functional requirements

- **FR-0 Maps built in code (the base).** Every part of a map — tile size, size in tiles, tilesets (a texture and its
  tile size), tile layers and their cells, tile flips, solid and one-way tiles, object layers, objects and properties
  — can be created and filled through the C++ API alone, with no file and no tool. A map from a file (FR-1) is the same
  object, so everything below works on both, and a game can load a map and then change it in code.
- **FR-1 Maps from a map editor (optional).** A map can also be a file in the game's assets folder, made with
  **Tiled** (the common free map editor; see open questions for the exact format). The build checks and converts it, like images (spec 007): a
  broken map, a missing tileset image or an unsupported feature fails the **build**, naming the file and the reason.
  The game loads it by path: `load_tilemap("levels/level1")`. A map that cannot be loaded at run time returns a
  `Result` error, never a crash.
- **FR-2 Tile layers and tilesets.** A map has a tile size (any size from 8 to 64 pixels, square or not), a size in
  tiles, one or more **tile layers** drawn in order, and the **tilesets** they use: images from the assets, with
  margin and spacing as Tiled allows. Tiles may be flipped horizontally, vertically or diagonally (as Tiled stores
  them). Empty cells draw nothing.
- **FR-3 Drawing.** One call draws a map layer (or all layers) on a screen with the screen's camera and a draw layer
  (0–15, as for sprites, so the player can stand between background and foreground). Only the tiles inside the
  camera's view are drawn. Layers can have their own offset, opacity and **parallax** factor (a background layer
  that scrolls slower), as set in the editor or changed by the game. Pixel art stays sharp (as sprites).
- **FR-4 Reading and changing tiles.** The game can read the tile at a cell or at a world position, convert between
  world pixels and cells, and change a tile at run time (a block breaks, a door opens); drawing and collision see the
  change at once.
- **FR-5 Objects.** Object layers from the editor are available to the game: each object has a name, a type
  (class), a position and size (rectangles and points), and its custom properties (text, number, true/false). The
  game can list them and find them by name or type.
- **FR-6 Properties.** Custom properties on the map, its layers and its tiles are readable by name (e.g. a map's
  `music`, a tile's `damage`).
- **FR-7 Solid tiles.** A tile is solid when its tileset marks it so in the editor (see open questions for how);
  a layer can be the collision layer. Besides fully solid tiles, **one-way platforms** (solid only from above, the
  platform a player jumps up through) are supported.
- **FR-8 Moving against the map.** Given a box (axis-aligned rectangle) and how far it wants to move this frame,
  the engine returns where it ends up without overlapping solid tiles, sliding along walls, and which sides touched
  (floor, ceiling, left, right). A fast box never passes through a wall, whatever its speed.
- **FR-9 Overlap queries.** Box against box, point in box, and "which solid tiles does this box touch" are
  available, with the overlap amount where it helps to push things apart. These are plain functions on values:
  usable for anything, not only maps.
- **FR-10 Same result everywhere.** Loading, collision and the tiles drawn give the same results in the simulator
  and on the 3DS, for the same inputs, frame after frame.
- **FR-11 Limits and memory.** Maps load into engine memory (own memory tag) at load time, with no allocation while
  playing; documented limits (map size, layers, tilesets, objects) are checked at build time and at load time. The
  docs give the memory a map costs, so a game knows what fits in an Old 3DS.

## Acceptance criteria

- **AC-1** An example game (`examples/platformer`, original art made for the engine) with a map made in Tiled: two or
  more tile layers with a parallax background, a player that runs and jumps on floors, walls and one-way platforms,
  coins placed as objects, a camera that follows the player; plus a small bonus level built entirely in code (FR-0). Runs on PC and on an Old 3DS at a steady 60 Hz while
  scrolling (the debug overlay shows no dropped frames).
- **AC-2** Unit tests: a map built in code (every FR-0 part) behaves like the same map read from a file; map reading (layers, tilesets, flips, objects, properties), coordinate conversions, tile
  changes, visible-tile range for a camera with parallax, box-against-map movement (slide, corners, one-way platforms,
  speed larger than a tile, touching sides), box and point overlaps.
- **AC-3** A broken or unsupported map fails the build with a message naming the file and the problem (test of the
  converter with bad files); a damaged converted map fails `load_tilemap` with an error (test). The map reader is a
  fuzz target, run on Linux under ASan/UBSan without crashes.
- **AC-4** A drawing benchmark in the example (like the sprites one) reports how many tile layers fill the top screen
  at 60 Hz on an Old 3DS; the number goes in the docs.
- **AC-5** Docs: `docs/api/` pages for the new headers, a guide on making a level in Tiled for NNHGE, the memory cost
  of a map, CLAUDE.md and the ROADMAP updated.

## Non-goals

- A physics engine: no forces, rotation, bounciness, polygons or circles; no slopes in this spec (see open questions).
- Entities/components, tweens and scene transitions (later M7 specs).
- Isometric and hexagonal maps; infinite (chunked) Tiled maps; image layers; Tiled's group layers, wang sets and
  text objects. The build refuses them with a clear message.
- (Animated tiles were a possible non-goal; the open question below decided to include them — stage 6.)
- A level editor inside the engine; reading Tiled files at run time on the console (they are converted at build
  time).
- Path-finding and AI.

## Constraints

- **I** — the same calls on PC and 3DS; no platform code in game code.
- **II** — memory and draw budget of an Old 3DS: a full-screen layer of 16×16 tiles is 25×15 = 375 draws, and the
  sprites benchmark reaches about 1300 sprites at 60 Hz; the plan must show how several layers plus sprites fit
  (fewer draws, batching or pre-rendered chunks). Tilesets obey the texture limits (power of two, ≤ 1024).
- **III** — converting maps must not slow builds: only changed maps are converted again.
- **V** — no heap allocation while playing; collision is plain math on values.
- **VI** — map reading and collision are unit tested first; drawing is checked by screenshots and on the console.
- **X** — the map converter and the run-time reader are bounded and fuzzed (maps are external data).
- **IX** — the example's art is original (made for NNHGE). Tiled is not shipped and not needed to build a game: the
  build reads the map files only.

## Open questions

- [x] Which Tiled format: **JSON (`.tmj`)** or XML (`.tmx`)? — **answer:** JSON, parsed at build time by a small
      bounded reader; `.tmx` later if asked for.
- [x] How a tile is marked solid: a custom boolean property `solid` (and `one_way`) on the tile in the tileset, a
      dedicated collision layer, or both? — **answer:** both; a tile property for tilesets shared between maps, and
      any layer whose property `collision` is true counts every non-empty tile as solid.
- [x] Slopes (45° and 22.5°) for platformers — **answer:** not in this spec; a follow-up if a game needs them.
- [x] Animated tiles (Tiled's tile animations: water, lava) — **answer:** include if the plan shows it is cheap
      (tile ids swapped by time, no per-tile state); otherwise a follow-up.
- [x] Map limits — **answer:** up to 1024×1024 tiles per layer, 16 layers, 8 tilesets, 4096 objects; to be checked
      against the memory budget in the plan.
- [x] Does collision belong in its own module (`<nnh/collision.hpp>`, usable without maps) and maps in
      `<nnh/tilemap.hpp>`? — **answer:** yes, two modules.
