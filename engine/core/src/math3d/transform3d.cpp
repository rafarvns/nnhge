// 3D transforms (spec 026 FR-1/FR-3): T × R × S as one matrix — the rotation's columns scaled, the position as the
// last column, so a translation-only transform adds once per axis. -ffp-contract=off.

#include "math3d/math3d_internal.hpp"

namespace nnh {

Mat4 to_mat4(const Transform3D& t) {
    Mat4 m = to_mat4(t.rotation);
    const float s[3] = {t.scale.x, t.scale.y, t.scale.z};
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row) m.m[col * 4 + row] = m.m[col * 4 + row] * s[col];
    m.m[12] = t.position.x;
    m.m[13] = t.position.y;
    m.m[14] = t.position.z;
    return m;
}

} // namespace nnh
