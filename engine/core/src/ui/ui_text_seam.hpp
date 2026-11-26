#pragma once
// The UI's text seam (spec 028 FR-24): every measure of widget text goes through here, drawing through ui_draw_out's
// run. Its only font today is spec 005's built-in grid (cells of kFontCellSize × scale, lines kFontLineHeight × scale
// apart); the game-fonts spec adds fonts behind UiTextLook::font without changing widget code.

#include "nnh/ui.hpp"

namespace nnh::detail {

struct UiBlock;

/// detail::next_codepoint (text/utf8.hpp), for seam_next's non-ASCII bytes.
char32_t seam_next_slow(const char*& p);

struct TextSize {
    int cells = 0; ///< the widest line, in characters
    int lines = 0; ///< 0 for empty text
    int total = 0; ///< characters of every line (what a non-wrapping label reveals)
};

/// The code point at `p` and moves past it: ASCII inline, the rest through detail::next_codepoint (same results).
inline char32_t seam_next(const char*& p) {
    const auto c = static_cast<unsigned char>(*p);
    if (c != 0 && c < 0x80) {
        ++p;
        return c;
    }
    return c == 0 ? char32_t{0} : seam_next_slow(p);
}

/// Cells and lines of `utf8` without wrapping ('\n' starts a line, '\r' takes no cell).
TextSize seam_measure(const char* utf8);
/// Pixels of one character cell and between lines for a text look (inline: called for every widget in layout).
inline int seam_scale(const UiTextLook& look) { return look.scale < 1 ? 1 : (look.scale > 8 ? 8 : look.scale); }
inline int seam_cell(const UiTextLook& look) { return kFontCellSize * seam_scale(look); }
inline int seam_line(const UiTextLook& look) { return kFontLineHeight * seam_scale(look); }

/// The text of a value of slider or progress `s` (its formatter, else the number; a progress bar adds "/max").
/// @return the bytes written into `out` (no NUL; at most `capacity`).
int value_text(UiBlock& b, int s, int value, char* out, int capacity);

} // namespace nnh::detail
