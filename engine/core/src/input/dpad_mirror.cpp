// Circle Pad → D-Pad mirror (dpad_mirror.hpp).

#include "input/dpad_mirror.hpp"

namespace nnh::detail {
namespace {

constexpr std::uint32_t bit(Button b) { return static_cast<std::uint32_t>(b); }

/// One direction: on at kMirrorOn, kept until the axis drops below kMirrorOff.
std::uint32_t direction(float amount, std::uint32_t mask, std::uint32_t previous) {
    const float threshold = (previous & mask) != 0 ? kMirrorOff : kMirrorOn;
    return amount >= threshold ? mask : 0;
}

} // namespace

std::uint32_t dpad_mirror(Stick pad, std::uint32_t previous) {
    return direction(pad.x, bit(Button::Right), previous) | direction(-pad.x, bit(Button::Left), previous) |
           direction(pad.y, bit(Button::Up), previous) | direction(-pad.y, bit(Button::Down), previous);
}

} // namespace nnh::detail
