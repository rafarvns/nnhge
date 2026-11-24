// Quaternion interpolation (spec 026 FR-1, plan P4): nlerp, and slerp by Eberly's polynomial ("A Fast and Accurate
// Algorithm for Computing SLERP", JGT 2011): no acos/sin, fixed coefficients, the shortest arc by the sign of the dot
// product. Stage 1 refinement: 16 terms with the last one scaled by 1.9092 (measured: max error 1.2e-7 against double
// over the whole [0, 90°] range; the paper's 8 terms measured 1.9e-5, above FR-3's 2⁻¹⁸). -ffp-contract=off.

#include "math3d/math3d_internal.hpp"

namespace nnh {
namespace {

constexpr int kTerms = 16;
constexpr float kOnePlusMu = 1.9092f;

/// uᵢ = 1/(i(2i+1)), vᵢ = i/(2i+1) for i = 1..16, the last pair × (1 + μ).
struct Coefficients {
    float u[kTerms];
    float v[kTerms];
    constexpr Coefficients() : u{}, v{} {
        for (int i = 1; i <= kTerms; ++i) {
            const double scale = i == kTerms ? double(kOnePlusMu) : 1.0;
            u[i - 1] = static_cast<float>(scale / (double(i) * (2 * i + 1)));
            v[i - 1] = static_cast<float>(scale * i / (2 * i + 1));
        }
    }
};
constexpr Coefficients kC{};
constexpr const float* kU = kC.u;
constexpr const float* kV = kC.v;

float qdot(Quat a, Quat b) {
    const float xy = a.x * b.x + a.y * b.y;
    const float xyz = xy + a.z * b.z;
    return xyz + a.w * b.w;
}

/// t · (1 + b0(1 + b1(1 + … (1 + b15)))) with bᵢ = (uᵢ t² − vᵢ)(x − 1).
float coefficient(float t, float xm1) {
    const float t2 = t * t;
    float acc = 1.0f;
    for (int i = kTerms - 1; i >= 0; --i) acc = 1.0f + ((kU[i] * t2 - kV[i]) * xm1) * acc;
    return t * acc;
}

Quat combine(Quat a, float ca, Quat b, float cb) {
    return {a.x * ca + b.x * cb, a.y * ca + b.y * cb, a.z * ca + b.z * cb, a.w * ca + b.w * cb};
}

} // namespace

Quat nlerp(Quat a, Quat b, float t) {
    const float sign = detail::sign_bit(qdot(a, b)) ? -1.0f : 1.0f;
    return normalize(combine(a, 1.0f - t, b, sign * t));
}

Quat slerp(Quat a, Quat b, float t) {
    float x = qdot(a, b);
    float sign = 1.0f;
    if (detail::sign_bit(x)) {
        x = -x;
        sign = -1.0f;
    }
    if (x > 1.0f) x = 1.0f; // rounding of unit inputs
    const float xm1 = x - 1.0f;
    const float cb = sign * coefficient(t, xm1);
    const float ca = coefficient(1.0f - t, xm1);
    return combine(a, ca, b, cb);
}

} // namespace nnh
