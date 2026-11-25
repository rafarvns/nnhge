#pragma once
// The UI's text seam (spec 028 FR-24): every measure of widget text goes through here, drawing through ui_draw_out's
// run. Its only font today is spec 005's built-in grid (cells of kFontCellSize × scale, lines kFontLineHeight × scale
// apart); the game-fonts spec adds fonts behind UiTextLook::font without changing widget code.

#include "nnh/ui.hpp"

namespace nnh::detail {

struct UiBlock;

struct TextSize {
    int cells = 0; ///< the widest line, in characters
    int lines = 0; ///< 0 for empty text
};

/// Cells and lines of `utf8` without wrapping ('\n' starts a line, '\r' takes no cell).
TextSize seam_measure(const char* utf8);
/// Pixels of one character cell and between lines for a text look.
int seam_cell(const UiTextLook& look);
int seam_line(const UiTextLook& look);

/// The text of a value of slider or progress `s` (its formatter, else the number; a progress bar adds "/max").
/// @return the bytes written into `out` (no NUL; at most `capacity`).
int value_text(UiBlock& b, int s, int value, char* out, int capacity);

} // namespace nnh::detail
