// PC simulator: keyboard → 3DS buttons. Keep in sync with docs/api/input.md.

#include <SDL3/SDL.h>

#include "nnh/input.hpp"
#include "nnh/platform/input.hpp"

namespace nnh::platform {
namespace {

struct KeyBinding {
    SDL_Scancode key;
    Button button;
};

constexpr KeyBinding kKeyBindings[] = {
    {SDL_SCANCODE_X, Button::A},          {SDL_SCANCODE_Z, Button::B},
    {SDL_SCANCODE_S, Button::X},          {SDL_SCANCODE_A, Button::Y},
    {SDL_SCANCODE_Q, Button::L},          {SDL_SCANCODE_W, Button::R},
    {SDL_SCANCODE_1, Button::ZL},         {SDL_SCANCODE_2, Button::ZR},
    {SDL_SCANCODE_RETURN, Button::Start}, {SDL_SCANCODE_BACKSPACE, Button::Select},
    {SDL_SCANCODE_UP, Button::Up},        {SDL_SCANCODE_DOWN, Button::Down},
    {SDL_SCANCODE_LEFT, Button::Left},    {SDL_SCANCODE_RIGHT, Button::Right},
};

} // namespace

std::uint32_t read_buttons() {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    std::uint32_t mask = 0;
    for (const KeyBinding& binding : kKeyBindings) {
        if (keys[binding.key]) mask |= static_cast<std::uint32_t>(binding.button);
    }
    return mask;
}

} // namespace nnh::platform
