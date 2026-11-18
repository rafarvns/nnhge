// PC simulator: keyboard, gamepad (gamepad_pc.cpp) and mouse (touch_pc.cpp) → 3DS controls.
// Keep in sync with docs/api/input.md.

#include <SDL3/SDL.h>

#include "context_pc.hpp"
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

/// Four keys as a stick: full deflection, diagonals scaled to length 1.
struct StickKeys {
    SDL_Scancode up, down, left, right;
};
constexpr StickKeys kCirclePadKeys{SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L};
constexpr StickKeys kCStickKeys{SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_6};

void read_stick_keys(const bool* keys, const StickKeys& k, float& x, float& y) {
    x = static_cast<float>(keys[k.right]) - static_cast<float>(keys[k.left]);
    y = static_cast<float>(keys[k.up]) - static_cast<float>(keys[k.down]);
    if (x != 0 && y != 0) {
        x *= 0.70710678f;
        y *= 0.70710678f;
    }
}

} // namespace

void read_input(InputSample& out) {
    out = InputSample{};
    if (pc::input_override(out)) return;

    const bool* keys = SDL_GetKeyboardState(nullptr);
    for (const KeyBinding& binding : kKeyBindings) {
        if (keys[binding.key]) out.buttons |= static_cast<std::uint32_t>(binding.button);
    }
    float stick[4] = {};
    read_stick_keys(keys, kCirclePadKeys, stick[0], stick[1]);
    read_stick_keys(keys, kCStickKeys, stick[2], stick[3]);
    pc::gamepad_read(out.buttons, stick);
    out.circle_x = stick[0];
    out.circle_y = stick[1];
    out.cstick_x = stick[2];
    out.cstick_y = stick[3];
    pc::touch_read(out.touching, out.touch_x, out.touch_y);
}

} // namespace nnh::platform
