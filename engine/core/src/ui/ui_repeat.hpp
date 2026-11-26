#pragma once
// Held-control repeat (spec 028 FR-22): a direction or page control fires on its press, then at `delay`, `delay +
// interval`, ... frames held. One direction per frame: the newest pressed of those held (pressed in the same frame:
// Up, Down, Left, Right first). Confirm and cancel fire only on their press. With delay 0 the input's own `repeated`
// bits are taken as given. Pure; unit tested in tests/ui/test_repeat.cpp.

#include <cstdint>

namespace nnh::detail {

struct RepeatState {
    std::uint16_t frames[8] = {};  ///< frames held since the press (valid while pressed_at != 0)
    std::uint32_t pressed_at[8] = {}; ///< the press's sequence number, 0: not held since a press
    std::uint32_t sequence = 0;
};

struct RepeatFired {
    std::uint8_t fresh = 0;  ///< fired by a press this frame
    std::uint8_t repeat = 0; ///< fired by holding
};

RepeatFired repeat_step(RepeatState& s, std::uint8_t held, std::uint8_t pressed, std::uint8_t given_repeats,
                        int delay, int interval);

} // namespace nnh::detail
