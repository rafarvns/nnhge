#pragma once
// What an action set reads each frame (plan 017): the frame's input sample as seen through the public
// <nnh/input.hpp> queries (remote input merged, D-Pad mirror applied), with stick pushes and lengths
// precomputed once so that slot evaluation needs no sqrt.

#include "nnh/input.hpp"

#include <cstdint>

namespace nnh::detail {

struct InputView {
    std::uint32_t held = 0; ///< Button bits
    Stick stick[2];         ///< AnalogStick order: Circle Pad, C-Stick
    float push[8] = {};     ///< stick * 4 + Direction (Up, Down, Left, Right): the push that way, >= 0
    float length[2] = {};   ///< of each stick
    bool touching = false;
    int touch_x = 0, touch_y = 0;
};

/// Pure: the view of these readings.
InputView make_input_view(std::uint32_t held, Stick circle_pad, Stick c_stick, bool touching, TouchPoint touch);

/// The view of this frame's public input queries.
InputView current_input_view();

} // namespace nnh::detail
