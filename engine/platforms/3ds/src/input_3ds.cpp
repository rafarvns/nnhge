// Nintendo 3DS: hardware input (libctru HID, and IRRST for the New 3DS C-Stick) → InputSample.

#include <3ds.h>

#include "context_3ds.hpp"
#include "nnh/input.hpp"
#include "nnh/platform/input.hpp"

namespace nnh::platform {
namespace {

struct KeyBinding {
    u32 key;
    Button button;
};

// D-Pad keys only (KEY_DUP, not KEY_UP, which also includes the Circle Pad).
constexpr KeyBinding kKeyBindings[] = {
    {KEY_A, Button::A},           {KEY_B, Button::B},         {KEY_X, Button::X},
    {KEY_Y, Button::Y},           {KEY_L, Button::L},         {KEY_R, Button::R},
    {KEY_ZL, Button::ZL},         {KEY_ZR, Button::ZR},       {KEY_START, Button::Start},
    {KEY_SELECT, Button::Select}, {KEY_DUP, Button::Up},      {KEY_DDOWN, Button::Down},
    {KEY_DLEFT, Button::Left},    {KEY_DRIGHT, Button::Right},
};

// Raw value treated as full deflection (circlePosition units, y up); readings past it are clamped by core.
// Circle Pad measured on an Old 3DS XL (2026-11-18): 159-167 at full push depending on the direction, so
// 156 reaches 1.0 in every direction with some slack for worn pads. C-Stick: same value, not measured yet
// (needs a New 3DS).
constexpr float kCirclePadRange = 156.0f;
constexpr float kCStickRange = 156.0f;

} // namespace

void ctr::input_start() {
    bool is_new = false;
    APT_CheckNew3DS(&is_new);
    context().c_stick_ready = is_new && R_SUCCEEDED(irrstInit());
}

void ctr::input_stop() {
    if (context().c_stick_ready) irrstExit();
    context().c_stick_ready = false;
}

void read_input(InputSample& out) {
    out = InputSample{};
    hidScanInput();
    const u32 keys = hidKeysHeld();
    for (const KeyBinding& binding : kKeyBindings) {
        if (keys & binding.key) out.buttons |= static_cast<std::uint32_t>(binding.button);
    }

    circlePosition pad{};
    hidCircleRead(&pad);
    out.circle_x = pad.dx / kCirclePadRange;
    out.circle_y = pad.dy / kCirclePadRange;

    if (ctr::context().c_stick_ready) {
        irrstScanInput();
        circlePosition stick{};
        hidCstickRead(&stick);
        out.cstick_x = stick.dx / kCStickRange;
        out.cstick_y = stick.dy / kCStickRange;
    }

    if (keys & KEY_TOUCH) {
        touchPosition touch{};
        hidTouchRead(&touch);
        out.touching = true;
        out.touch_x = static_cast<std::int16_t>(touch.px);
        out.touch_y = static_cast<std::int16_t>(touch.py);
    }
}

} // namespace nnh::platform
