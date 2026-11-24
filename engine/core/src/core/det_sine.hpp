#pragma once
// The engine's libm-free sine polynomial (spec 015 FR-3, moved here by spec 022 T1.1 so the easings and the transforms
// share it without depending on each other). Same bits on every system: only IEEE + − × in a fixed order, compiled
// with -ffp-contract=off by every core source that includes this header (engine/core/CMakeLists.txt).

#if defined(__GNUC__) || defined(__clang__)
#define NNH_DET_INLINE inline __attribute__((always_inline))
#else
#define NNH_DET_INLINE inline
#endif

namespace nnh::detail {

/// sin(π/2 · u) for u in [0, 1]: an odd degree-7 polynomial fitted to the minimax error (< 2·10⁻⁶ against the exact
/// sine, measured < 7·10⁻⁷ plus float rounding). Exactly 0 at 0 and 1 at 1, never above 1. Inline for the hot paths
/// (the easings' sin_turns, the transforms' sin_cos); sin_quarter() below is the same code out of line.
NNH_DET_INLINE float sin_quarter_inline(float u) {
    const float u2 = u * u;
    const float v = u * (1.5707903f + u2 * (-0.64588609f + u2 * (0.079418352f + u2 * -0.0043225876f)));
    return v < 1.0f ? v : 1.0f; // the fit touches 1 only at u = 1; rounding must not cross it
}

/// The same, out of line (tests, callers that don't need it inline).
float sin_quarter(float u);

} // namespace nnh::detail
