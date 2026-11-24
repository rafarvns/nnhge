// Input queries (nnh/input.hpp). The pure parts live in input_state.hpp (button edges), stick.hpp
// (dead zone), touch_state.hpp (touch edges) and dpad_mirror.hpp (Circle Pad → D-Pad).

#include "nnh/input.hpp"

#include "input/dpad_mirror.hpp"
#include "input/input_internal.hpp"
#include "input/input_state.hpp"
#include "input/stick.hpp"
#include "input/touch_state.hpp"
#include "nnh/events.hpp"
#include "nnh/platform/input.hpp"
#include "nnh/system.hpp"

namespace nnh {
namespace {

struct Input {
    detail::InputState buttons;
    detail::InputState engine; // raw buttons, for engine controls (never blocked)
    detail::TouchState touch;
    Stick circle_pad;
    Stick c_stick;
    std::uint32_t mirrored = 0; // D-Pad bits held by the mirror last frame
    bool mirror_enabled = false;
    bool has_c_stick = false;
};
static_assert(sizeof(Input) <= 64, "input state stays small (spec 003 FR-11)");

Input g_input;

bool has(std::uint32_t mask, Button button) { return (mask & static_cast<std::uint32_t>(button)) != 0; }

} // namespace

void (*detail::g_input_filter)(const platform::InputSample&) = nullptr;

void detail::input_reset() {
    g_input = {};
    g_input.has_c_stick = system::has(Capability::CStick);
    g_input_filter = nullptr; // a block doesn't survive shutdown() and init()
}

void detail::input_apply(const platform::InputSample& sample) {
    g_input.circle_pad = shape_stick(sample.circle_x, sample.circle_y);
    g_input.c_stick = g_input.has_c_stick ? shape_stick(sample.cstick_x, sample.cstick_y) : Stick{};
    g_input.mirrored = g_input.mirror_enabled ? dpad_mirror(g_input.circle_pad, g_input.mirrored) : 0;
    g_input.buttons.update(sample.buttons | g_input.mirrored);
    g_input.touch.update(sample.touching, sample.touch_x, sample.touch_y);
}

void detail::input_forget_edges() {
    g_input.buttons.previous = g_input.buttons.held;
    g_input.touch.previous = g_input.touch.touching;
}

void detail::input_update(const platform::InputSample& raw) {
    g_input.engine.update(raw.buttons);
    if (g_input_filter != nullptr) [[unlikely]] { // the input block (spec 023): one load and a branch otherwise
        g_input_filter(raw);
        return;
    }
    input_apply(raw);
}

std::uint32_t detail::input_engine_held_mask() { return g_input.engine.held; }
std::uint32_t detail::input_engine_pressed_mask() { return g_input.engine.pressed(); }

void detail::input_publish_events() {
    // Presses first, then releases, each in Button bit order (A, B, X, Y, ...); then touch.
    for (std::uint32_t bits = g_input.buttons.pressed(); bits != 0; bits &= bits - 1) {
        events::publish(ButtonPressed{static_cast<Button>(bits & (~bits + 1))});
    }
    for (std::uint32_t bits = g_input.buttons.released(); bits != 0; bits &= bits - 1) {
        events::publish(ButtonReleased{static_cast<Button>(bits & (~bits + 1))});
    }
    if (g_input.touch.started()) events::publish(TouchStarted{g_input.touch.position});
    if (g_input.touch.ended()) events::publish(TouchEnded{g_input.touch.position});
}

bool input::held(Button button) { return has(g_input.buttons.held, button); }
bool input::pressed(Button button) { return has(g_input.buttons.pressed(), button); }
bool input::released(Button button) { return has(g_input.buttons.released(), button); }
std::uint32_t input::held_mask() { return g_input.buttons.held; }

Stick input::circle_pad() { return g_input.circle_pad; }
Stick input::c_stick() { return g_input.c_stick; }

bool input::touching() { return g_input.touch.touching; }
bool input::touch_started() { return g_input.touch.started(); }
bool input::touch_ended() { return g_input.touch.ended(); }
TouchPoint input::touch_position() { return g_input.touch.position; }

void input::set_circle_pad_mirrors_dpad(bool enabled) { g_input.mirror_enabled = enabled; }
bool input::circle_pad_mirrors_dpad() { return g_input.mirror_enabled; }

} // namespace nnh
