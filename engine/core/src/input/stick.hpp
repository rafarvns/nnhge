#pragma once
// Analog stick shaping: dead zone, rescale and clamp. Pure logic, unit tested in tests/input/test_stick.cpp.

#include "nnh/input.hpp"

namespace nnh::detail {

/// Radius of the dead zone around the center, as a fraction of the full range (fixed, spec 003).
inline constexpr float kStickDeadZone = 0.10f;

/// Turns a platform reading (about [-1, 1] per axis, y up, no dead zone) into the public Stick value:
/// inside the dead zone → (0, 0); outside, the length is rescaled so it starts at 0 at the dead zone's
/// edge and reaches 1 at full range, keeping the direction; the result is clamped to the unit circle.
/// Non-finite input reads (0, 0).
Stick shape_stick(float x, float y);

} // namespace nnh::detail
