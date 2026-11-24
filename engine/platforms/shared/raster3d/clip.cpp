// Homogeneous clipping (raster3d.hpp, FR-20): Sutherland–Hodgman against the near (z ≥ −w) and far (z ≤ w) planes
// and a guard band (|x|, |y| ≤ 8w), so triangles crossing the camera plane are clipped, not dropped, and the
// fixed-point setup never overflows. Attributes are interpolated linearly in clip space.

#include "raster3d.hpp"

namespace nnh::raster3d {
namespace {

constexpr float kGuard = 8.0f;

/// Signed distance to plane `p` (inside when ≥ 0).
float distance(const ClipVertex& v, int p) {
    const float* c = v.clip;
    switch (p) {
    case 0: return c[2] + c[3];          // near
    case 1: return c[3] - c[2];          // far
    case 2: return kGuard * c[3] - c[0]; // right
    case 3: return kGuard * c[3] + c[0]; // left
    case 4: return kGuard * c[3] - c[1]; // top
    default: return kGuard * c[3] + c[1]; // bottom
    }
}

ClipVertex lerp(const ClipVertex& a, const ClipVertex& b, float t) {
    ClipVertex r;
    for (int i = 0; i < 4; ++i) r.clip[i] = a.clip[i] + (b.clip[i] - a.clip[i]) * t;
    for (int i = 0; i < kAttributes; ++i) r.attr[i] = a.attr[i] + (b.attr[i] - a.attr[i]) * t;
    return r;
}

int clip_plane(const ClipVertex* in, int n, ClipVertex* out, int p) {
    int m = 0;
    for (int i = 0; i < n; ++i) {
        const ClipVertex& a = in[i];
        const ClipVertex& b = in[(i + 1) % n];
        const float da = distance(a, p), db = distance(b, p);
        if (da >= 0) out[m++] = a;
        if ((da >= 0) != (db >= 0)) out[m++] = lerp(a, b, da / (da - db));
    }
    return m;
}

} // namespace

int clip_triangle(const ClipVertex in[3], ClipVertex out[9]) {
    bool inside = true;
    for (int p = 0; p < 6 && inside; ++p)
        for (int i = 0; i < 3; ++i) inside = inside && distance(in[i], p) >= 0;
    if (inside) {
        out[0] = in[0], out[1] = in[1], out[2] = in[2];
        return 3;
    }
    ClipVertex a[9], b[9];
    a[0] = in[0], a[1] = in[1], a[2] = in[2];
    int n = 3;
    ClipVertex* src = a;
    ClipVertex* dst = b;
    for (int p = 0; p < 6 && n >= 3; ++p) {
        n = clip_plane(src, n, dst, p);
        ClipVertex* t = src;
        src = dst;
        dst = t;
    }
    if (n < 3) return 0;
    if (n > 9) n = 9; // a triangle against 6 planes has at most 9 corners
    for (int i = 0; i < n; ++i) out[i] = src[i];
    return n;
}

} // namespace nnh::raster3d
