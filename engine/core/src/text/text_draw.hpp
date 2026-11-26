#pragma once
// draw_text's path for game fonts, wrapping, line limits and outlines (spec 029 FR-7, FR-9, FR-10, FR-11): lines from
// the shared wrapper, each glyph one sprite command of its page (or of the built-in font's texture), the shadow, then
// the outline glyph, then the glyph; missing characters as the replacement glyph or the hollow box. gfx/draw_text.cpp
// sends a call here only when it needs it, so the built-in path stays spec 005's.

#include "nnh/gfx.hpp"
#include "text/text_wrap.hpp"

namespace nnh::detail {

/// A text needs the run layout: a game font, a wrap width, a line limit or an outline colour.
constexpr bool uses_text_layout(const TextOptions& o) {
    return o.font.id != 0 || o.wrap_width > 0 || o.max_lines > 0 || o.outline_color.a > 0;
}

void draw_text_layout(Screen screen, Vec2 position, const char* utf8, const TextOptions& options);

/// Calls fn(const WrapLine&, int index) for each line of `utf8` as draw_text lays it out with `options` and `face`.
template <typename Fn>
void for_each_text_line(const TextFace& face, const char* utf8, const TextOptions& options, Fn&& fn) {
    int length = 0;
    while (utf8[length] != 0) ++length;
    const int wrap = options.wrap_width > 0 ? options.wrap_width : 0;
    TextWrapper w(face, utf8, length, wrap, options.max_lines > 0 ? options.max_lines : 0);
    WrapLine line;
    for (int index = 0; w.next(line); ++index) fn(static_cast<const WrapLine&>(line), index);
}

/// A line's full width at scale 1: its text and, when cut, the ellipsis.
inline int text_line_width(const TextFace& face, const WrapLine& line) {
    return line.width + (line.ellipsis ? ellipsis_width(face) : 0);
}

} // namespace nnh::detail
