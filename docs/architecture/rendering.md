# Rendering: 2D now, 3D later

> Status: design for [spec 004](../../specs/004-2d-graphics/spec.md) (2D graphics, in progress). 3D
> (M8) is not built yet; this page fixes the structure so it can be added without rewriting games.

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
   debug pass (debug text, overlay)     always on top
 present, wait for 60 Hz
```

Today every layer is in the first 2D pass. When 3D arrives, a game marks where the world sits among the
layers, and the core submits the 2D list in two slices around the 3D pass. The 2D calls don't change.

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
- Each screen has a **`Camera2D`** (a position subtracted from camera-relative draws). A 3D camera, 3D
  vectors and matrices will be separate types next to these, never a renamed 2D type.
- The final position of a sprite is rounded to whole pixels on both systems, so pixel art stays sharp
  and PC and 3DS agree; game state keeps the sub-pixel position.

## Rules

- Platform code draws only what core submits; it never keeps its own draw state between frames.
- Textures follow the 3DS rules everywhere (power-of-two sides, 8–1024), checked at build time and on load.
- Texture memory is accounted in the `Linear` region under the `textures` tag on every system.
- No drawing outside a frame; calls outside one are ignored (as for debug text).

See also: [decisions D5, D19, D20](decisions.md), [platforms.md](platforms.md),
[devkitpro-3ds.md](../tech/devkitpro-3ds.md), [sdl3.md](../tech/sdl3.md).
