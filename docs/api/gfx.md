# gfx — screens, colors, 2D drawing, cameras

Header: `<nnh/gfx.hpp>` · Design: [architecture/rendering.md](../architecture/rendering.md) · Textures:
[texture.md](texture.md) · Types: [math.md](math.md)

## Screens

| `nnh::Screen` | Size | Notes |
|---------------|------|-------|
| `Top` | 400×240 | Stereoscopic 3D screen (3D comes in M8). |
| `Bottom` | 320×240 | Touch screen ([input.md](input.md)). |

`nnh::screen_width(screen)` and `nnh::screen_height(screen)` are `constexpr`. Coordinates are pixels with
(0, 0) at the top-left of each screen.

## Colors

`nnh::Color` is RGBA8. Build with `nnh::rgb(r, g, b)` / `nnh::rgba(r, g, b, a)` or use `nnh::colors::*`
(`black`, `white`, `red`, `green`, `blue`, `yellow`, `gray`).

## Clearing

```cpp
nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(32, 48, 96));
```

Each screen is cleared to its color at the start of every frame. The default is black. The change applies
from the next `nnh::frame()`.

## Drawing sprites

```cpp
nnh::Texture hero = std::move(nnh::load_texture("art/hero").value());   // assets/art/hero.png, see texture.md
...
nnh::gfx::draw(nnh::Screen::Top, hero, player.position,
               {.region = {16 * frame, 0, 16, 16}, .pivot = {8, 16}, .flip_x = facing_left, .layer = 2});
```

`gfx::draw(screen, texture, position, options = {})` draws the texture's `region` (empty = the whole
texture) so that its `pivot` lands on `position`. `DrawOptions`:

| Field | Default | Effect |
|-------|---------|--------|
| `region` | whole texture | `Region{x, y, w, h}` in texture pixels: one frame of a sprite sheet |
| `pivot` | (0, 0) = top-left | point of the region placed at `position`; rotation and scale use it |
| `rotation` | 0 | radians, clockwise on screen |
| `scale` | (1, 1) | a negative scale mirrors, like `flip_x`/`flip_y` |
| `flip_x`, `flip_y` | false | mirror the image inside its rectangle (the pivot is a point of the unmirrored region, so a centered pivot keeps the sprite in place) |
| `tint` | white | multiplies the texture's colors |
| `opacity` | 1 | 0–1, multiplies the tint's alpha (PNG transparency is always kept) |
| `layer` | 0 | 0–15, see below |
| `ignore_camera` | false | `position` is in screen pixels (HUD) |

Pixel art stays sharp: textures use nearest filtering and the final position is rounded to whole pixels
(your game keeps the exact `float` position).

## Shapes

```cpp
nnh::gfx::fill_rect(nnh::Screen::Bottom, {10, 10, 100, 20}, nnh::colors::blue);
nnh::gfx::draw_rect(nnh::Screen::Bottom, {10, 10, 100, 20}, nnh::colors::white);   // 1-pixel outline
nnh::gfx::draw_line(nnh::Screen::Top, {0, 0}, {399, 239}, nnh::colors::red, {.layer = 15});
```

Shapes take `ShapeOptions{layer, ignore_camera}`.

## Order: layers

Everything drawn on a screen in a frame is **recorded** and drawn when the frame ends: layer 0 first,
layer 15 last; within a layer, in call order. Tilemap layers (`gfx::draw_tilemap`, [tilemap.md](tilemap.md#drawing))
follow the same order, so a sprite can sit between a background and a foreground layer. Debug text and the debug overlay always draw on top.
Animated sprites: a `SpriteAnimator`'s `draw_options()` fills the region, pivot and flips of its current frame
([animation.md](animation.md#drawing-fr-10)).
Calls outside a frame (before the first `frame()`, after the loop) are ignored.

## Cameras

```cpp
nnh::gfx::set_camera(nnh::Screen::Top, {.position = {player.x - 200, 0}});   // follow the player
nnh::gfx::draw(nnh::Screen::Top, hud_icon, {4, 4}, {.ignore_camera = true}); // HUD stays put
```

Each screen has a `Camera2D`: its `position` is the world point shown at the screen's top-left corner.
Draws are shifted by it unless they set `ignore_camera`. Both cameras start at (0, 0) at `init()`.

## Text

`gfx::draw_text(screen, position, "Olá!", {.scale = 2, .align = TextAlign::Center, .shadow = true})` and
`gfx::measure_text(text, scale)` draw and measure UTF-8 text with the built-in NNHGE font, identical on
every system: see [text.md](text.md).

## Capacities

Fixed at `nnh::init()` through `AppConfig::gfx` (allocated once, memory tag `gfx`):

| Field | Default | Range | Cost |
|-------|---------|-------|------|
| `max_draws` | 2048 | 16–65535 | draws per frame, both screens; about 136 bytes each (~280 KB by default) |
| `max_textures` | 128 | ≥ 4 | textures loaded at once; about 32 bytes each |
| `max_tiles` | 0 | 0–65535 | tile quads per frame, both screens, drawn by `gfx::draw_tilemap` ([tilemap.md](tilemap.md#drawing)). 0 = no tilemap drawing and no memory for it; otherwise ~20 KB once, and on 3DS ~192 bytes of `Linear` per tile (citro2d's buffer). A full layer of 16×16 tiles on the top screen is up to 26 × 16 = 416 |

```cpp
nnh::init({.title = "Bullet Hell", .gfx = {.max_draws = 8192}});
nnh::init({.title = "Platformer", .gfx = {.max_tiles = 2048}});   // ~4 full 16x16 layers on both screens
```

How many sprites fit in a frame at 60 Hz (an Old 3DS XL: about 1300 of 16×16 from one texture): run the benchmark of [`examples/sprites`](../../examples/sprites/main.cpp)
(SELECT on the console, `SPRITES_DEMO=bench` on PC; L/R and Y/X change the count).

More draws than `max_draws` in a frame is a bug: Debug and Development **assert**; Release drops the
extra draws and logs one warning per frame. The same holds for tile quads over `max_tiles` (the simulator
counts and drops exactly what the 3DS would), and `draw_tilemap` with `max_tiles = 0` asserts with the fix in
the message. Each tile batch (one per map layer and tileset) also takes one of `max_draws`.

`gfx::stats()` returns `draws_last_frame`, `max_draws`, `dropped_draws`, `textures`, `max_textures` and
`texture_bytes` (the game's textures: the built-in font has its own slot and memory tag), plus
`tiles_last_frame`, `max_tiles` and `dropped_tiles` (since `init()`); the debug overlay shows them too (the
tiles line only when `max_tiles > 0`). How many tile layers fit at 60 Hz (an Old 3DS XL: **4 full layers of
16×16 tiles on the top screen + 300 sprites**, avg 16.7 ms, worst 17.4 ms, no missed frame of 600; limits 6 layers
with 300 sprites, or 600 sprites with 4 layers): the benchmark of
[`examples/platformer`](../../examples/platformer/main.cpp) (SELECT twice on the console, after level 1 and the
bonus level; `PLATFORMER_DEMO=bench` on PC); A there runs its automatic gate, which finds the limits by itself
(`PLATFORMER_DEMO=gate`, or the script `examples/platformer/tests/gate.nnhs` with `nnh run`).
