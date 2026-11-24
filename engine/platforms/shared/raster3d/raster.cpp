// Triangle setup and coverage (raster3d.hpp, FR-22/FR-23): 28.4 fixed-point vertices, int64 edge functions stepped
// per pixel, the top-left fill rule (shared edges: no gap, no pixel twice), pixel centres at +½, back faces by the
// sign of the area, depth affine in screen space, attributes perspective-correct (interpolated × 1/w, divided back).

#include <cmath>

#include "raster3d.hpp"

namespace nnh::raster3d {
namespace {

std::int32_t fixed(float s) {
    const float f = std::floor(s * 16.0f + 0.5f);
    return static_cast<std::int32_t>(f > 1e8f ? 1e8f : (f < -1e8f ? -1e8f : f));
}

std::int64_t edge(const ScreenVertex& v0, const ScreenVertex& v1, std::int64_t px, std::int64_t py) {
    return (static_cast<std::int64_t>(v1.x) - v0.x) * (py - v0.y) - (static_cast<std::int64_t>(v1.y) - v0.y) * (px - v0.x);
}

/// 0 for a top or left edge, −1 otherwise (those edges must be strictly inside).
std::int64_t bias(const ScreenVertex& v0, const ScreenVertex& v1) {
    const std::int64_t dx = static_cast<std::int64_t>(v1.x) - v0.x, dy = static_cast<std::int64_t>(v1.y) - v0.y;
    return (dy < 0 || (dy == 0 && dx > 0)) ? 0 : -1;
}

} // namespace

ScreenVertex to_screen(const ClipVertex& v, int width, int height) {
    ScreenVertex s;
    s.inv_w = 1.0f / v.clip[3];
    const float nx = v.clip[0] * s.inv_w, ny = v.clip[1] * s.inv_w;
    s.x = fixed((nx + 1.0f) * (0.5f * static_cast<float>(width)));
    s.y = fixed((1.0f - ny) * (0.5f * static_cast<float>(height)));
    s.z = v.clip[2] * s.inv_w;
    for (int i = 0; i < kAttributes; ++i) s.attr[i] = v.attr[i] * s.inv_w;
    return s;
}

int rasterize(const ScreenVertex& a, const ScreenVertex& b_in, const ScreenVertex& c_in, bool cull_back, int width,
              int height, const PixelSink& sink) {
    std::int64_t area = edge(a, b_in, c_in.x, c_in.y);
    if (area == 0 || (cull_back && area > 0)) return 0; // front faces are negative on the y-down screen
    const bool flip = area < 0;
    const ScreenVertex& b = flip ? c_in : b_in;
    const ScreenVertex& c = flip ? b_in : c_in;
    if (flip) area = -area;
    const std::int32_t minx = std::min(a.x, std::min(b.x, c.x)), maxx = std::max(a.x, std::max(b.x, c.x));
    const std::int32_t miny = std::min(a.y, std::min(b.y, c.y)), maxy = std::max(a.y, std::max(b.y, c.y));
    const int x0 = std::max(0, static_cast<int>((minx - 8) >> 4)), x1 = std::min(width - 1, static_cast<int>((maxx - 8 + 15) >> 4));
    const int y0 = std::max(0, static_cast<int>((miny - 8) >> 4)), y1 = std::min(height - 1, static_cast<int>((maxy - 8 + 15) >> 4));
    const std::int64_t ba = bias(b, c), bb = bias(c, a), bc = bias(a, b);
    const std::int64_t sx_bc = -(static_cast<std::int64_t>(c.y) - b.y) * 16, sx_ca = -(static_cast<std::int64_t>(a.y) - c.y) * 16,
                       sx_ab = -(static_cast<std::int64_t>(b.y) - a.y) * 16;
    const float inv_area = 1.0f / static_cast<float>(area);
    int covered = 0;
    float attr[kAttributes];
    for (int py = y0; py <= y1; ++py) {
        const std::int64_t cy = static_cast<std::int64_t>(py) * 16 + 8, cx = static_cast<std::int64_t>(x0) * 16 + 8;
        std::int64_t e_bc = edge(b, c, cx, cy), e_ca = edge(c, a, cx, cy), e_ab = edge(a, b, cx, cy);
        for (int px = x0; px <= x1; ++px, e_bc += sx_bc, e_ca += sx_ca, e_ab += sx_ab) {
            if (e_bc + ba < 0 || e_ca + bb < 0 || e_ab + bc < 0) continue;
            const float wa = static_cast<float>(e_bc) * inv_area, wb = static_cast<float>(e_ca) * inv_area,
                        wc = static_cast<float>(e_ab) * inv_area;
            const float z = (wa * a.z + wb * b.z) + wc * c.z;
            const float iw = (wa * a.inv_w + wb * b.inv_w) + wc * c.inv_w;
            const float w = 1.0f / iw;
            for (int i = 0; i < kAttributes; ++i) attr[i] = ((wa * a.attr[i] + wb * b.attr[i]) + wc * c.attr[i]) * w;
            sink.fn(sink.context, px, py, z, attr);
            ++covered;
        }
    }
    return covered;
}

} // namespace nnh::raster3d
