#pragma once
// Hooks the frame loop uses to feed the input module. Internal.

#include <cstdint>

namespace nnh::platform {
struct InputSample;
}

namespace nnh::detail {

/// Forgets all input state and turns the D-Pad mirror off (called by nnh::init(), after platform init).
void input_reset();

/// Feeds this frame's sample (called once per nnh::frame()).
void input_update(const platform::InputSample& sample);

/// Buttons that went down this frame, as a mask.
std::uint32_t input_pressed_mask();

/// Publishes this frame's ButtonPressed/ButtonReleased, then TouchStarted/TouchEnded (called by
/// nnh::frame() before dispatch).
void input_publish_events();

} // namespace nnh::detail
