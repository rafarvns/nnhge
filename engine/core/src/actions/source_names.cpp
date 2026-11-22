// Display names of controls (spec 017 FR-19): short English names in the built-in font's characters.

#include "nnh/actions.hpp"

namespace nnh::actions {
namespace {

constexpr const char* kButtons[14] = {"A",     "B",      "X",        "Y",          "L",          "R",
                                      "ZL",    "ZR",     "Start",    "Select",     "D-Pad Up",   "D-Pad Down",
                                      "D-Pad Left",      "D-Pad Right"};
constexpr const char* kDirections[2][4] = {
    {"Circle Pad Up", "Circle Pad Down", "Circle Pad Left", "Circle Pad Right"},
    {"C-Stick Up", "C-Stick Down", "C-Stick Left", "C-Stick Right"},
};
constexpr const char* kSticks[2] = {"Circle Pad", "C-Stick"};
constexpr const char* kAxes[2][2] = {{"Circle Pad X", "Circle Pad Y"}, {"C-Stick X", "C-Stick Y"}};

const char* button_name(std::uint8_t index) { return index < 14 ? kButtons[index] : "-"; }

/// Appends `text` to `out` (capacity `cap`, used `n`), truncating; keeps the NUL.
void append(char* out, std::size_t cap, std::size_t& n, const char* text) {
    for (; *text != '\0' && n + 1 < cap; ++text) out[n++] = *text;
    out[n] = '\0';
}

} // namespace

const char* control_name(Source s) {
    switch (s.kind) {
    case Source::Kind::None: return "";
    case Source::Kind::Button: return button_name(s.a);
    case Source::Kind::StickDirection: return kDirections[s.a & 1][s.b & 3];
    case Source::Kind::Stick: return kSticks[s.a & 1];
    case Source::Kind::StickAxis: return kAxes[s.a & 1][s.b & 1];
    case Source::Kind::Buttons2:
    case Source::Kind::Buttons4: return "";
    case Source::Kind::Touch: return "Touch";
    }
    return "";
}

std::size_t describe(Source s, char* out, std::size_t capacity) {
    if (out == nullptr || capacity == 0) return 0;
    std::size_t n = 0;
    out[0] = '\0';
    if (s.kind == Source::Kind::Buttons4 && s == Source::dpad()) {
        append(out, capacity, n, "D-Pad");
    } else if (s.kind == Source::Kind::Buttons2 || s.kind == Source::Kind::Buttons4) {
        const std::uint8_t parts[4] = {s.a, s.b, s.c, s.d};
        const int count = s.kind == Source::Kind::Buttons2 ? 2 : 4;
        for (int i = 0; i < count; ++i) {
            if (i > 0) append(out, capacity, n, "/");
            append(out, capacity, n, button_name(parts[i]));
        }
    } else {
        append(out, capacity, n, control_name(s));
    }
    return n;
}

} // namespace nnh::actions
