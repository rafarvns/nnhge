// libm-free sine and 2^x approximations (plan 015 "Determinism"). Coefficients fitted once (Lawson's weighted
// least squares → minimax) with the end constraints p(1) = 1 for the sine and p(0) = 1, p(1) = 2 for 2^f.

#include "tween/ease_math.hpp"

#include <bit>
#include <cstdint>

namespace nnh::detail {

float sin_quarter(float u) {
    const float u2 = u * u;
    const float v = u * (1.5707903f + u2 * (-0.64588609f + u2 * (0.079418352f + u2 * -0.0043225876f)));
    return v < 1.0f ? v : 1.0f; // the fit touches 1 only at u = 1; rounding must not cross it
}

float sin_turns(float x) {
    float r = x - static_cast<float>(static_cast<std::int32_t>(x)); // fraction, sign of x
    if (r < 0.0f) r += 1.0f;
    const float r4 = r * 4.0f; // exact (power of two)
    const int quarter = static_cast<int>(r4);
    const float u = r4 - static_cast<float>(quarter); // exact
    switch (quarter & 3) {
    case 0:
        return sin_quarter(u);
    case 1:
        return sin_quarter(1.0f - u);
    case 2:
        return -sin_quarter(u);
    default:
        return -sin_quarter(1.0f - u);
    }
}

float exp2_neg(float x) {
    std::int32_t whole = static_cast<std::int32_t>(x); // truncates toward zero
    if (static_cast<float>(whole) > x) --whole;        // floor
    const float f = x - static_cast<float>(whole);     // [0, 1), exact
    const float p =
        1.0f +
        f * (0.69315174f + f * (0.24015927f + f * (0.055818676f + f * (0.0089909951f + f * 0.0018793186f))));
    const float scale = std::bit_cast<float>(static_cast<std::uint32_t>(whole + 127) << 23); // 2^whole
    return p * scale;
}

} // namespace nnh::detail
