# Spec 005: Text — the built-in NNHGE font

Status: implemented (2026-11-18); verified on PC and an Old 3DS XL
Author: NNHGE maintainers · Date: 2026-11-18
Depends on: [spec 004 — 2D graphics](../004-2d-graphics/spec.md) (textures, draw list, passes) —
implemented.

## Problem and goal

Games can't draw real text: only debug text, which uses the 3DS system font on the console and an 8×8
font on PC. The two have different widths, so text laid out on PC overflows on the 3DS (seen in the
`examples/sprites` HUD), and any future console would bring yet another font. After this spec the
engine ships **its own font**, part of the engine itself: every game gets it without declaring
anything, it draws **pixel-identical on every system** (PC, 3DS, future consoles), text can be measured
before it is drawn, it covers Portuguese and other Latin languages, and debug text uses it too.

## User stories

- As a game developer, I want to draw text in my HUD and menus that looks the same on PC and 3DS, so
  that a layout I build in the simulator doesn't break on the console.
- As a game developer, I want to measure a text before drawing it, so that I can size boxes and center
  labels.
- As a game developer writing in Portuguese (or Spanish, French…), I want accented letters
  (á, ã, ç, é, õ, ü…) to just work from UTF-8 strings.
- As an engine contributor porting to a new console, I want the font to come with the engine, so that
  the port only has to upload a texture, not provide a font.

## Functional requirements

- **FR-1 Built-in font.** The engine contains a default pixel font, original to NNHGE (zlib), available
  after `nnh::init()` with nothing declared by the game. It is part of the engine's source (not a game
  image), so every system gets the same glyphs.
- **FR-2 Characters.** The font covers printable ASCII (32–126) and the printable Latin-1 range
  (160–255: accented letters, ç, ñ, ß, ¿, ¡, «», °; Latin-1 has no €). Strings are UTF-8. A character the font
  doesn't have, or an invalid UTF-8 sequence, draws a visible replacement glyph (never crashes, never
  reads past the string).
- **FR-3 Drawing text.** A game draws a string on a screen at a position, with color, integer scale
  (1×, 2×, …), layer, `ignore_camera` and horizontal alignment (left, center, right of the position),
  through the same recorded, layered draw list as sprites (spec 004). `\n` starts a new line.
- **FR-4 Measuring.** A game gets the width and height in pixels a string will take at a given scale
  (widest line × number of lines), without drawing it. The value is the same on every system.
- **FR-5 Identical everywhere.** The same string at the same position produces the same pixels on PC and
  3DS (same glyph bitmaps, same advance, same line height); screenshots of the simulator are the
  reference for the console.
- **FR-6 Debug text uses it.** `nnh::debug::print`/`printf` and the debug overlay draw with the built-in
  font, so debug text is identical on PC and 3DS too; `debug::line_height()` becomes the same value on
  every system. Debug text keeps its rules (on top of everything, only inside a frame, no allocation).
- **FR-7 Textures from pixels.** The platform contract gains "create a texture from RGBA pixels in
  memory", used by the engine for the font (and available to later features: generated textures,
  render targets). A new console port implements it once and gets the font for free.
- **FR-8 Cost.** No heap allocation per frame; the font texture is created once at `init()` and
  accounted in the `textures` memory tag; drawing a character costs about one sprite draw, and the
  per-frame capacity rules of spec 004 apply (documented: how characters count against `max_draws`).

## Acceptance criteria

- **AC-1** Unit tests (fake platform) cover UTF-8 decoding (ASCII, 2-byte Latin-1, invalid and truncated
  sequences, characters outside the font), measurement (single line, several lines, scale, empty
  string), alignment, line breaks, layers/camera reaching the draw list, and the debug text path.
- **AC-2** A new example `examples/text` shows the whole character set, a Portuguese sentence with
  accents, the three alignments, scales 1× to 3×, and a box sized with the measurement function.
  Simulator screenshots show it.
- **AC-3** On the 3DS, `examples/text` matches the simulator screenshot (maintainer), and the HUD of
  `examples/sprites` fits its box with the box sized by measuring the text (no hand-tuned width).
- **AC-4** `hello`, `input`, `scenes` and `foundation` run unchanged in code; their screenshots change only
  by the new debug font (references updated).
- **AC-5** `docs/api/text.md` (new), `debug.md`, `gfx.md` and the platform contract docs are updated; build
  budgets still met; Release strings check still passes.

## Non-goals

- Fonts provided by the game (from its `IMAGES` or font files) — a follow-up spec (see open questions).
- Proportional (variable-width) fonts, kerning, word wrap, right-to-left scripts, CJK, emoji.
- TrueType/OpenType rendering, outlines/shadows effects beyond what the open questions decide.
- Localization tables (string IDs, language switching).

## Constraints

- Constitution I: same API and same pixels on every system; no `#ifdef` in games.
- Constitution II and V: one texture created at `init()` (small: e.g. 128×128 RGBA = 64 KB of linear
  memory), no per-frame heap; 3DS citro2d object budget respected (each character is an object).
- Constitution III: the font lives in a C++ source file; build budgets unchanged.
- Constitution IX: the font is drawn for NNHGE (zlib), generated by a script in the repository, like the
  example art; no copied fonts.
- Constitution VII: UTF-8 decoding, measurement/layout, the font data and the drawing in separate files.

## Open questions

Answered by the maintainer (2026-11-18): the proposed options, all accepted.

- [x] **Glyph size** — 8×8 cells, monospace (50 characters per line on the top screen).
- [x] **Game fonts** — a follow-up spec.
- [x] **Shadow** — yes: a `shadow` option draws a 1-pixel drop shadow (one extra draw per character).
- [x] **Characters vs `max_draws`** — each drawn character (and its shadow) is one draw.
- [x] **Debug text budget** — the debug pass keeps its own fixed capacity (about 4096 characters per
  frame), separate from the game's `max_draws`.
- [x] **API** — `nnh::gfx::draw_text(...)` and `nnh::gfx::measure_text(...)` in `<nnh/gfx.hpp>`.
