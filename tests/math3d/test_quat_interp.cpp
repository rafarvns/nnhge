// Interpolation (spec 026 FR-1, T1.4): nlerp and Eberly's slerp — exact ends, midpoints, the shortest arc across
// hemispheres, accuracy against a double reference (AC-3).

#include <cmath>

#include "math3d_fixture.hpp"

using math3d_test::near;
using math3d_test::same_rotation;
using nnh::Quat;

namespace {

/// The reference slerp in double (tests may use libm).
Quat slerp_ref(Quat a, Quat b, double t) {
    double d = double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z + double(a.w) * b.w;
    double s = 1;
    if (d < 0) {
        d = -d;
        s = -1;
    }
    const double th = std::acos(d > 1 ? 1 : d);
    double ca = 1 - t, cb = t;
    if (th > 1e-9) {
        ca = std::sin((1 - t) * th) / std::sin(th);
        cb = std::sin(t * th) / std::sin(th);
    }
    cb *= s;
    return {float(ca * a.x + cb * b.x), float(ca * a.y + cb * b.y), float(ca * a.z + cb * b.z),
            float(ca * a.w + cb * b.w)};
}

} // namespace

NNH_TEST(slerp_ends_are_exact) {
    const Quat a = nnh::quat_axis_angle({0, 1, 0}, 0.3f);
    const Quat b = nnh::quat_axis_angle({1, 0, 1}, 2.1f);
    CHECK(nnh::slerp(a, b, 0) == a);
    CHECK(nnh::slerp(a, b, 1) == b);
    const Quat nb{-b.x, -b.y, -b.z, -b.w}; // other hemisphere: t = 1 gives the same rotation, −(−b)
    CHECK(nnh::slerp(a, nb, 1) == b);
}

NNH_TEST(slerp_midpoint_of_a_quarter_turn_is_an_eighth) {
    const Quat m = nnh::slerp(Quat{}, nnh::quat_axis_angle({0, 0, 1}, nnh::kQuarterTurn), 0.5f);
    CHECK(same_rotation(m, nnh::quat_axis_angle({0, 0, 1}, nnh::kQuarterTurn / 2), 2e-6f));
}

NNH_TEST(slerp_takes_the_shortest_arc) {
    const Quat a = nnh::quat_axis_angle({0, 1, 0}, 0.1f);
    const Quat b = nnh::quat_axis_angle({0, 1, 0}, 0.5f);
    const Quat nb{-b.x, -b.y, -b.z, -b.w};
    CHECK(same_rotation(nnh::slerp(a, nb, 0.5f), nnh::quat_axis_angle({0, 1, 0}, 0.3f), 2e-6f));
    CHECK(same_rotation(nnh::nlerp(a, nb, 0.5f), nnh::quat_axis_angle({0, 1, 0}, 0.3f), 2e-6f));
}

NNH_TEST(slerp_accuracy_against_double_is_within_two_to_the_minus_18) {
    math3d_test::Rng rng;
    float worst = 0;
    for (int i = 0; i < 4000; ++i) {
        const Quat a = nnh::normalize(Quat{rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1)});
        const Quat b = nnh::normalize(Quat{rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1)});
        const float t = rng.range(0, 1);
        const Quat s = nnh::slerp(a, b, t), r = slerp_ref(a, b, t);
        const float e[4] = {std::fabs(s.x - r.x), std::fabs(s.y - r.y), std::fabs(s.z - r.z), std::fabs(s.w - r.w)};
        for (const float v : e) worst = v > worst ? v : worst;
    }
    std::printf("    slerp worst error %.3g (bound %.3g)\n", double(worst), std::ldexp(1.0, -18));
    CHECK(worst <= std::ldexp(1.0f, -18));
}

NNH_TEST(nlerp_is_normalised_and_exact_at_zero) {
    const Quat a = nnh::quat_axis_angle({0, 1, 0}, 0.3f);
    const Quat b = nnh::quat_axis_angle({1, 0, 0}, 1.1f);
    const Quat n = nnh::nlerp(a, b, 0.37f);
    CHECK(near(n.x * n.x + n.y * n.y + n.z * n.z + n.w * n.w, 1, 1e-6f));
    CHECK(same_rotation(nnh::nlerp(a, b, 0), nnh::normalize(a), 0));
}
