#pragma once
// Template part of layout.hpp. Internal.

#include "text/glyphs.hpp"
#include "text/utf8.hpp"

namespace nnh::detail {

/// Cells in the line starting at `p` (up to '\n' or the end).
int line_cells(const char* p);

template <typename Fn>
void layout_text(const char* utf8, Vec2 origin, int scale, TextAlign align, Fn&& fn) {
    if (utf8 == nullptr) return;
    scale = clamp_text_scale(scale);
    const float cell = static_cast<float>(kFontCellSize * scale);
    const float line_height = static_cast<float>(kFontLineHeight * scale);
    const char* p = utf8;
    float y = origin.y;
    for (;;) {
        const float width = static_cast<float>(line_cells(p)) * cell;
        float x = origin.x;
        if (align == TextAlign::Center) x -= width / 2;
        if (align == TextAlign::Right) x -= width;
        char32_t c = 0;
        while ((c = next_codepoint(p)) != 0 && c != U'\n') {
            if (c == U'\r') continue;
            if (!is_space(c)) fn(GlyphPlacement{glyph_index(c), x, y});
            x += cell;
        }
        if (c == 0) return;
        y += line_height;
    }
}

} // namespace nnh::detail
