// The iris (plan 023 "Iris"): everything outside a circle or diamond, drawn as bands of `band` rows sampled at their
// middle row. R0 = ⌈√D⌉ (D: the largest squared distance from the centre to an area corner), r = R0 − ext(R0);
// fully covered bands merge into one rectangle (at most one run above and one below the shape). Integer-only: the
// square root is the bit method (no libm, no VFP), no division in the band loop.

#include "transition/effect_geometry.hpp"

namespace nnh::detail {
namespace {

std::uint32_t square(int v) { return static_cast<std::uint32_t>(v) * static_cast<std::uint32_t>(v); }

int max_corner_distance2(int cx, int cy, int w, int h) {
    // The farther of x = 0 and x = w (always ≥ w / 2 > 0, even for a centre outside the area); the same for y.
    const int dx = cx > w - cx ? cx : w - cx;
    const int dy = cy > h - cy ? cy : h - cy;
    return static_cast<int>(square(dx) + square(dy));
}

/// A run of fully covered rows waiting to be emitted as one rectangle.
struct FullRun {
    int start = -1;
    void flush(int end, int w, const RectTarget& t) {
        if (start >= 0) t.emit(0, start, w, end - start);
        start = -1;
    }
};

} // namespace

std::uint32_t isqrt(std::uint32_t v) {
    std::uint32_t result = 0;
    std::uint32_t bit = 1u << 30;
    while (bit > v) bit >>= 2;
    while (bit != 0) {
        if (v >= result + bit) {
            v -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result;
}

void geometry_iris(const ResolvedEffect& fx, int q, int w, int h, const RectTarget& t) {
    if (q <= 0) return;
    const int cx = fx.cx;
    const int cy = fx.cy;
    const std::uint32_t d2 = static_cast<std::uint32_t>(max_corner_distance2(cx, cy, w, h));
    std::uint32_t r0 = isqrt(d2);
    if (r0 * r0 < d2) ++r0; // ⌈√D⌉
    const int r = static_cast<int>(r0) - ext(q, static_cast<int>(r0));
    const std::uint32_t r2 = square(r);
    const bool circle = static_cast<transition::IrisShape>(fx.shape) == transition::IrisShape::Circle;
    const int band = fx.band;
    // Only the bands that meet the target's rows (one division per call).
    const int first_row = t.y0 > 0 ? t.y0 : 0;
    const int last_row = t.y1 < h ? t.y1 : h;
    FullRun run;
    int y1 = 0;
    for (int y0 = first_row > 0 ? first_row - first_row % band : 0; y0 < last_row; y0 += band) {
        y1 = y0 + band < h ? y0 + band : h;
        const int ym = y0 + ((y1 - y0) >> 1);
        const int dy = ym > cy ? ym - cy : cy - ym;
        if (dy >= r) {
            if (run.start < 0) run.start = y0;
            continue;
        }
        run.flush(y0, w, t);
        const int half = circle ? static_cast<int>(isqrt(r2 - square(dy))) : r - dy;
        const int left = cx - half;   // [0, left) is covered
        const int right = cx + half;  // [right, w) is covered
        if (left > 0) t.emit(0, y0, left < w ? left : w, y1 - y0);
        if (right < w) {
            const int x = right > 0 ? right : 0;
            t.emit(x, y0, w - x, y1 - y0);
        }
    }
    run.flush(y1, w, t);
}

} // namespace nnh::detail
