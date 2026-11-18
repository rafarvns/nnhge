#pragma once
/// @file nnh/input.hpp
/// Buttons. Input is sampled once per nnh::frame(), so all queries in a frame agree.
/// See docs/api/input.md for the PC keyboard mapping.

#include <cstdint>

namespace nnh {

/// Console buttons. Values are bit flags and can be combined for held_mask() checks.
enum class Button : std::uint32_t {
    A = 1u << 0,
    B = 1u << 1,
    X = 1u << 2,
    Y = 1u << 3,
    L = 1u << 4,
    R = 1u << 5,
    ZL = 1u << 6, ///< New 3DS only.
    ZR = 1u << 7, ///< New 3DS only.
    Start = 1u << 8,
    Select = 1u << 9,
    Up = 1u << 10,    ///< D-Pad.
    Down = 1u << 11,  ///< D-Pad.
    Left = 1u << 12,  ///< D-Pad.
    Right = 1u << 13, ///< D-Pad.
};

/// Analog stick position: x grows to the right, y grows up, each in [-1, 1]; (0, 0) at rest.
/// A small dead zone around the center reads exactly (0, 0); the length never exceeds 1.
struct Stick {
    float x = 0;
    float y = 0;
};

/// A point on the bottom (touch) screen, in pixels: x 0-319, y 0-239, (0, 0) top-left.
struct TouchPoint {
    int x = 0;
    int y = 0;
};

/// Event published once per button that went down this frame (see <nnh/events.hpp>):
/// @code
/// nnh::Subscription sub = nnh::events::subscribe<nnh::ButtonPressed>([this](const nnh::ButtonPressed& e) {
///     if (e.button == nnh::Button::Start) paused_ = true;
/// });
/// @endcode
struct ButtonPressed {
    Button button;
};

/// Event published once per button that went up this frame.
struct ButtonReleased {
    Button button;
};

namespace input {

/// True while the button is down this frame.
bool held(Button button);

/// True only on the frame the button went down.
/// @code
/// if (nnh::input::pressed(nnh::Button::A)) jump();
/// @endcode
bool pressed(Button button);

/// True only on the frame the button went up.
bool released(Button button);

/// All buttons held this frame, as a mask of Button bits.
std::uint32_t held_mask();

} // namespace input
} // namespace nnh
