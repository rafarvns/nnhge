// PC simulator: the first connected gamepad → 3DS controls (SDL3 gamepad API). Keep in sync with
// docs/api/input.md. Face buttons map by position, so any pad plays like the 3DS layout.

#include <SDL3/SDL.h>

#include "context_pc.hpp"
#include "nnh/input.hpp"

namespace nnh::platform {
namespace {

struct PadBinding {
    SDL_GamepadButton pad;
    Button button;
};

constexpr PadBinding kPadBindings[] = {
    {SDL_GAMEPAD_BUTTON_EAST, Button::A},           {SDL_GAMEPAD_BUTTON_SOUTH, Button::B},
    {SDL_GAMEPAD_BUTTON_NORTH, Button::X},          {SDL_GAMEPAD_BUTTON_WEST, Button::Y},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, Button::L},  {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, Button::R},
    {SDL_GAMEPAD_BUTTON_START, Button::Start},      {SDL_GAMEPAD_BUTTON_BACK, Button::Select},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, Button::Up},       {SDL_GAMEPAD_BUTTON_DPAD_DOWN, Button::Down},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, Button::Left},   {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, Button::Right},
};

constexpr Sint16 kTriggerPressed = 16384; // half of the trigger's travel

float axis(SDL_Gamepad* pad, SDL_GamepadAxis which) { return SDL_GetGamepadAxis(pad, which) / 32767.0f; }

/// Keeps the reading with the larger deflection (keyboard vs gamepad).
void keep_stronger(float& x, float& y, float new_x, float new_y) {
    if (new_x * new_x + new_y * new_y > x * x + y * y) {
        x = new_x;
        y = new_y;
    }
}

void open_first_available() {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    for (int i = 0; ids != nullptr && i < count && pc::context().gamepad == nullptr; ++i) {
        pc::context().gamepad = SDL_OpenGamepad(ids[i]);
    }
    SDL_free(ids);
}

} // namespace

void pc::gamepad_event(const SDL_Event& event) {
    Context& ctx = context();
    if (event.type == SDL_EVENT_GAMEPAD_ADDED && ctx.gamepad == nullptr) {
        ctx.gamepad = SDL_OpenGamepad(event.gdevice.which);
    } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED && ctx.gamepad != nullptr &&
               SDL_GetGamepadID(ctx.gamepad) == event.gdevice.which) {
        gamepad_close();
        open_first_available(); // another pad still connected takes over
    }
}

void pc::gamepad_close() {
    Context& ctx = context();
    if (ctx.gamepad != nullptr) SDL_CloseGamepad(ctx.gamepad);
    ctx.gamepad = nullptr;
}

void pc::gamepad_read(std::uint32_t& buttons, float stick[4]) {
    SDL_Gamepad* pad = context().gamepad;
    if (pad == nullptr) return;
    for (const PadBinding& binding : kPadBindings) {
        if (SDL_GetGamepadButton(pad, binding.pad)) buttons |= static_cast<std::uint32_t>(binding.button);
    }
    if (SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > kTriggerPressed) {
        buttons |= static_cast<std::uint32_t>(Button::ZL);
    }
    if (SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > kTriggerPressed) {
        buttons |= static_cast<std::uint32_t>(Button::ZR);
    }
    // SDL's stick y grows down; the engine's grows up.
    keep_stronger(stick[0], stick[1], axis(pad, SDL_GAMEPAD_AXIS_LEFTX), -axis(pad, SDL_GAMEPAD_AXIS_LEFTY));
    keep_stronger(stick[2], stick[3], axis(pad, SDL_GAMEPAD_AXIS_RIGHTX), -axis(pad, SDL_GAMEPAD_AXIS_RIGHTY));
}

} // namespace nnh::platform
