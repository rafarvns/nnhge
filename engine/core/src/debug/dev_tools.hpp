#pragma once
// Developer tools driven by the frame loop: profiler frame boundary and the debug overlay. Internal.

#include <cstdint>

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

} // namespace nnh::detail
