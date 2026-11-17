// Button queries (nnh/input.hpp). Edge detection lives in input_state.hpp.

#include "nnh/input.hpp"

#include "input/input_internal.hpp"
#include "input/input_state.hpp"
#include "nnh/events.hpp"

namespace nnh {
namespace {

detail::InputState g_input;

bool has(std::uint32_t mask, Button button) { return (mask & static_cast<std::uint32_t>(button)) != 0; }

} // namespace

void detail::input_reset() { g_input = {}; }
void detail::input_update(std::uint32_t held_mask) { g_input.update(held_mask); }
std::uint32_t detail::input_pressed_mask() { return g_input.pressed(); }

void detail::input_publish_events() {
    // Presses first, then releases, each in Button bit order (A, B, X, Y, ...).
    for (std::uint32_t bits = g_input.pressed(); bits != 0; bits &= bits - 1) {
        events::publish(ButtonPressed{static_cast<Button>(bits & (~bits + 1))});
    }
    for (std::uint32_t bits = g_input.released(); bits != 0; bits &= bits - 1) {
        events::publish(ButtonReleased{static_cast<Button>(bits & (~bits + 1))});
    }
}

bool input::held(Button button) { return has(g_input.held, button); }
bool input::pressed(Button button) { return has(g_input.pressed(), button); }
bool input::released(Button button) { return has(g_input.released(), button); }
std::uint32_t input::held_mask() { return g_input.held; }

} // namespace nnh
