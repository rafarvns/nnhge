#pragma once
/// @file nnh/platform/video.hpp
/// Platform contract — frames and drawing. Implemented by every platform library, called only by
/// nnh::core. Not for games. See docs/architecture/platforms.md.

#include <cstdint>

#include "nnh/gfx.hpp"
#include "nnh/math.hpp"

namespace nnh::platform {

/// A texture as the platform knows it (0 = none). See docs/architecture/rendering.md.
using TextureHandle = std::uintptr_t;

/// One 2D draw of a pass, ready for the GPU: sorted, camera applied, positions in whole screen pixels.
struct DrawItem2D {
    enum class Kind : std::uint8_t { Sprite, FillRect, RectOutline, Line };
    Kind kind = Kind::Sprite;
    std::uint8_t layer = 0; ///< 0..15; kept for stereoscopic depth later (rendering.md)
    bool flip_x = false;    ///< Sprite: mirror the region horizontally
    bool flip_y = false;
    TextureHandle texture = 0; ///< Sprite
    Region region{};           ///< Sprite: source pixels in the texture (never empty here)
    /// Sprite: (x, y) = where the pivot lands, (w, h) = destination size.
    /// FillRect/RectOutline: top-left and size. Line: from (x, y) to (w, h).
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;
    float pivot_x = 0; ///< Sprite: pivot in destination pixels from the destination's top-left
    float pivot_y = 0;
    float rotation = 0; ///< Sprite: radians, clockwise on screen, around the pivot
    Color color{};      ///< Sprite: tint (white = unchanged), alpha = opacity. Shapes: the color.
};

/// Starts a frame with both screens cleared to the given colors. Called once per nnh::frame(), after
/// poll_events() returned true. Drawing functions are valid until end_frame(). Must not allocate.
void begin_frame(Color top_clear, Color bottom_clear);

/// Submits the frame and waits until the next 60 Hz frame slot (vsync). Called exactly once after each
/// begin_frame(). Must not allocate.
void end_frame();

/// Draws debug text with its top-left corner at (x, y) in screen pixels; '\n' starts a new line
/// (line_height() apart). Only called between begin_frame() and end_frame(). Text outside the screen is
/// clipped. Must not allocate.
void debug_text(Screen screen, int x, int y, const char* text, Color color);

/// Height in pixels of one line of debug text. Always > 0 once init() succeeded.
int debug_line_height();

/// Fills a rectangle (screen pixels, clipped to the screen), alpha-blended. Only between begin_frame()
/// and end_frame(). Used by the debug overlay. Must not allocate.
void fill_rect(Screen screen, int x, int y, int width, int height, Color color);

} // namespace nnh::platform
