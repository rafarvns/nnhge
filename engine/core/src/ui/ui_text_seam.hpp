#pragma once
// The UI's text seam (spec 028 FR-24, spec 029 FR-14): every measure of widget text goes through here, drawing through
// ui_draw_out's run. Sizes are pixels. A style's text names the built-in font (spec 005's grid: cells of kFontCellSize ×
// scale, lines kFontLineHeight × scale apart — computed exactly as spec 028 did, so built-in layouts and draw hashes
// don't change) or one of the UI's font slots (Ui::set_font), measured through the shared run code of the font's face.

#include "nnh/ui.hpp"
#include "text/text_face.hpp"

namespace nnh::detail {

struct UiBlock;

/// detail::next_codepoint (text/utf8.hpp), for seam_next's non-ASCII bytes.
char32_t seam_next_slow(const char*& p);

struct TextSize {
    int width = 0; ///< the widest line, pixels at the look's scale
    int lines = 0; ///< 0 for empty text
    int total = 0; ///< characters of every line but '\r' (what a non-wrapping label reveals)
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

inline int seam_scale(const UiTextLook& look) { return look.scale < 1 ? 1 : (look.scale > 8 ? 8 : look.scale); }
inline bool seam_builtin(const UiTextLook& look) { return look.font == UiFont::Builtin; }
/// The built-in font's cell and line (inline: called for every widget in layout).
inline int seam_cell(const UiTextLook& look) { return kFontCellSize * seam_scale(look); }

/// The face of a look: the built-in font, or its font slot's font (a stale or empty slot: the built-in font).
TextFace seam_face(const UiBlock& b, const UiTextLook& look);
/// Pixels between lines.
int seam_line(const UiBlock& b, const UiTextLook& look);
/// Widest line, lines and characters of `utf8` without wrapping ('\n' starts a line, '\r' takes no room).
TextSize seam_measure(const UiBlock& b, const UiTextLook& look, const char* utf8);
/// Pixels of the first `bytes` bytes of a one-line text.
int seam_width(const UiBlock& b, const UiTextLook& look, const char* utf8, int bytes);
/// Bytes of `utf8` that fit `width` pixels on one line (whole code points).
int seam_fit(const UiBlock& b, const UiTextLook& look, const char* utf8, int width);

/// The text of a value of slider or progress `s` (its formatter, else the number; a progress bar adds "/max").
/// @return the bytes written into `out` (no NUL; at most `capacity`).
int value_text(UiBlock& b, int s, int value, char* out, int capacity);

} // namespace nnh::detail
