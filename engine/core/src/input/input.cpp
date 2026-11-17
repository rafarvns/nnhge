// Button queries (nnh/input.hpp). Edge detection lives in input_state.hpp.

#include "nnh/input.hpp"

#include "input/input_internal.hpp"
#include "input/input_state.hpp"

namespace nnh {
namespace {

detail::InputState g_input;

bool has(std::uint32_t mask, Button button) { return (mask & static_cast<std::uint32_t>(button)) != 0; }

} // namespace

void detail::input_reset() { g_input = {}; }
void detail::input_update(std::uint32_t held_mask) { g_input.update(held_mask); }

bool input::held(Button button) { return has(g_input.held, button); }
bool input::pressed(Button button) { return has(g_input.pressed(), button); }
bool input::released(Button button) { return has(g_input.released(), button); }
std::uint32_t input::held_mask() { return g_input.held; }

} // namespace nnh
