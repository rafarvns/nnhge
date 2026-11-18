#pragma once
// Measuring text and placing its glyphs in the built-in font's grid. Pure; unit tested in
// tests/text/test_layout.cpp.
//
// Every code point takes one cell (kFontCellSize wide); lines are kFontLineHeight apart; '\n' starts a
// line, '\r' takes no room. Spaces advance without a glyph. Scale is clamped to 1..8.

#include "nnh/gfx.hpp"
#include "nnh/math.hpp"

namespace nnh::detail {

struct GlyphPlacement {
    int glyph = 0; ///< index in the built-in font (glyphs.hpp)
    float x = 0;   ///< top-left of the cell
    float y = 0;
};

int clamp_text_scale(int scale);

/// Size the text takes: {widest line, lines × line height}, both × scale. nullptr or "" → {0, 0}.
Vec2 measure(const char* utf8, int scale);

/// Calls `fn(const GlyphPlacement&)` for every visible character, in order. `origin` is the top-left of
/// the first line for TextAlign::Left; for Center/Right, each line is centered on / ends at origin.x.
template <typename Fn>
void layout_text(const char* utf8, Vec2 origin, int scale, TextAlign align, Fn&& fn);

} // namespace nnh::detail

#include "text/layout_impl.hpp"
