# debug — debug text

Header: `<nnh/debug.hpp>`

Quick text for development: values, state, instructions. **Not for game UI**: the font is different on
each platform (3DS system font at half size, proportional; PC 8×8 monospace). Real text rendering with
your own fonts comes in M2.

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
- No heap allocation; safe every frame.
- **3DS limit**: about 4096 glyphs per frame in total. Text past the limit is dropped.
