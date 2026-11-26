# fonts — game fonts

Header: `<nnh/font.hpp>` (drawing options in `<nnh/gfx.hpp>`) · Spec: [029](../../specs/029-game-fonts-and-localization/spec.md) ·
Decision: [D49](../architecture/decisions.md) · Example: `examples/localization` · Languages: [localization.md](localization.md) ·
Guide: [localizing a game](../guides/localizing-a-game.md)

A game can draw text in its own fonts — proportional TrueType/OpenType faces baked at one size, or pixel BMFonts — with
the same `gfx::draw_text` the built-in font uses ([text.md](text.md)). The build converts each font into a `.nnhfont`:
4-bit coverage pages (or colour pages for a coloured BMFont), integer metrics, kerning and line-break classes, so the
text lays out **identically on the PC simulator and the 3DS** (only partial texels may differ by the GPU's filtering).
Fonts are optional: a game that never calls `load_font()` links no font code.

```cpp
auto body = nnh::load_font("fonts/body_16");              // assets/fonts/body_16.font, converted at build
if (!body) return false;
font_ = static_cast<nnh::Font&&>(body.value());
nnh::gfx::draw_text(nnh::Screen::Top, {8, 8}, "Olá, mundo — ¡hola!", {.font = font_});
nnh::gfx::draw_text(nnh::Screen::Bottom, {8, 40}, long_text, {.font = font_, .wrap_width = 300, .max_lines = 3});
const nnh::Vec2 box = nnh::gfx::measure_text(long_text, {.font = font_, .wrap_width = 300});
```

## Font assets

Put fonts in the game's `ASSETS` folder; `nnh_add_game` routes them (the sources are never packed):

| File | Becomes |
|------|---------|
| `fonts/body_16.font` — a description of one face at one size | `fonts/body_16.nnhfont` |
| `fonts/hud.fnt` + `hud_0.png`… — an AngelCode BMFont (text or XML form) | `fonts/hud.nnhfont` |
| `*.ttf`, `*.otf`, `*.ttc`, `*.chars` | read by descriptions, not packed |

A description (spec 014's TOML subset):

```toml
source = "src/NNHExampleSansJP.otf"   # beside the description
size = 16                             # em size in pixels, 6..128
chars = ["ascii", "latin1", "U+0400..U+045F", "U+20AC"]
strings = ["en", "pt-BR", "de"]       # every character of these languages' string files
fallback = ["fonts/names_13"]         # up to 3 fonts searched for a character this one lacks (one level)
outline = 2                           # 0..3: a baked outline glyph per glyph (TextOptions::outline_color)
licence = "licences/OFL-Example.txt"  # an asset path packed with the game, or "own"
# antialias = false, threshold = 128, padding = 1, page = 1024, replacement = "none", face = 0, chars_from = ["x.chars"]
```

Named character sets: `ascii`, `latin1`, `latin-ext-a`, `latin-ext-b`, `latin-ext-additional`, `greek`, `cyrillic`,
`armenian`, `georgian`, `general-punctuation`, `currency`, `arrows`, `box-drawing`, `cjk-punctuation`, `hiragana`,
`katakana`, `kana`, `fullwidth`, `hangul`, `jis-level1` (2 965 kanji), `kyoiku` (the 1 006 school kanji), and
`"U+XXXX"` / `"U+XXXX..U+YYYY"`. Characters the source lacks are reported as one note; a licence is required for a
`source` (write `"own"` for your own font). The build prints one line per font:
`font fonts/body_16: 280 glyphs, 1 page 512x256 (A4), 82 KB, licence licences/OFL-Example.txt`.

A page is at most 1024 × 1024; a font has at most 8 pages. A4 pages cost half a byte per texel in the file and one
byte (LA4) on the 3DS; the Japanese body font of the example (1 358 glyphs at 13 px) takes one 1024 × 512 page, 295 KB.

## API

| API | |
|-----|-|
| `load_font(path, tag = nullptr)` | loads `<path>.nnhfont` and its fallbacks (a font loaded earlier only as another's fallback gets its own now; a fallback that already falls back to this font is skipped); `Result<Font>`. Errors: `NotFound`, `Corrupted` (the validator's reason logged, or the font names itself as a fallback), `OutOfMemory`, `LimitReached` (32 fonts or no texture slot), `InvalidArgument` (a path whose name hash is another loaded font's) |
| `Font` | the owner (movable). A path loaded twice shares one entry; the last owner frees it (pages released after the GPU, D47) |
| `font.ref()` / `FontRef` | what `TextOptions::font` and the UI hold; `FontRef{}` is the built-in font |
| `metrics()`, `has(cp)`, `covers(cp)`, `advance(cp)`, `kerning(a, b)`, `path()`, `dump()` | queries (integers at scale 1) |
| `gfx::font_metrics(ref)` | `{size, line_height, ascent, descent, glyphs, pages, bytes, outline, colour}` |
| `gfx::measure_text(utf8, options)` | the box `draw_text` fills with these options (font, wrap, line limit, ellipsis) |
| `gfx::measure_chars(utf8, options, out, capacity)` | per code point: byte offset, pen x, line y, advance, line (each clamped to −32768..32767) — cursors, typewriters |
| `font::debug_draw_pages(screen, ref, at, page = -1)` | Debug/Development: the pages side by side, scaled down to fit the screen, with page outlines and glyph rectangles |

`TextOptions` fields for fonts (all default to the built-in behaviour):

| Field | Effect |
|-------|--------|
| `font` | the font (`FontRef{}`: the built-in font) |
| `wrap_width` | > 0: lines break at this width in pixels (× scale): spaces and breaking punctuation, between CJK characters (no line starts with `、。」` or ends with `「`, moving back at most two characters), long words split |
| `max_lines` | > 0: at most this many lines; the last ends with `…` (or `...` when the font lacks it) |
| `outline_color` | alpha > 0: the font's baked outline drawn under each glyph (a font without outlines ignores it, said once) |

Kerning applies between two characters of the same font. A character no font of the face has draws the font's
replacement glyph (U+FFFD or `?`), else a hollow box of the font's size; Debug/Development logs it once per font
(`text: U+4E00 not in fonts/body_16`).

## Costs

Drawing a game glyph is one sprite like a built-in one, plus the lookup: Latin-1 is a direct table, the BMP a bucket of
at most 32 glyphs, so a kanji costs two or three compares. Measuring and wrapping allocate nothing. Loading reads the
file once, decodes each page into a temporary buffer and uploads it. On the PC simulator (Development build, PC numbers
only): 400 glyphs cost 0.69 µs per glyph with a font against 0.73 µs built-in, CPU and submit together (the CPU part:
0.10 µs against 0.04 µs; text without wrapping walks each line once); a 400-character paragraph wraps in 0.056 ms
(Latin) and 0.062 ms (Japanese); loading a full 1024 × 1024 page takes ≈ 15 ms. The console's numbers come with
the console run (spec 029 T10.2).

## Misuse

A destroyed font drawn or measured asserts in Debug/Development and draws with the built-in font in Release; a scale
outside 1..8 is clamped; `outline_color` on a font without outlines is ignored (logged once).
