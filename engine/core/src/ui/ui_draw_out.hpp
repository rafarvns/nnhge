#pragma once
// The UI's only gfx caller (spec 028 FR-28, D48): rectangles, outlines, sprites and text runs, always ignore_camera on
// the root's layer, each a public gfx call exactly as a hand-written menu would make it. Every call also folds its
// arguments into the draw hash and counts its draws (glyphs as gfx counts them) — computed outside a frame too.

#include "ui/ui_block.hpp"

namespace nnh::detail {

/// Where and how the current widget draws: the screen, layer, render offset and opacity accumulated down the tree.
struct DrawCtx {
    Screen screen = Screen::Top;
    int layer = kUiDefaultLayer;
    int dx = 0, dy = 0;
    int opacity = 255;
};

/// `c` with its alpha × opacity / 255 (rounded).
Color fade(Color c, int opacity);

void out_fill(const UiBlock& b, const DrawCtx& d, const UiRect& r, Color color);
void out_outline(const UiBlock& b, const DrawCtx& d, const UiRect& r, Color color);
/// A region of texture slot `slot` with its top-left at (x, y), scaled.
void out_sprite(const UiBlock& b, const DrawCtx& d, int slot, const Region& src, int x, int y, float sx, float sy,
                Color tint);
/// `length` bytes of UTF-8 at (x, y) (top-left of the line), at most kUiRunBytes; "..." appended when `ellipsis`.
void out_run(const UiBlock& b, const DrawCtx& d, int x, int y, const char* utf8, int length, const UiTextLook& look,
             Color color, bool ellipsis = false);
/// A background: nothing, a solid fill (+ border) or nine sprites.
void out_background(const UiBlock& b, const DrawCtx& d, const UiRect& r, const UiBackground& bg);

} // namespace nnh::detail
