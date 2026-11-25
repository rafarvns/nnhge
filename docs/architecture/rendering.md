# Rendering: 2D and 3D

> Status: **2D implemented** ([spec 004](../../specs/004-2d-graphics/spec.md), PC and 3DS). **3D mono implemented**
> ([spec 026](../../specs/026-3d-foundation/spec.md) part 026a: meshes, materials, lights, the 3D pass among the 2D
> layers; PC rasteriser and citro3d backend, checked on the Old 3DS); **stereo** (026b: the 3D slider, the top screen
> drawn once per eye) implemented on the PC, console run pending.

## The frame, per screen

Drawing is **recorded** during the frame and **submitted** at its end, screen by screen, in passes:

```
game update()/draw()          gfx::draw / fill_rect / line ...   → recorded in the frame's draw list
                              debug::print, overlay              → recorded in the debug list
end of frame (next nnh::frame() or shutdown())
 for each screen (top, bottom):
   2D pass, layers below the world      (a sky)                  ┐ sorted by layer, then call order
   3D pass  (spec 026: meshes, camera)  only in frames with 3D   │ batched by texture
   2D pass, layers from the world up    (HUD)                    ┘
   debug pass (debug text, overlay)     always on top, built-in font, own capacity
 present, wait for 60 Hz
```

A frame without 3D draws submits the whole 2D list as one pass, exactly as before 3D existed. A frame with 3D draws on
the top screen splits it at the screen's **world layer** (`gfx3d::set_world_layer`, default 8; decision D43):
`gfx/world_split.cpp` finds the first item and tile batch at or above it, `gfx_submit_slice` sends the 2D below, the 3D
module (`gfx3d/screen_pass.cpp`) culls, orders and submits the 3D pass (`platform::begin_3d / draw_3d / end_3d`), and
the rest of the 2D follows. The 2D calls don't change.

The 3D module is reached only through one hook pointer (`gfx/gfx3d_hook.hpp`), set by a registrar in an object that
only mesh and 3D entry points link: a 2D-only game links no 3D code (its cost: the pointer's null checks).

## The 3D pass (spec 026)

- **Recording**: `gfx3d::draw` stores a 92-byte record (model rows, optional material, mesh handle, tint) and the
  draw's world bounding sphere, O(1), no allocation; capacity `AppConfig::gfx.max_draws_3d`.
- **At the end of the frame**: spheres culled against the camera's frustum in view space; the opaque and cut-out parts
  in call order, then the blended parts back to front (stable merge sort by view depth); per draw, core folds the
  mesh's quantisation decode into the model-view matrix and resolves the material (tint, texture handle), so the
  backend only uploads values. Lights go to the backend in view space.
- **Backends**: the 3DS draws with citro3d (a 24-bit depth target replacing citro2d's top target, the mesh vertex
  shader, fragment lighting tables, texture combiners) between `C2D_Flush()` and `C2D_Prepare()`; 2D in a 3D game draws
  with depth off. The PC simulator draws with the engine's own **CPU rasteriser** (`engine/platforms/shared/raster3d/`,
  decision D41): into a 400×240 colour and depth buffer that starts as the clear colour or as the top target read back
  when 2D was drawn below the world, put back opaque, so every blend happens on the CPU and screenshots are the same on
  every renderer. The fake platform records every call and can run the same rasteriser for pixel tests.
- **Stereo (026b, FR-14/FR-15)**: core reads `system::stereo_slider()` and, when it is above 0 with a perspective camera
  whose stereo is on, makes two eyes (`gfx3d/eye_setup.cpp`: the view moved by ∓separation/2 along the camera's right,
  an off-axis projection that keeps the focal plane on the same pixels). Culling runs once with both eyes' frusta (a
  draw is kept if either sees it), the order once by the centre camera; then the screen's whole sequence — 2D below,
  3D pass, 2D above — runs per eye after `platform::select_eye(eye)`, and `select_eye(0)` restores the left eye; the
  debug pass draws the top screen's items in both eyes. `platform::set_stereo(on)` tells the backend each frame (3DS:
  `gfxSet3D` on change; a frame without 3D draws shows one picture). The tile and particle budgets are rewound before
  the second eye, so they are spent once. Backends: the 3DS swaps citro2d's top target between two RGBA8 + D24S8 targets
  on the left and right outputs (the right one cleared at its first use in a frame); the PC swaps its top render target
  and composes the eye view (left, right or a red/cyan anaglyph, F8) into the window; the fake records `L`/`R` calls.

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

> Status: **implemented** on PC, 3DS and the fake platform ([spec 018](../../specs/018-camera-helpers/spec.md),
> [plan 018](../../specs/018-camera-helpers/plan.md), decision D33); console check (screenshots, the zoomed gate)
> pending. API: [gfx.md](../api/gfx.md#zoom), [tilemap.md](../api/tilemap.md#zoom).

`Camera2D` has a `zoom` (0.5–4, default 1). Core applies it when it builds a screen's pass, so platforms keep
receiving screen pixels:

- A camera-relative draw lands at `(world − position) × zoom`; `position` is still the world point at the screen's
  top-left. Sprites and text glyphs scale (position, size, pivot); rectangles scale, outlines and lines stay one pixel
  thick. `ignore_camera` draws and the debug pass are never zoomed. At zoom 1 every value is today's (multiplying by 1
  is exact).
- Tile layers snap each tile's **edges**, not its size: `edge(i) = O + round(i × tile × zoom)` from the layer's origin
  `O` (snapped once, as at zoom 1), so tiles never gap or overlap. The edges are computed in integers from the zoom in
  16.16 fixed point (`tilemap/zoom_edges.hpp`), so every step is exactly `floor(tile × zoom)` or one more and PC and
  3DS compute the same edges; at zoom 1 the unzoomed loop runs (`tilemap/tile_expand.cpp`), the zoomed one is
  `tilemap/tile_expand_zoom.cpp`. A batch tells the platform the destination tile size
  (`dst_w`, `dst_h` = `floor(tile × zoom)`) and a quad marks with two orientation bits the tiles one pixel wider or
  taller. Integer zooms are pixel-exact; fractional zooms give uneven texel sizes.
- Visible cells follow the zoomed view (zoom 0.5 shows about four times the tiles, within `max_tiles`). A batch larger
  than the 1600-quad scratch is expanded and sent in row chunks (several `draw_tiles` calls, in order): the expand
  function stops at a row end and reports the next row, `gfx_frame.cpp` loops. The destination tile size comes from
  the expand function too (`TileExpandResult::dst_w/dst_h`), so the rounding lives in one place.
- The cameras live apart from the rest of the graphics state (`detail::gfx_cameras()`), so the zoom's default of 1
  doesn't move that state from `.bss` to `.data`. The zoomed arithmetic (`draw_list.cpp`, `visible_range.cpp`,
  `tile_expand*.cpp`, `zoom_edges.cpp`) is compiled with `-ffp-contract=off`.
- The optional camera helper `nnh::CameraRig` ([camera.md](../api/camera.md)) zooms about the view's centre and writes
  `Camera2D` with `apply()`; games may set `Camera2D` by hand.
- Platforms: the 3DS draws each quad at the destination size with the same texture coordinates; the PC copies to the
  destination rectangle (a diagonal flip with unequal sides uses a pre-swapped rectangle); the fake platform records
  the zoomed items, batches and quads for tests.

## Particle batches (D39)

A second kind of batch command ([particles.md](../api/particles.md), spec 024): `ParticleEmitter::draw` records a
particle batch (`{expand function, source = the emitter's block, texture or none, tint × opacity, layer,
ignore_camera}`) in a table of 32 per frame, and one draw-list command whose `tile_batch` index is 64 + i. The draw
list is unchanged: any batch index makes a split point (a screen holds up to 64 tile and 32 particle splits). At
submit, `gfx_frame.cpp` sends a split below 64 to the tile path and one from 64 to the particle batches through
`detail::g_gfx_particle_ops` — null unless `gfx/particle_batches.o` is linked (its static registrar sets it before
`main()`), so a game without particles pays a few null checks. The batch's expand function (in the particles module)
reads the emitter then, skips it when its bounds grown by its largest quad miss the view, and writes the visible
particles' `DrawItem2D`s through the placement shared with `DrawList::build` (`gfx/sprite_place.hpp`: camera, zoom,
rounding) into a 128-item scratch, sent with `platform::draw_2d` chunk by chunk within `AppConfig::gfx.max_particles`.
No contract change; the 3DS sizes citro2d for `max_particles` more objects. A destroyed emitter forgets its batches.

## Why record instead of drawing on the spot

- **Order by layer** without making the game sort its calls.
- **Batching**: consecutive draws of the same texture go to the GPU together.
- **Stereoscopic 3D (spec 026b)**: the top screen is drawn twice, once per eye. The recorded list is submitted twice
  (a horizontal offset per layer is a follow-up), so 2D needs no new calls.
- **Testability**: the list is plain data, checked by unit tests on the fake platform.

The list has a fixed capacity set at `init()` (no allocation per frame). It stores texture **handles**,
not pointers; a texture destroyed before the end of the frame is skipped (and asserts in Debug). A texture or mesh
released while a frame is open, or while the GPU may still draw the last submitted frame, is freed only after the next
`platform::begin_frame()` has waited for the GPU (D47); `nnh::shutdown()` calls `platform::finish_frames()` first.

## One texture type for 2D and 3D

| System | A texture is | 2D draws it with | 3D samples it with |
|--------|--------------|------------------|-----------------------------|
| 3DS | a citro3d `C3D_Tex` in linear memory (from a tex3ds `.t3x`) | citro2d (`C2D_Image` points at the `C3D_Tex`) | citro3d directly |
| PC | an `SDL_Texture` of a renderer created on an `SDL_GPUDevice` (`SDL_CreateGPURenderer`, SDL 3.4) | the SDL renderer | the CPU rasteriser, from its pixels read back once (`texels3d_pc.cpp`) |

citro2d is a layer on top of citro3d: a 3D pass is raw citro3d between `C2D_Flush()` and `C2D_Prepare()`.
On PC the 3D pass needs no GPU device (decision D41, amending D20): headless runs (CI, screenshots with
`SDL_VIDEO_DRIVER=offscreen`) and SDL's default renderer draw 3D too.

## Coordinates and names

- 2D works in **screen pixels**, (0, 0) top-left, y down, `float` positions (`nnh::Vec2`).
- Each screen has a **`Camera2D`** (a position subtracted from camera-relative draws and a zoom, see
  [Zoom (D33)](#zoom-d33)). The 3D camera, vectors and matrices are separate types
  ([math3d.md](../api/math3d.md), [gfx3d.md](../api/gfx3d.md)), never renamed 2D types.
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
