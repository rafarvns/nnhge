#pragma once
// Bounded, libm-free approximations used by the easing curves (spec 015 FR-3). Same bits on every system:
// only IEEE + − × and float↔int conversions, compiled with -ffp-contract=off (engine/core/CMakeLists.txt).

#include "core/det_sine.hpp"

namespace nnh::detail {

// sin_quarter (sin(π/2 · u) for u in [0, 1]) is core/det_sine.hpp's, shared with the transforms (spec 022).

/// sin(2π · x) for any |x| < 2²³ (in turns): x is reduced to its fraction, a quarter is picked, then
/// sin_quarter. Exact at the quarter points (0, ±1).
float sin_turns(float x);

/// 2^x for x in [−10, 0] (the elastic decay): the whole part builds the exponent bits, the fraction a
/// degree-5 polynomial (relative error < 10⁻⁶). Exact at whole x.
float exp2_neg(float x);

} // namespace nnh::detail
