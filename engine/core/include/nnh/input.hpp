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

/// Event published on the frame the bottom screen starts being touched, after the button events.
/// @code
/// nnh::Subscription sub = nnh::events::subscribe<nnh::TouchStarted>([this](const nnh::TouchStarted& e) {
///     if (play_button_.contains(e.position)) start_game();
/// });
/// @endcode
struct TouchStarted {
    TouchPoint position;
};

/// Event published on the frame the touch ends; `position` is the last touched point.
struct TouchEnded {
    TouchPoint position;
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

/// Circle Pad position this frame (see Stick).
/// @code
/// const nnh::Stick pad = nnh::input::circle_pad();
/// player.x += pad.x * kRunSpeed;   // pushed halfway → half speed
/// player.y -= pad.y * kRunSpeed;   // stick y grows up, screen y grows down
/// @endcode
Stick circle_pad();

/// C-Stick position this frame. Always (0, 0) on systems without one: check
/// nnh::system::has(nnh::Capability::CStick) (<nnh/system.hpp>) to offer another control instead.
Stick c_stick();

/// True while the bottom screen is touched this frame.
bool touching();

/// True only on the frame the touch started (like pressed()).
bool touch_started();

/// True only on the frame the touch ended (like released()).
bool touch_ended();

/// Where the bottom screen is touched; while not touching, the last touched point ((0, 0) before the
/// first touch).
TouchPoint touch_position();

/// Off by default (and at every init()): Up/Down/Left/Right are the D-Pad only. When on, pushing the
/// Circle Pad past half its range also holds the matching direction — with pressed()/released() and
/// ButtonPressed/ButtonReleased like a real D-Pad press; it lets go below 40%, so it doesn't flicker
/// around the threshold. Handy for menus. Takes effect from the next nnh::frame().
/// @code
/// nnh::input::set_circle_pad_mirrors_dpad(true);
/// if (nnh::input::pressed(nnh::Button::Down)) menu.next();   // D-Pad or Circle Pad
/// @endcode
void set_circle_pad_mirrors_dpad(bool enabled);
bool circle_pad_mirrors_dpad();

/// Blocks the game's input (cut-scenes, dialogs): from the next nnh::frame(), every query reads idle (no button
/// held, sticks centred, no touch, no edges) and no ButtonPressed/ButtonReleased/TouchStarted/TouchEnded or action
/// event is queued — remote input from `nnh run --script` included. When unblocked, every control still held stays
/// blocked until it is released (so a press held through a dialog never reaches the game); touch_position() keeps
/// the last point seen before. Engine controls (the debug overlay's SELECT + L, the simulator's keys) are never
/// blocked. A scene transition's block (<nnh/transition.hpp>) is separate: one never lifts the other. Lifted by
/// init(). Cheap; no allocation. A game that never calls it pays one pointer check per frame.
/// @code
/// nnh::input::set_blocked(true);   // the cut-scene starts
/// nnh::input::set_blocked(false);  // it ended: A, still held from skipping it, waits for its release
/// @endcode
void set_blocked(bool blocked);
/// True while the game or a transition blocks input.
bool blocked();

} // namespace input
} // namespace nnh
