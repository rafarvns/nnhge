#pragma once
// Circle Pad → D-Pad mirror with hysteresis. Pure logic, unit tested in tests/input/test_dpad_mirror.cpp.

#include <cstdint>

#include "nnh/input.hpp"

namespace nnh::detail {

/// A direction is pressed when its axis reaches kMirrorOn and released when it drops below kMirrorOff.
inline constexpr float kMirrorOn = 0.5f;
inline constexpr float kMirrorOff = 0.4f;

/// Up/Down/Left/Right bits (nnh::Button) for a shaped Circle Pad position. `previous` is the result of
/// the previous frame (other bits are ignored), used for the hysteresis.
std::uint32_t dpad_mirror(Stick circle_pad, std::uint32_t previous);

} // namespace nnh::detail
