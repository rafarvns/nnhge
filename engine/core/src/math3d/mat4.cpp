// 4×4 matrices (spec 026 FR-1): product, transpose, points, directions, Mat4 × Vec4, finiteness. Every sum in the
// fixed order ((a0 + a1) + a2) + a3; -ffp-contract=off.

#include "math3d/math3d_internal.hpp"

namespace nnh {
namespace {

/// Row `row` of `a` times column `col` of `b`.
float row_col(const Mat4& a, const Mat4& b, int row, int col) {
    const float* bc = b.m + col * 4;
    const float s01 = a.m[row] * bc[0] + a.m[4 + row] * bc[1];
    const float s012 = s01 + a.m[8 + row] * bc[2];
    return s012 + a.m[12 + row] * bc[3];
}

/// Row `row` of `m` times (x, y, z, w).
float row_dot(const Mat4& m, int row, float x, float y, float z, float w) {
    const float s01 = m.m[row] * x + m.m[4 + row] * y;
    const float s012 = s01 + m.m[8 + row] * z;
    return s012 + m.m[12 + row] * w;
}

/// Row `row` of the 3×3 part times (x, y, z).
float row_dot3(const Mat4& m, int row, Vec3 v) {
    const float s01 = m.m[row] * v.x + m.m[4 + row] * v.y;
    return s01 + m.m[8 + row] * v.z;
}

} // namespace

Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row) r.m[col * 4 + row] = row_col(a, b, row, col);
    return r;
}

Mat4 transpose(const Mat4& m) {
    Mat4 t;
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row) t.m[col * 4 + row] = m.m[row * 4 + col];
    return t;
}

Vec4 operator*(const Mat4& m, Vec4 v) {
    return {row_dot(m, 0, v.x, v.y, v.z, v.w), row_dot(m, 1, v.x, v.y, v.z, v.w), row_dot(m, 2, v.x, v.y, v.z, v.w),
            row_dot(m, 3, v.x, v.y, v.z, v.w)};
}

Vec3 transform_point(const Mat4& m, Vec3 p) {
    const Vec3 r{row_dot3(m, 0, p) + m.m[12], row_dot3(m, 1, p) + m.m[13], row_dot3(m, 2, p) + m.m[14]};
    const float w = row_dot3(m, 3, p) + m.m[15];
    if (w == 1.0f) return r;
    if (!detail::usable(w)) return {};
    return {r.x / w, r.y / w, r.z / w};
}

Vec3 transform_direction(const Mat4& m, Vec3 d) { return {row_dot3(m, 0, d), row_dot3(m, 1, d), row_dot3(m, 2, d)}; }

bool is_finite(const Mat4& m) {
    for (const float f : m.m)
        if (!detail::finite(f)) return false;
    return true;
}

} // namespace nnh
