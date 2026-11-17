#pragma once
// Hooks the frame loop uses to feed the input module. Internal.

#include <cstdint>

namespace nnh::detail {

/// Forgets all button state (called by nnh::init()).
void input_reset();

/// Feeds this frame's held-button mask (called once per nnh::frame()).
void input_update(std::uint32_t held_mask);

/// Buttons that went down this frame, as a mask.
std::uint32_t input_pressed_mask();

} // namespace nnh::detail
