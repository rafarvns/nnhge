// The PICA projection (video3d_3ds.hpp, plan T7.4): core gives an OpenGL clip-space projection (depth −1..1, y up on a
// 400×240 screen); the PICA200 wants citro3d's depth range and the top screen's 90° rotation. Both differences are a
// constant matrix F, taken once from citro3d itself: F = Mtx_PerspTilt(ref) × inverse(GL(ref)). The Development
// self-test checks F × GL(p) against Mtx_PerspTilt(p) and Mtx_OrthoTilt(p) for other parameters.

#include <cmath>
#include <cstdio>

#include "video3d_3ds.hpp"

namespace nnh::platform::ctr {
namespace {

C3D_Mtx g_fix;
bool g_fix_ready = false;

C3D_Mtx from_core(const float p[16]) { // column-major → citro3d rows
    C3D_Mtx m;
    for (int r = 0; r < 4; ++r) m.r[r] = FVec4_New(p[r], p[4 + r], p[8 + r], p[12 + r]);
    return m;
}

void gl_perspective(float fov, float aspect, float n, float f, float out[16]) {
    for (int i = 0; i < 16; ++i) out[i] = 0;
    const float t = 1.0f / std::tan(fov * 0.5f);
    out[0] = t / aspect, out[5] = t, out[10] = (f + n) / (n - f), out[11] = -1, out[14] = 2 * f * n / (n - f);
}

void gl_orthographic(float height, float aspect, float n, float f, float out[16]) {
    for (int i = 0; i < 16; ++i) out[i] = 0;
    out[0] = 2 / (height * aspect), out[5] = 2 / height, out[10] = -2 / (f - n), out[14] = -(f + n) / (f - n);
    out[15] = 1;
}

void make_fix() {
    float gl[16];
    gl_perspective(1.04719758f, C3D_AspectRatioTop, 0.1f, 100.0f, gl);
    C3D_Mtx inv = from_core(gl), tilt;
    Mtx_Inverse(&inv);
    Mtx_PerspTilt(&tilt, 1.04719758f, C3D_AspectRatioTop, 0.1f, 100.0f, false);
    Mtx_Multiply(&g_fix, &tilt, &inv);
    g_fix_ready = true;
}

float max_difference(const C3D_Mtx& a, const C3D_Mtx& b) {
    float worst = 0;
    for (int i = 0; i < 16; ++i) {
        const float d = std::fabs(a.m[i] - b.m[i]) / (std::fabs(b.m[i]) > 1 ? std::fabs(b.m[i]) : 1.0f);
        worst = d > worst ? d : worst;
    }
    return worst;
}

} // namespace

void pica_projection(const float core_projection[16], C3D_Mtx& out) {
    if (!g_fix_ready) make_fix();
    const C3D_Mtx p = from_core(core_projection);
    Mtx_Multiply(&out, &g_fix, &p);
}

const char* tilt_self_test() {
    static char line[96];
    float gl[16];
    C3D_Mtx mine, theirs;
    gl_perspective(0.785398f, C3D_AspectRatioTop, 0.5f, 50.0f, gl);
    pica_projection(gl, mine);
    Mtx_PerspTilt(&theirs, 0.785398f, C3D_AspectRatioTop, 0.5f, 50.0f, false);
    const float persp = max_difference(mine, theirs);
    gl_orthographic(6.0f, C3D_AspectRatioTop, 1.0f, 30.0f, gl);
    pica_projection(gl, mine);
    Mtx_OrthoTilt(&theirs, -5.0f, 5.0f, -3.0f, 3.0f, 1.0f, 30.0f, false);
    const float ortho = max_difference(mine, theirs);
    const bool pass = persp < 1e-5f && ortho < 1e-5f;
    std::snprintf(line, sizeof line, "3d tilt: %s (perspective %.2g, orthographic %.2g)", pass ? "PASS" : "FAIL",
                  static_cast<double>(persp), static_cast<double>(ortho));
    return line;
}

} // namespace nnh::platform::ctr
