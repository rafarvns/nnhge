# gfx — screens and colors

Header: `<nnh/gfx.hpp>`

## Screens

| `nnh::Screen` | Size | Notes |
|---------------|------|-------|
| `Top` | 400×240 | Stereoscopic 3D screen (3D comes in M8). |
| `Bottom` | 320×240 | Touch screen (touch input in M1). |

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

Sprites, textures and real text rendering arrive in M2.
