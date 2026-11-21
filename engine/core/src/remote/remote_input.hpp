#pragma once
// Console remote input (spec 013 FR-1): the controls the PC holds, and their merge into the frame's platform
// sample before core reads it (so queries, events, dead zone and D-Pad mirror work unchanged). Pure, no heap.
// Tests: tests/remote/test_remote_input.cpp.

#include <cstdint>

#include "nnh/platform/input.hpp"
#include "remote/remote_wire.hpp"

namespace nnh::detail::remote {

struct RemoteInput {
    std::uint32_t buttons = 0;  ///< nnh::Button bits held by the PC
    std::int16_t circle_x = 0;  ///< thousandths, y up
    std::int16_t circle_y = 0;
    std::int16_t cstick_x = 0;
    std::int16_t cstick_y = 0;
    bool touching = false;
    std::int16_t touch_x = 0;
    std::int16_t touch_y = 0;

    /// Applies a decoded Buttons (hold += press, −= release), Stick or Touch message; other types change nothing.
    void apply(const Message& message);

    /// Lets every remote control go: no buttons, sticks centred, touch up (Bye, disconnect).
    void release_all() { *this = RemoteInput{}; }

    /// Merges into the platform's reading: buttons are OR-ed in; each stick takes whichever of real and remote
    /// is pushed further (ties keep the real one); a remote touch replaces the real one while it is down.
    void merge_into(platform::InputSample& sample) const;
};

} // namespace nnh::detail::remote
