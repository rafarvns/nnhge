// Accuracy against a double reference (spec 026 FR-3, AC-3, T1.6): unit quaternions and directions within 2⁻¹⁸ per
// component, points within 2⁻¹⁶ × magnitude for coordinates up to 4096 through chains of 8 transforms. The measured
// maxima are printed (plan 026 "Stage 1 results").

#include <cmath>

#include "math3d_fixture.hpp"

using nnh::Mat4;
using nnh::Quat;
using nnh::Vec3;

namespace {

struct DMat {
    double m[16];
};

DMat to_double(const Mat4& a) {
    DMat d{};
    for (int i = 0; i < 16; ++i) d.m[i] = a.m[i];
    return d;
}

DMat mul(const DMat& a, const DMat& b) {
    DMat r{};
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) {
            double s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}

/// The exact rotation of axis-angle in double (unit axis), as T(pos) × R × S.
DMat transform_ref(Vec3 axis, double angle, Vec3 pos, Vec3 scale) {
    const double l = std::sqrt(double(axis.x) * axis.x + double(axis.y) * axis.y + double(axis.z) * axis.z);
    const double x = axis.x / l, y = axis.y / l, z = axis.z / l, c = std::cos(angle), s = std::sin(angle), t = 1 - c;
    const double r[9] = {t * x * x + c,     t * x * y + s * z, t * x * z - s * y, t * x * y - s * z, t * y * y + c,
                         t * y * z + s * x, t * x * z + s * y, t * y * z - s * x, t * z * z + c};
    DMat m{};
    const double sc[3] = {scale.x, scale.y, scale.z};
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row) m.m[col * 4 + row] = r[col * 3 + row] * sc[col];
    m.m[12] = pos.x;
    m.m[13] = pos.y;
    m.m[14] = pos.z;
    m.m[15] = 1;
    return m;
}

} // namespace

NNH_TEST(accuracy_unit_quaternions_and_directions) {
    math3d_test::Rng rng;
    double worst_q = 0, worst_d = 0;
    for (int i = 0; i < 5000; ++i) {
        const Vec3 axis = nnh::normalize(Vec3{rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1) + 0.01f});
        const float angle = rng.range(-6.3f, 6.3f);
        const Quat q = nnh::quat_axis_angle(axis, angle);
        const double hs = std::sin(angle * 0.5), hc = std::cos(angle * 0.5);
        const double rq[4] = {axis.x * hs, axis.y * hs, axis.z * hs, hc};
        const float fq[4] = {q.x, q.y, q.z, q.w};
        for (int k = 0; k < 4; ++k) worst_q = std::fmax(worst_q, std::fabs(fq[k] - rq[k]));
        const Vec3 v = nnh::normalize(Vec3{rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1) + 0.01f});
        const Vec3 r = nnh::rotate(q, v);
        const DMat m = transform_ref(axis, angle, {}, {1, 1, 1});
        const double ref[3] = {m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z, m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z,
                               m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z};
        worst_d = std::fmax(worst_d, std::fmax(std::fabs(r.x - ref[0]), std::fmax(std::fabs(r.y - ref[1]),
                                                                                    std::fabs(r.z - ref[2]))));
    }
    std::printf("    quaternion worst %.3g, direction worst %.3g (bound %.3g)\n", worst_q, worst_d,
                std::ldexp(1.0, -18));
    CHECK(worst_q <= std::ldexp(1.0, -18));
    CHECK(worst_d <= std::ldexp(1.0, -18));
}

NNH_TEST(accuracy_points_through_chains_of_eight) {
    math3d_test::Rng rng;
    double worst = 0;
    for (int i = 0; i < 1000; ++i) {
        Mat4 chain;
        DMat ref = to_double(Mat4{});
        for (int k = 0; k < 8; ++k) {
            const Vec3 axis = nnh::normalize(Vec3{rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1) + 0.01f});
            const float angle = rng.range(-3.2f, 3.2f);
            const Vec3 pos{rng.range(-512, 512), rng.range(-512, 512), rng.range(-512, 512)};
            const nnh::Transform3D t{.position = pos, .rotation = nnh::quat_axis_angle(axis, angle)};
            chain = chain * nnh::to_mat4(t);
            ref = mul(ref, transform_ref(axis, angle, pos, {1, 1, 1}));
        }
        const Vec3 p{rng.range(-4096, 4096), rng.range(-4096, 4096), rng.range(-4096, 4096)};
        const Vec3 r = nnh::transform_point(chain, p);
        const double rx = ref.m[0] * p.x + ref.m[4] * p.y + ref.m[8] * p.z + ref.m[12];
        const double ry = ref.m[1] * p.x + ref.m[5] * p.y + ref.m[9] * p.z + ref.m[13];
        const double rz = ref.m[2] * p.x + ref.m[6] * p.y + ref.m[10] * p.z + ref.m[14];
        const double mag = std::fmax(std::sqrt(rx * rx + ry * ry + rz * rz), std::fabs(p.x) + 1);
        worst = std::fmax(worst, std::fmax(std::fabs(r.x - rx), std::fmax(std::fabs(r.y - ry), std::fabs(r.z - rz))) /
                                     std::fmax(mag, 4096.0));
    }
    std::printf("    chain-of-8 point worst %.3g of magnitude (bound %.3g)\n", worst, std::ldexp(1.0, -16));
    CHECK(worst <= std::ldexp(1.0, -16));
}
