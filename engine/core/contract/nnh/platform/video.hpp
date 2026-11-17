#pragma once
/// @file nnh/platform/video.hpp
/// Platform contract — frames and drawing. Implemented by every platform library, called only by
/// nnh::core. Not for games. See docs/architecture/platforms.md.

#include "nnh/gfx.hpp"

namespace nnh::platform {

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
