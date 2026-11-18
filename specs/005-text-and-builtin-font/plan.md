# Plan 005: Text — the built-in NNHGE font

Status: implemented (2026-11-18); verified on PC and an Old 3DS XL
Spec: [spec.md](spec.md) · Architecture: [rendering.md](../../docs/architecture/rendering.md)

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | One font, one layout code in core; platforms only upload pixels and draw sprites |
| II 3DS constraint | ✅ | 128×128 RGBA font texture = 64 KB linear; one citro2d object per character |
| III Build speed | ✅ | Font data is one generated `.cpp` in core (rebuilt only when the generator runs) |
| IV Event-driven | ✅ | Drawing is per-frame state; no events |
| V No hidden costs | ✅ | Texture created at `init()`; layout walks the string, no allocation |
| VI Spec + test first | ✅ | UTF-8 decoding, glyph mapping and layout are pure and tested first |
| VII Small files | ✅ | utf8, glyph map, layout, font data, draw_text, debug pass in separate files |
| VIII Documented API | ✅ | New `docs/api/text.md`; `gfx.md`, `debug.md`, contract docs updated |
| IX Clean and legal | ✅ | Font drawn for NNHGE by `scripts/make_font.py` (zlib) |

## Design

### Public API (`<nnh/gfx.hpp>`)

```cpp
enum class TextAlign : std::uint8_t { Left, Center, Right };   // relative to the position's x, per line

struct TextOptions {
    Color color = colors::white;
    int scale = 1;                      // 1, 2, 3... (clamped to 1..8)
    TextAlign align = TextAlign::Left;
    bool shadow = false;                // 1-pixel (× scale) drop shadow, down-right
    Color shadow_color = rgba(0, 0, 0, 160);
    int layer = 0;
    bool ignore_camera = false;
};

inline constexpr int kFontCellSize = 8;      // glyph cell and advance, pixels at scale 1
inline constexpr int kFontLineHeight = 10;   // cell + 2 pixels of line spacing

namespace gfx {
void draw_text(Screen, Vec2 position, const char* utf8, const TextOptions& = {});   // top-left of line 1
Vec2 measure_text(const char* utf8, int scale = 1);   // {widest line × 8, lines × 10} × scale; "" → {0, 0}
}
```

`debug::line_height()` returns `kFontLineHeight` (10) on every system.

### Font

- **Cell 8×8, monospace**, advance 8, line height 10. Glyph rows: 1–6 capitals and digits (6 rows),
  2–6 lowercase x-height (5 rows), 0 ascenders, 7 descenders, 0–1 accents (over lowercase, and over
  capitals shortened to rows 2–6). Glyphs are mostly 5 pixels wide (odd, so A, T, V, X, Y, I, + keep a
  centre column); M, W and a few symbols use a 6th column. Columns 0 and 7 stay empty (≥ 2 px gap).
  (Revised after the maintainer's review of the first preview: lowercase was 4 rows and too small.)
- **192 glyphs**: ASCII 32–126 (95), a replacement glyph (a hollow box, index 95), Latin-1 160–255 (96).
  Latin-1 letters are composed by the generator from base letters + diacritic marks (grave, acute,
  circumflex, tilde, diaeresis, ring, cedilla, stroke), so they stay consistent.
- **Generator** `scripts/make_font.py` (pure Python, like the example art): the glyphs are drawn as text
  grids in the script; it writes `engine/core/src/text/builtin_font.cpp` (192 × 8 bytes, one bit per
  pixel, plus a header comment) and a preview PNG for review (not committed). The generated `.cpp` is
  committed, so building needs no Python.
- **Atlas** at `init()`: 16 glyphs per row × 12 rows = 128×96, placed in a **128×128** RGBA texture
  (power of two), white glyphs on transparent; color comes from the tint (spec 004).

### Pure core logic (unit tested)

- `text/utf8.{hpp,cpp}` — `char32_t next_codepoint(const char*& p)`: decodes 1–4 byte sequences; invalid,
  overlong, surrogate or truncated sequences return U+FFFD and advance at least one byte; stops at `\0`.
- `text/glyphs.hpp` — `int glyph_index(char32_t c)`: 32–126 → 0–94, 160–255 → 96–191, anything else →
  95 (replacement); `\n` and other controls are handled by the layout, not drawn.
- `text/layout.{hpp,cpp}` — `measure(utf8, scale)` and `for_each_glyph(utf8, origin, scale, align, fn)`:
  walks lines (per-line width for alignment), yields `{glyph index, x, y}` for visible glyphs (spaces
  advance without drawing).

### Contract (`nnh/platform/video.hpp`)

```cpp
/// Creates a texture from RGBA8 pixels (row-major, top row first, width × height × 4 bytes), sides
/// powers of two 8..1024. Load time only (may allocate). Same TextureInfo/texture_destroy as images.
bool texture_create_rgba(const void* pixels, int width, int height, TextureInfo& out);
```

The debug pass moves onto the font, so `debug_text`, `debug_line_height` and `fill_rect` leave the
contract: a port implements textures and `draw_2d`, nothing font-specific.

### Core changes

- `text/builtin_font.{hpp,cpp}` (generated data) + `text/font_texture.cpp`: builds the 128×128 RGBA
  atlas into a temporary buffer at `gfx_init`, creates the texture with `texture_create_rgba`, registers
  it in the texture table with one extra slot reserved for the engine (`max_textures` stays the game's
  limit; `gfx::stats()` counts only game textures), memory tag `font`.
- `gfx/draw_text.cpp` — `draw_text`/`measure_text`: one `DrawCommand` per visible glyph (shadow first,
  then the glyph), region = the glyph's cell in the atlas, `scale` as sprite scale; counts against
  `max_draws` like any draw.
- **Debug pass** (`debug/debug_list` + new `debug/debug_pass.cpp`): debug text and the overlay's
  rectangles are recorded in the debug list during the frame; at the end, after the 2D passes, they are
  laid out into a **separate** item buffer (4096 items, allocated at init, tag `gfx`) and submitted with
  `draw_2d` per screen. The overlay records rectangles instead of calling `fill_rect`.
- Tests keep checking debug output through an internal hook that exposes the last frame's debug list
  (the fake platform no longer sees text, only glyph sprites).

### Platform work

- **pc:** `texture_create_rgba` = `SDL_CreateTexture(SDL_PIXELFORMAT_RGBA32, STATIC)` +
  `SDL_UpdateTexture`, nearest, blend. Remove `debug_text_pc.cpp` and `fill_rect`.
- **3ds:** `texture_create_rgba` = `C3D_TexInit(w, h, GPU_RGBA8)` (linear) + CPU conversion to the GPU's
  layout (8×8 tiles in Morton order, rows stored bottom-up, bytes ABGR) + `C3D_TexFlush`. Remove
  `debug_text_3ds.cpp`, the citro2d text buffer and `fill_rect`.
- **fake:** `texture_create_rgba` records size; tests read glyph items from `draw_2d`.

### Data and memory

Font texture 64 KB linear (tag `font`). Debug item buffer 4096 × ~60 B ≈ 240 KB heap (tag `gfx`) plus
the existing debug list (12 KB static). Font data in the executable: 1.5 KB. No per-frame allocation.

## Test strategy

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `tests/text/test_builtin_font.cpp`: every glyph but space has pixels; atlas texture created at init |
| FR-2 | `test_utf8.cpp` (valid, invalid, truncated, overlong) + `test_glyphs.cpp` (mapping, replacement) |
| FR-3 | `test_draw_text.cpp`: glyph items, regions, scale, layer, camera, alignment, `\n`, shadow order |
| FR-4 | `test_layout.cpp`: widths/heights, scales, empty string, trailing newline |
| FR-5 | `examples/text` screenshots vs the 3DS (AC-3) |
| FR-6 | `test_debug_pass.cpp`: debug text becomes glyph items after the 2D passes; overlay tests via the hook |
| FR-7 | conformance `texture_create_rgba_accepts_pixels`; 3DS tiling checked by the font on hardware |
| FR-8 | `test_draw_text.cpp`: characters count against `max_draws`; font not in game stats |

## Risks and alternatives

- **3DS tiling/byte order** wrong → garbled or mirrored font. Mitigation: the font itself is the test
  pattern (asymmetric glyphs like `F`, `R`, `7`), compared with the simulator screenshot on hardware.
- **Readability at 8×8** with accents: capitals shrink to 5 rows when accented; checked in the preview
  and on the 3DS screen.
- **Screenshots change** for every example using debug text (expected; new references).
- Alternative rejected: keeping the 3DS system font for debug text (the inconsistency this spec removes).

## Constitution Check (after design)

No change. The contract shrinks for ports (no font functions) and gains one generic function.
