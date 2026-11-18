#pragma once
/// @file nnh/platform/input.hpp
/// Platform contract — input. Implemented by every platform library, called only by nnh::core.
/// Not for games. See docs/architecture/platforms.md.

#include <cstdint>

namespace nnh::platform {

/// Every control, sampled at one moment. Core applies the dead zone, clamping, touch edges and the
/// Circle Pad → D-Pad mirror; the platform only reports what the hardware (or the simulator) sees.
struct InputSample {
    /// Buttons held, as nnh::Button bits (<nnh/input.hpp>). Up/Down/Left/Right are the D-Pad only.
    /// Bits for buttons the system doesn't have (e.g. ZL on Old 3DS) are never set.
    std::uint32_t buttons = 0;
    /// Sticks scaled by the platform to about [-1, 1] per axis (its full range), x right, y up, no dead
    /// zone. 0 for a stick the system doesn't have.
    float circle_x = 0;
    float circle_y = 0;
    float cstick_x = 0;
    float cstick_y = 0;
    /// Bottom screen touched, and where (pixels, x 0-319, y 0-239), valid while touching.
    bool touching = false;
    std::int16_t touch_x = 0;
    std::int16_t touch_y = 0;
};

/// Fills `out` with this frame's input. Called once per frame by nnh::frame(), after poll_events().
/// Reports nothing held, sticks at 0 and no touch when there is no input. Must not allocate.
void read_input(InputSample& out);

} // namespace nnh::platform
