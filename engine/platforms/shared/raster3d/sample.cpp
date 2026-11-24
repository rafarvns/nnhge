// Texture sampling (raster3d.hpp, FR-22): nearest or bilinear, repeat / clamp / mirror per axis. Texture coordinates
// (0, 0) are the image's top-left corner; texel centres at +½.

#include <cmath>

#include "raster3d.hpp"

namespace nnh::raster3d {
namespace {

int wrap(int i, int n, std::uint8_t mode) {
    if (mode == 1) return i < 0 ? 0 : (i >= n ? n - 1 : i); // clamp
    if (mode == 2) {                                         // mirror: period 2n
        int m = i % (2 * n);
        if (m < 0) m += 2 * n;
        return m < n ? m : 2 * n - 1 - m;
    }
    const int m = i % n; // repeat
    return m < 0 ? m + n : m;
}

/// floor for the range a texture coordinate × size can take (clamped to ±2²⁴ first).
int floor_int(float x) {
    if (x > 16777216.0f) x = 16777216.0f;
    if (x < -16777216.0f) x = -16777216.0f;
    return static_cast<int>(std::floor(x));
}

void texel(const TextureView& t, int x, int y, float out[4]) {
    const std::uint8_t* p = t.rgba + (static_cast<std::size_t>(y) * t.width + x) * 4;
    for (int c = 0; c < 4; ++c) out[c] = static_cast<float>(p[c]) / 255.0f;
}

} // namespace

void sample(const TextureView& t, float u, float v, std::uint8_t filter, std::uint8_t wrap_u, std::uint8_t wrap_v,
            float out[4]) {
    const float fx = u * static_cast<float>(t.width), fy = v * static_cast<float>(t.height);
    if (filter == 0) {
        texel(t, wrap(floor_int(fx), t.width, wrap_u), wrap(floor_int(fy), t.height, wrap_v), out);
        return;
    }
    const float sx = fx - 0.5f, sy = fy - 0.5f;
    const int x0 = floor_int(sx), y0 = floor_int(sy);
    const float ax = sx - static_cast<float>(x0), ay = sy - static_cast<float>(y0);
    float a[4], b[4], c[4], d[4];
    const int xa = wrap(x0, t.width, wrap_u), xb = wrap(x0 + 1, t.width, wrap_u);
    const int ya = wrap(y0, t.height, wrap_v), yb = wrap(y0 + 1, t.height, wrap_v);
    texel(t, xa, ya, a), texel(t, xb, ya, b), texel(t, xa, yb, c), texel(t, xb, yb, d);
    for (int k = 0; k < 4; ++k) {
        const float top = a[k] + (b[k] - a[k]) * ax, bottom = c[k] + (d[k] - c[k]) * ax;
        out[k] = top + (bottom - top) * ay;
    }
}

} // namespace nnh::raster3d
