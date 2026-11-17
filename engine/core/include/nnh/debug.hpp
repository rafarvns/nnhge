#pragma once
/// @file nnh/debug.hpp
/// Debug text drawn on top of a screen. Meant for development, not for game UI:
/// the font differs per platform (3DS system font vs. an 8x8 font on PC), so measure with
/// line_height() instead of assuming a size. See docs/api/debug.md.

#include "nnh/gfx.hpp"

#include "nnh/detail/printf_format.hpp"

namespace nnh::debug {

/// Draws text with its top-left corner at (x, y) in screen pixels. '\n' starts a new line.
/// Only valid inside a frame (after nnh::frame() returned true); ignored otherwise.
/// Per-frame safe, no heap allocation.
/// @code
/// nnh::debug::print(nnh::Screen::Top, 8, 8, "Hello!");
/// @endcode
void print(Screen screen, int x, int y, const char* text, Color color = colors::white);

/// printf-style variant of print(). Output longer than 255 characters is truncated.
/// @code
/// nnh::debug::printf(nnh::Screen::Bottom, 8, 8, "frame %u", unsigned(nnh::frame_count()));
/// @endcode
void printf(Screen screen, int x, int y, const char* format, ...) NNH_PRINTF_FORMAT(4, 5);

/// Height in pixels of one line of debug text on the current platform.
int line_height();

/// Shows or hides the debug overlay (FPS, frame-time graph, memory, slowest profile scopes) on the
/// bottom screen. Players toggle it with SELECT + L (3DS) or F1 (PC simulator). Always hidden in Release.
void set_overlay_visible(bool visible);
bool overlay_visible();

} // namespace nnh::debug
