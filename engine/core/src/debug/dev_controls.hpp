#pragma once
// Engine controls for the overlay's pages and the pause (spec 030 FR-8, FR-10, D52): held, pressed and released buttons
// per tick from the post-remote sample (so a script's input drives them and, in exclusive mode, the console's own
// controls don't), repeat after 24 ticks then every 6 (D48's values), touch down/held/up. Works on paused ticks, where
// the input module is not updated. Pure. Tests: tests/debug/test_dev_controls.cpp.

#include <cstdint>

namespace nnh::platform {
struct InputSample;
}

namespace nnh::detail {

inline constexpr int kDevRepeatDelay = 24;
inline constexpr int kDevRepeatEvery = 6;

/// One tick of engine controls.
struct DevInput {
    std::uint32_t held = 0;
    std::uint32_t pressed = 0;  ///< fresh presses this tick
    std::uint32_t released = 0;
    std::uint32_t repeated = 0; ///< fresh presses and repeats of held buttons this tick
    bool touching = false;
    bool touch_started = false;
    bool touch_ended = false;
    int touch_x = 0, touch_y = 0; ///< while touching; the last point on touch_ended
};

class DevControls {
public:
    DevInput update(const platform::InputSample& sample);
    void reset();

private:
    std::uint32_t previous_ = 0;
    std::uint16_t ticks_[16] = {}; ///< ticks each button has been held
    bool touching_ = false;
    std::int16_t last_x_ = 0, last_y_ = 0;
};

} // namespace nnh::detail
