#pragma once
// Developer tools driven by the frame loop: profiler frame boundary, debug overlay, debug text. Internal.

#include <cstdint>

#include "nnh/gfx.hpp"
#include "nnh/time.hpp"

namespace nnh::detail {

/// Publishes this frame's profile scopes as "previous frame" and starts a new table.
void profile_frame_boundary();
void profile_reset();

/// Called once per frame after input is read: records the frame time and handles the toggles
/// (SELECT + L, or a platform developer request).
void overlay_update(Duration frame_delta, std::uint32_t held_buttons, std::uint32_t pressed_buttons,
                    std::uint32_t platform_dev_requests);

/// Draws the overlay into the open frame if it's visible (called right before the frame ends).
void overlay_draw();

void overlay_reset();

/// Records debug text / an overlay rectangle for this frame's debug pass.
void debug_record_text(Screen screen, int x, int y, const char* text, Color color);
void debug_record_rect(Screen screen, int x, int y, int w, int h, Color color);

/// The debug pass: draws what was recorded this frame with the built-in font (after the 2D passes, so
/// it stays on top) and starts a new list.
void debug_text_flush();
/// Forgets recorded debug output (init).
void debug_text_clear();

class DebugList;
/// Submits a debug list (debug_pass.cpp).
void debug_pass_submit(const DebugList& list);
/// What the previous frame's debug pass drew (for tests).
const DebugList& debug_list_last_frame();

} // namespace nnh::detail
