// Matrix inverses (spec 026 FR-1/FR-3): the general one by cofactors (2×2 sub-determinants in a fixed order) and the
// affine one (3×3 cofactors + translation). A |determinant| below 2⁻¹⁰⁰ or not finite refuses (an exponent test).
// -ffp-contract=off.

#include "math3d/math3d_internal.hpp"

namespace nnh {

bool inverse(const Mat4& m, Mat4& out) {
    if (!is_finite(m)) return false;
    // a[row][col]; the sub-determinants of rows 0-1 (s) and rows 2-3 (c), Laplace expansion by complementary minors.
    auto a = [&m](int r, int c) { return m.m[c * 4 + r]; };
    const float s0 = a(0, 0) * a(1, 1) - a(1, 0) * a(0, 1);
    const float s1 = a(0, 0) * a(1, 2) - a(1, 0) * a(0, 2);
    const float s2 = a(0, 0) * a(1, 3) - a(1, 0) * a(0, 3);
    const float s3 = a(0, 1) * a(1, 2) - a(1, 1) * a(0, 2);
    const float s4 = a(0, 1) * a(1, 3) - a(1, 1) * a(0, 3);
    const float s5 = a(0, 2) * a(1, 3) - a(1, 2) * a(0, 3);
    const float c5 = a(2, 2) * a(3, 3) - a(3, 2) * a(2, 3);
    const float c4 = a(2, 1) * a(3, 3) - a(3, 1) * a(2, 3);
    const float c3 = a(2, 1) * a(3, 2) - a(3, 1) * a(2, 2);
    const float c2 = a(2, 0) * a(3, 3) - a(3, 0) * a(2, 3);
    const float c1 = a(2, 0) * a(3, 2) - a(3, 0) * a(2, 2);
    const float c0 = a(2, 0) * a(3, 1) - a(3, 0) * a(2, 1);
    const float det = ((((s0 * c5 - s1 * c4) + s2 * c3) + s3 * c2) - s4 * c1) + s5 * c0;
    if (!detail::usable(det)) return false;
    float b[4][4]; // b[row][col] before the division
    b[0][0] = (a(1, 1) * c5 - a(1, 2) * c4) + a(1, 3) * c3;
    b[0][1] = (-a(0, 1) * c5 + a(0, 2) * c4) - a(0, 3) * c3;
    b[0][2] = (a(3, 1) * s5 - a(3, 2) * s4) + a(3, 3) * s3;
    b[0][3] = (-a(2, 1) * s5 + a(2, 2) * s4) - a(2, 3) * s3;
    b[1][0] = (-a(1, 0) * c5 + a(1, 2) * c2) - a(1, 3) * c1;
    b[1][1] = (a(0, 0) * c5 - a(0, 2) * c2) + a(0, 3) * c1;
    b[1][2] = (-a(3, 0) * s5 + a(3, 2) * s2) - a(3, 3) * s1;
    b[1][3] = (a(2, 0) * s5 - a(2, 2) * s2) + a(2, 3) * s1;
    b[2][0] = (a(1, 0) * c4 - a(1, 1) * c2) + a(1, 3) * c0;
    b[2][1] = (-a(0, 0) * c4 + a(0, 1) * c2) - a(0, 3) * c0;
    b[2][2] = (a(3, 0) * s4 - a(3, 1) * s2) + a(3, 3) * s0;
    b[2][3] = (-a(2, 0) * s4 + a(2, 1) * s2) - a(2, 3) * s0;
    b[3][0] = (-a(1, 0) * c3 + a(1, 1) * c1) - a(1, 2) * c0;
    b[3][1] = (a(0, 0) * c3 - a(0, 1) * c1) + a(0, 2) * c0;
    b[3][2] = (-a(3, 0) * s3 + a(3, 1) * s1) - a(3, 2) * s0;
    b[3][3] = (a(2, 0) * s3 - a(2, 1) * s1) + a(2, 2) * s0;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) out.m[c * 4 + r] = b[r][c] / det;
    return true;
}

bool inverse_affine(const Mat4& m, Mat4& out) {
    if (!is_finite(m)) return false;
    auto a = [&m](int r, int c) { return m.m[c * 4 + r]; };
    const float k00 = a(1, 1) * a(2, 2) - a(2, 1) * a(1, 2); // cofactors of the 3×3 part
    const float k01 = a(2, 1) * a(0, 2) - a(0, 1) * a(2, 2);
    const float k02 = a(0, 1) * a(1, 2) - a(1, 1) * a(0, 2);
    const float det = (a(0, 0) * k00 + a(1, 0) * k01) + a(2, 0) * k02;
    if (!detail::usable(det)) return false;
    float r[9]; // r[row * 3 + col] of the inverse 3×3
    r[0] = k00 / det;
    r[1] = k01 / det;
    r[2] = k02 / det;
    r[3] = (a(2, 0) * a(1, 2) - a(1, 0) * a(2, 2)) / det;
    r[4] = (a(0, 0) * a(2, 2) - a(2, 0) * a(0, 2)) / det;
    r[5] = (a(1, 0) * a(0, 2) - a(0, 0) * a(1, 2)) / det;
    r[6] = (a(1, 0) * a(2, 1) - a(2, 0) * a(1, 1)) / det;
    r[7] = (a(2, 0) * a(0, 1) - a(0, 0) * a(2, 1)) / det;
    r[8] = (a(0, 0) * a(1, 1) - a(1, 0) * a(0, 1)) / det;
    const float tx = m.m[12], ty = m.m[13], tz = m.m[14];
    Mat4 inv;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) inv.m[col * 4 + row] = r[row * 3 + col];
        const float s = r[row * 3] * tx + r[row * 3 + 1] * ty;
        inv.m[12 + row] = -(s + r[row * 3 + 2] * tz);
    }
    out = inv;
    return true;
}

} // namespace nnh
