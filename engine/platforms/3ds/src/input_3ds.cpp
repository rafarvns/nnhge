// Nintendo 3DS: hardware buttons (libctru HID) → nnh::Button mask.

#include <3ds.h>

#include "nnh/input.hpp"
#include "nnh/platform/input.hpp"

namespace nnh::platform {
namespace {

struct KeyBinding {
    u32 key;
    Button button;
};

constexpr KeyBinding kKeyBindings[] = {
    {KEY_A, Button::A},           {KEY_B, Button::B},         {KEY_X, Button::X},
    {KEY_Y, Button::Y},           {KEY_L, Button::L},         {KEY_R, Button::R},
    {KEY_ZL, Button::ZL},         {KEY_ZR, Button::ZR},       {KEY_START, Button::Start},
    {KEY_SELECT, Button::Select}, {KEY_DUP, Button::Up},      {KEY_DDOWN, Button::Down},
    {KEY_DLEFT, Button::Left},    {KEY_DRIGHT, Button::Right},
};

} // namespace

std::uint32_t read_buttons() {
    hidScanInput();
    const u32 keys = hidKeysHeld();
    std::uint32_t mask = 0;
    for (const KeyBinding& binding : kKeyBindings) {
        if (keys & binding.key) mask |= static_cast<std::uint32_t>(binding.button);
    }
    return mask;
}

} // namespace nnh::platform
