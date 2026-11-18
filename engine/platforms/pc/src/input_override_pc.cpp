// PC simulator: NNH_SIM_INPUT scripts the input for headless screenshots (Debug/Development only).
//
//   NNH_SIM_INPUT="circle=0.7,0.5;cstick=-1,0;touch=160,120;buttons=A+R"
//
// Every part is optional; the sample is reported on every frame instead of keyboard, gamepad and mouse.
// Sticks use the platform scale (about [-1, 1], y up, before the engine's dead zone).

#include <cstdlib>
#include <cstring>

#include "context_pc.hpp"
#include "nnh/build.hpp"
#include "nnh/input.hpp"

namespace nnh::platform {
namespace {

bool g_active = false;
InputSample g_sample;

struct ButtonName {
    const char* name;
    Button button;
};

constexpr ButtonName kButtonNames[] = {
    {"A", Button::A},         {"B", Button::B},         {"X", Button::X},          {"Y", Button::Y},
    {"L", Button::L},         {"R", Button::R},         {"ZL", Button::ZL},        {"ZR", Button::ZR},
    {"Start", Button::Start}, {"Select", Button::Select}, {"Up", Button::Up},      {"Down", Button::Down},
    {"Left", Button::Left},   {"Right", Button::Right},
};

/// "x,y" → two floats; returns the text after them.
const char* parse_pair(const char* text, float& x, float& y) {
    char* end = nullptr;
    x = std::strtof(text, &end);
    if (*end == ',') ++end;
    y = std::strtof(end, &end);
    return end;
}

/// "A+R+Start" up to ';' or the end.
const char* parse_buttons(const char* text, std::uint32_t& buttons) {
    while (*text != '\0' && *text != ';') {
        const std::size_t length = std::strcspn(text, "+;");
        for (const ButtonName& b : kButtonNames) {
            if (std::strlen(b.name) == length && std::strncmp(b.name, text, length) == 0) {
                buttons |= static_cast<std::uint32_t>(b.button);
            }
        }
        text += length;
        if (*text == '+') ++text;
    }
    return text;
}

bool starts_with(const char* text, const char* prefix) { return std::strncmp(text, prefix, std::strlen(prefix)) == 0; }

void parse(const char* text) {
    while (*text != '\0') {
        float a = 0;
        float b = 0;
        if (starts_with(text, "circle=")) {
            text = parse_pair(text + 7, g_sample.circle_x, g_sample.circle_y);
        } else if (starts_with(text, "cstick=")) {
            text = parse_pair(text + 7, g_sample.cstick_x, g_sample.cstick_y);
        } else if (starts_with(text, "touch=")) {
            text = parse_pair(text + 6, a, b);
            g_sample.touching = true;
            g_sample.touch_x = static_cast<std::int16_t>(a);
            g_sample.touch_y = static_cast<std::int16_t>(b);
        } else if (starts_with(text, "buttons=")) {
            text = parse_buttons(text + 8, g_sample.buttons);
        }
        text = std::strchr(text, ';'); // skip the rest of this part (also unknown parts)
        if (text == nullptr) break;
        ++text;
    }
}

} // namespace

void pc::input_override_init() {
    g_active = false;
    g_sample = InputSample{};
    if constexpr (!build::kDevToolsEnabled) return;
    const char* text = std::getenv("NNH_SIM_INPUT");
    if (text == nullptr || *text == '\0') return;
    parse(text);
    g_active = true;
}

bool pc::input_override(InputSample& out) {
    if (!g_active) return false;
    out = g_sample;
    return true;
}

} // namespace nnh::platform
