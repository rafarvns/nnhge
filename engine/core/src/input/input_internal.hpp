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

/// The input block's filter (input_block.cpp, spec 023 FR-12): null unless a block is on or a control is still
/// filtered, so games that never block pay one load and a branch per frame. When set, input_update() hands it the raw
/// sample; it filters a copy and feeds it through input_apply() (then input_forget_edges() while blocking).
extern void (*g_input_filter)(const platform::InputSample& raw);

/// The module's state from a (filtered) sample: sticks, D-Pad mirror, buttons, touch.
void input_apply(const platform::InputSample& sample);
/// Makes this frame's state edge-free (nothing pressed, released, started or ended): a blocked frame.
void input_forget_edges();

/// Raw buttons held / gone down this frame, for engine controls (the debug overlay's SELECT + L): never blocked.
std::uint32_t input_engine_held_mask();
std::uint32_t input_engine_pressed_mask();

/// Publishes this frame's ButtonPressed/ButtonReleased, then TouchStarted/TouchEnded (called by
/// nnh::frame() before dispatch).
void input_publish_events();

} // namespace nnh::detail
