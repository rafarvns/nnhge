# Spec 004: 2D graphics — textures, sprites, shapes, camera

Status: in progress (approved 2026-11-18)
Author: NNHGE maintainers · Date: 2026-11-18
Depends on: [spec 002 — core foundation](../002-core-foundation/spec.md) (memory regions, platform
contract, debug overlay) — implemented. Related: M3 (`assetc`, romfs) in [ROADMAP](../../docs/ROADMAP.md).

## Problem and goal

Today a game can only clear the screens and print debug text: it cannot show a single image. Every game
needs to draw pictures (characters, tiles, backgrounds, UI) on both screens, move a view over a world
bigger than the screen, and do it fast on an Old 3DS. After this spec a game puts images in its project,
draws them as sprites (position, frame of a sprite sheet, flip, rotation, scale, tint) and simple shapes
on either screen, with a 2D camera per screen, and gets the same picture in the simulator and on the 3DS
at 60 Hz.

## User stories

- As a game developer, I want to add a PNG to my game and draw it on a screen, so that my game has
  graphics.
- As a game developer, I want to draw one frame of a sprite sheet, flipped, rotated, scaled or tinted, so
  that one image gives me animations and variations.
- As a game developer, I want to control which sprites draw on top, so that the player is in front of
  the background and the HUD in front of everything.
- As a game developer, I want a camera per screen, so that the level scrolls while the HUD stays put.
- As a game developer, I want images that break 3DS rules (size not a power of two, too big) to be
  rejected on PC too, so that I don't discover it only on the console.
- As a game developer, I want to know how many sprites I can draw at 60 Hz on an Old 3DS, so that I can
  design within that budget.

## Functional requirements

- **FR-1 Images in the game.** A game declares its images in its build (e.g. in `nnh_add_game`); they
  are converted for each platform at build time and become loadable by name. Changing an image rebuilds
  only what depends on it (constitution III).
- **FR-2 Textures.** Loading an image gives a texture handle (RAII: freed when the handle goes away,
  like `Subscription`). Loading reports failure (missing, invalid, over a limit) as a result the game can
  check, not a crash. Textures live in the memory the GPU can read, accounted in the memory tags of
  spec 002. Loading happens at load time; drawing never allocates.
- **FR-3 3DS texture rules everywhere.** Width and height must each be a power of two from 8 to 1024. The
  build step and the PC simulator reject images that break it with a clear message naming the file
  (decision D4). Images that are not a power of two are not padded silently.
- **FR-4 Sprites.** A game draws a texture or a rectangular region of it (a sprite-sheet frame) at a
  position on a screen, with optional horizontal/vertical flip, rotation around a pivot, scale, color
  tint and opacity. Pixel art stays sharp (nearest filtering by default).
- **FR-5 Shapes.** A game draws filled rectangles, rectangle outlines and lines with a color and
  opacity (for UI, debug and prototypes).
- **FR-6 Draw order.** Everything drawn on a screen in a frame appears in a predictable order: by layer
  first, then in call order. The debug text and overlay of spec 002 stay on top of everything.
- **FR-7 Camera.** Each screen has a 2D camera (position; zoom per open question) that shifts what is
  drawn on that screen. A draw can ignore the camera (HUD). The camera is plain state set by the game,
  not an object the engine owns.
- **FR-8 Batching and budget.** Draws of the same texture are submitted together to the GPU where the
  order allows it. The per-frame capacity of draws is fixed at `init()` (configurable like the event
  bus); going over it asserts in Debug/Development and drops the extra draws with one warning in Release.
- **FR-9 Same picture on PC and 3DS.** A given frame looks the same in the simulator and on the console
  (same pixel positions, flips, rotation, blending), within the limits of the two GPUs.
- **FR-10 Testable without a GPU.** The draw list (order, layers, camera, region math, capacity) is core
  logic driven through the platform contract, so it is unit tested with the fake platform recording what
  would be drawn.

### Ready for 3D

The engine must render 3D later (M8: citro3d meshes, stereoscopic top screen) without rewriting games
or this spec's API. Nothing here draws 3D; these requirements keep the door open.

- **FR-11 One texture type.** A texture loaded by this spec is the same resource a future 3D renderer
  will sample (3DS: a citro3d texture, which citro2d draws through; PC: a texture of the GPU device the
  future 3D renderer will use). Same handle, same memory accounting, same 3DS rules.
- **FR-12 Frame passes.** Each screen's frame is drawn in passes: today one 2D pass; later a 3D pass
  before it (world), with 2D on top (HUD), and 2D allowed before it (backgrounds). The platform contract
  is organized by screen and pass so a 3D pass can be added without changing the 2D calls.
- **FR-13 Replayable draw list.** 2D draws are recorded into a per-frame list and submitted at the end of
  the frame, not drawn on the spot, so the same list can later be submitted twice for the stereoscopic
  top screen (left and right eye). A layer can later get a stereo depth (parallax) without new draw calls.
- **FR-14 Names that leave room.** 2D concepts are named as 2D where a 3D counterpart will exist
  (e.g. the screen camera is a 2D camera; positions are 2D vectors), so 3D types (3D camera, 3D vectors,
  matrices) can be added beside them later without renames.

## Acceptance criteria

- **AC-1** Unit tests (fake platform) cover texture validation and failures (FR-2/FR-3), region and
  flip/rotate/scale parameters reaching the platform (FR-4), shapes (FR-5), ordering by layer and call
  (FR-6), camera and camera-free draws (FR-7), capacity overflow (FR-8).
- **AC-2** A new example `examples/sprites/` shows an animated character from a sprite sheet walking over
  a scrolling background (camera) on the top screen, with a HUD that ignores the camera, and shapes on
  the bottom screen. Simulator screenshots show it.
- **AC-3** A benchmark (example or mode of the example) draws N 16×16 sprites from one texture; on an Old
  3DS it holds 60 Hz with at least the number agreed in the open questions, measured on the maintainer's
  console and recorded in the ROADMAP.
- **AC-4** An image that is not a power of two fails the build (or the PC load) with a message naming the
  file and its size.
- **AC-5** The 3DS run of `examples/sprites` matches the simulator screenshot (maintainer).
- **AC-6** `docs/api/gfx.md` documents textures, sprites, shapes, layers and camera; `hello`, `input`,
  `scenes` and `foundation` still work unchanged; build budgets still met (constitution III).

## Non-goals

- Bitmap fonts and real text rendering — next spec (the ROADMAP M2 item "Text rendering").
- Tilemaps, animation playback (timelines), tweens, particles — M7 or later specs.
- Drawing anything in 3D: meshes, 3D camera, lighting, depth buffer use, stereoscopic output and the 3D
  slider (M8). Only the readiness of FR-11–FR-14 is in scope.
- Custom shaders and render-to-texture.
- The full asset pipeline (`assetc`, romfs, incremental cache) — M3; this spec only needs enough to get
  images into a game (FR-1) and must not block M3 from replacing it without changing the game API.
- Simulator window options (layout, scale presets) — separate small item.

## Constraints

- Constitution I: no `#ifdef` in games; one API on PC and 3DS.
- Constitution II: textures power of two 8–1024; GPU memory (linear heap, VRAM 6 MB) and the citro2d
  per-frame object limit; the simulator enforces the same limits (D4).
- Constitution III: images converted incrementally; editing one game `.cpp` stays under the budget.
- Constitution V: no heap allocation or RTTI per frame; fixed draw capacity.
- Constitution VII: texture loading, draw list, camera and per-platform backends in separate files.
- Constitution IX: no copyrighted art in examples; example images are made for the project (zlib).
- FR-11/FR-12 on PC must keep headless simulator runs (`SDL_VIDEO_DRIVER=offscreen`, screenshots) working:
  if the GPU device is not available there, the plan must say what the simulator falls back to.

## Open questions

Answered by the maintainer (2026-11-18): the proposed options, all accepted.

- [x] **Where images come from** — embedded in the executable: the build converts each declared image
  (tex3ds on 3DS, PNG decoded by SDL on PC) and links it in; M3 later moves images to romfs without
  changing the game API.
- [x] **API style** — immediate draw calls every frame (`gfx::draw(texture, x, y, options)`), recorded
  into the frame's draw list (FR-13) and batched by the engine.
- [x] **Layers** — a small integer layer per draw (0–15), then call order.
- [x] **Camera** — position only for now.
- [x] **Sprite budget** — default capacity 2048 draws per frame (configurable at `init()`); AC-3 target:
  1000 16×16 sprites at 60 Hz on an Old 3DS.
- [x] **Sub-pixel positions** — `float` positions.
- [x] **Transparency** — alpha from the PNG plus a per-draw opacity; no color key.
- [x] **3D readiness (FR-11–FR-14)** — added after the answers above, at the maintainer's request;
  confirmed (2026-11-18).
