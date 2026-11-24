#pragma once
// Shared helpers for the 3D math tests (spec 026 stage 1): bit views, tolerant compares and the misuse helper.

#include <bit>
#include <cmath>
#include <cstdint>

#include "crash/crash_internal.hpp"
#include "nnh/build.hpp"
#include "nnh/math3d.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace math3d_test {

inline std::uint32_t bits(float f) { return std::bit_cast<std::uint32_t>(f); }

inline bool near(float a, float b, float eps) { return std::fabs(a - b) <= eps; }
inline bool near(nnh::Vec3 a, nnh::Vec3 b, float eps) {
    return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps);
}
inline bool near(const nnh::Mat4& a, const nnh::Mat4& b, float eps) {
    for (int i = 0; i < 16; ++i)
        if (!near(a.m[i], b.m[i], eps)) return false;
    return true;
}
/// Same rotation: q and −q are the same.
inline bool same_rotation(nnh::Quat a, nnh::Quat b, float eps) {
    const bool plus = near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps) && near(a.w, b.w, eps);
    const bool minus = near(a.x, -b.x, eps) && near(a.y, -b.y, eps) && near(a.z, -b.z, eps) && near(a.w, -b.w, eps);
    return plus || minus;
}

/// Misuse asserts in Debug/Development and is harmless in Release.
inline bool asserts_or_is_ignored(void (*fn)()) {
    nnh::detail::crash_install();
    return nnh::platform_fake::run_expecting_crash(fn) == nnh::build::kAssertsEnabled;
}

/// A small seeded generator for tests (xorshift32; no rand()).
struct Rng {
    std::uint32_t s = 0x9E3779B9u;
    std::uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    /// Uniform in [lo, hi).
    float range(float lo, float hi) { return lo + (hi - lo) * static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
};

} // namespace math3d_test
