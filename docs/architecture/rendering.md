# Rendering: 2D now, 3D later

> Status: **2D implemented** ([spec 004](../../specs/004-2d-graphics/spec.md), PC and 3DS). 3D (M8) is not
> built yet; this page fixes the structure so it can be added without rewriting games.

## The frame, per screen

Drawing is **recorded** during the frame and **submitted** at its end, screen by screen, in passes:

```
game update()/draw()          gfx::draw / fill_rect / line ...   → recorded in the frame's draw list
                              debug::print, overlay              → recorded in the debug list
end of frame (next nnh::frame() or shutdown())
 for each screen (top, bottom):
   2D pass, layers below the world      (today: all 2D)          ┐ sorted by layer, then call order
   3D pass  (M8: meshes, 3D camera)     (not yet)                │ batched by texture
   2D pass, layers above the world      (HUD)                    ┘
   debug pass (debug text, overlay)     always on top, built-in font, own capacity
 present, wait for 60 Hz
```

Today every layer is in the first 2D pass. When 3D arrives, a game marks where the world sits among the
layers, and the core submits the 2D list in two slices around the 3D pass. The 2D calls don't change.

## Tile layers (batches)

> Status: **implemented** on PC, 3DS and the fake platform (spec 012, decision D26). Measured on an Old 3DS XL
> (2026-11-21, `examples/platformer` bench): **4 full layers of 16×16 tiles on the top screen + 300 sprites** at
> avg 16.7 ms, worst 17.4 ms, no missed frame of 600; limits 6 layers (with 300 sprites) or 600 sprites (with 4
> layers). None of D26's fallbacks was needed. API: [tilemap.md](../api/tilemap.md#drawing).

A tilemap layer is recorded as **one command per (map layer, tileset)**, sorted with the sprites by layer and
call order, so sprites can sit between map layers. When the sorted pass reaches it, core sends the sprites
before it (`draw_2d`), expands only the cells the camera sees into compact quads (destination, source, flip
bits) and calls the contract's `draw_tiles` once for the batch. Camera, parallax and offset are applied once
per batch and the layer's origin is snapped to whole pixels once, so tiles never show seams. Tile quads per
frame are capped by `AppConfig::gfx.max_tiles` on every system (the simulator drops what the 3DS would).

How it is built (`engine/core/src/gfx/`, `engine/core/src/tilemap/`):

- `gfx` sees a batch only as `{expand function, source}` plus its texture, tile size, tint and draw layer
  (`gfx/tile_batches.hpp`, at most 64 per frame): it never includes the tilemap module. `DrawList::build()`
  turns each tile command into a **split point** (`{items before it, batch}`) instead of a 2D item, and
  `gfx_frame.cpp` alternates `draw_2d` slices and `draw_tiles` calls.
- At submit the batch's function (`tilemap/map_draw.cpp`) reads the map *then* — camera at the end of the frame,
  tiles changed by `set_tile` after the draw call — computes the layer's snapped origin and visible cells
  (`tilemap/visible_range.cpp`, Tiled's centre-based parallax) and writes quads (`tilemap/tile_expand.cpp`:
  one id test, one table read, one 10-byte write per cell) into one shared scratch of 1600 quads, within the
  frame's remaining `max_tiles`. Animated tiles (`tilemap/tile_animation.cpp`) resolve the current frame of the
  batch's tileset once per batch, from an opaque clock gfx carries for the batch (the frame count at 60 Hz by
  default), so the simulator and the console show the same frame on the same frame number.
- A map destroyed after its draw call forgets its batches (`Tilemap::reset`); a tileset whose texture is gone
  is skipped with an assert, like sprites.
- Memory: nothing when `max_tiles` is 0 (the default); otherwise ~20 KB once (64 batch records and the
  scratch), and on 3DS one citro2d quad (~192 B, `Linear`) per `max_tiles`.

## Zoom (D33)

> Status: **design** ([spec 018](../../specs/018-camera-helpers/spec.md), [plan 018](../../specs/018-camera-helpers/plan.md),
> decision D33). Not built yet: today `Camera2D` has only a position.

`Camera2D` gains a `zoom` (0.5–4, default 1). Core applies it when it builds a screen's pass, so platforms keep
receiving screen pixels:

- A camera-relative draw lands at `(world − position) × zoom`; `position` is still the world point at the screen's
  top-left. Sprites and text glyphs scale (position, size, pivot); rectangles scale, outlines and lines stay one pixel
  thick. `ignore_camera` draws and the debug pass are never zoomed. At zoom 1 every value is today's (multiplying by 1
  is exact).
- Tile layers snap each tile's **edges**, not its size: `edge(i) = O + round(i × tile × zoom)` from the layer's origin
  `O` (snapped once, as today), so tiles never gap or overlap. A batch tells the platform the destination tile size
  (`dst_w`, `dst_h` = `floor(tile × zoom)`) and a quad marks with two orientation bits the tiles one pixel wider or
  taller. Integer zooms are pixel-exact; fractional zooms give uneven texel sizes.
- Visible cells follow the zoomed view (zoom 0.5 shows about four times the tiles, within `max_tiles`). A batch larger
  than the 1600-quad scratch is expanded and sent in row chunks (several `draw_tiles` calls, in order).
- Platforms: the 3DS draws each quad at the destination size with the same texture coordinates; the PC copies to the
  destination rectangle (a diagonal flip with unequal sides uses a pre-swapped rectangle); the fake platform records
  the zoomed items, batches and quads for tests.

## Why record instead of drawing on the spot

- **Order by layer** without making the game sort its calls.
- **Batching**: consecutive draws of the same texture go to the GPU together.
- **Stereoscopic 3D (M8)**: the top screen is drawn twice, once per eye. A recorded list can be
  submitted twice with a horizontal offset per layer (parallax), so 2D games get depth without new calls.
- **Testability**: the list is plain data, checked by unit tests on the fake platform.

The list has a fixed capacity set at `init()` (no allocation per frame). It stores texture **handles**,
not pointers; a texture destroyed before the end of the frame is skipped (and asserts in Debug).

## One texture type for 2D and 3D

| System | A texture is | 2D draws it with | 3D (M8) will sample it with |
|--------|--------------|------------------|-----------------------------|
| 3DS | a citro3d `C3D_Tex` in linear memory (from a tex3ds `.t3x`) | citro2d (`C2D_Image` points at the `C3D_Tex`) | citro3d directly |
| PC | an `SDL_Texture` of a renderer created on an `SDL_GPUDevice` (`SDL_CreateGPURenderer`, SDL 3.4) | the SDL renderer | the same `SDL_GPUDevice` |

citro2d is a layer on top of citro3d: a 3D pass is raw citro3d between `C2D_Flush()` and `C2D_Prepare()`.
On PC the GPU device is shared by the 2D renderer and the future 3D code. Headless runs (CI,
screenshots with `SDL_VIDEO_DRIVER=offscreen`) may have no GPU device: the simulator then falls back to
SDL's default renderer, which is enough for 2D (3D will need a GPU device).

## Coordinates and names

- 2D works in **screen pixels**, (0, 0) top-left, y down, `float` positions (`nnh::Vec2`).
- Each screen has a **`Camera2D`** (a position subtracted from camera-relative draws; a zoom is designed in
  [Zoom (D33)](#zoom-d33)). A 3D camera, 3D
  vectors and matrices will be separate types next to these, never a renamed 2D type.
- The final position of a sprite is rounded to whole pixels on both systems, so pixel art stays sharp
  and PC and 3DS agree; game state keeps the sub-pixel position.

## Rules

- Platform code draws only what core submits; it never keeps its own draw state between frames.
- Textures follow the 3DS rules everywhere (power-of-two sides, 8–1024), checked at build time and on load.
- Texture memory is accounted in the `Linear` region under the `textures` tag on every system.
- No drawing outside a frame; calls outside one are ignored (as for debug text).
- Text, game and debug alike, is engine sprites of the built-in font ([text.md](../api/text.md)); a
  platform has no text functions, only `texture_create_rgba` and `draw_2d` (and `draw_tiles` for tilemaps).

See also: [decisions D5, D19, D20](decisions.md), [platforms.md](platforms.md),
[devkitpro-3ds.md](../tech/devkitpro-3ds.md), [sdl3.md](../tech/sdl3.md).
