# debug — debug text

Header: `<nnh/debug.hpp>`

Quick text for development: values, state, instructions. It uses the built-in NNHGE font, so it looks
**the same on every system** and `line_height()` is 10 everywhere. For game text (colors, scale,
alignment, shadow, layers, measuring) use [`gfx::draw_text`](text.md).

```cpp
const int line = nnh::debug::line_height();
nnh::debug::print(nnh::Screen::Top, 8, 8, "Hello!", nnh::colors::yellow);
nnh::debug::printf(nnh::Screen::Top, 8, 8 + line, "x=%d y=%d", x, y);
```

| Function | Notes |
|----------|-------|
| `print(screen, x, y, text, color = white)` | `(x, y)` is the top-left corner in pixels. `\n` starts a new line. |
| `printf(screen, x, y, format, ...)` | Always white. Output is truncated at 255 characters. |
| `line_height()` | Pixels per line on this platform. Use it instead of a fixed number. |

- Only works inside a frame (after `nnh::frame()` returned true); otherwise ignored.
- Drawn when the frame ends, **on top of** every sprite and shape ([gfx.md](gfx.md)), in call order.
- No heap allocation; safe every frame. Per frame: up to 256 texts and 8 KB of text; more is dropped.
- Debug output has its own capacity, apart from the game's `max_draws`: up to 4096 characters and
  overlay rectangles per screen per frame; more is dropped.

## Debug overlay

`nnh::debug::set_overlay_visible(bool)` / `overlay_visible()`. Players toggle it with **SELECT + L**
(3DS) or **F1** (PC simulator). It covers the bottom screen with: FPS and frame time (average and max
over the last 2 s), a frame-time graph (green ≤ 1 frame, yellow ≤ 2, red more), memory per region
(used / capacity), 2D draws and textures ([gfx.md](gfx.md#capacities)) and the slowest profile scopes ([profile.md](profile.md)). Debug and Development
only; always hidden in Release.

## Overlay pages (spec 030)

With the overlay shown, SELECT + R cycles Stats → Tunables → Watches; the live pages and their controls are in
[live.md](live.md#the-overlays-pages). The Stats page gains a line `live 12/128 tun 5/64 wat 3/64 shown 20.1 KB` when the
game registers live values. A new engine starts with the overlay hidden on the Stats page.
