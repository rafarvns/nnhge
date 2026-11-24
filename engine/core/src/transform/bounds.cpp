// Axis-aligned bounds of a local rectangle under a map (spec 022 FR-14). Not a hot path of the pass: float compares
// are fine here.

#include "transform/sin_cos.hpp"

namespace nnh {
namespace {

/// The range covered on one axis by [r, r + size] scaled by `a` and moved by `origin`, normalised.
void axis(float origin, float a, float r, float size, float& at, float& extent) {
    extent = (a < 0.0f ? -a : a) * size;
    at = origin + a * r;
    if (a < 0.0f) at = at - extent;
}

} // namespace

Rect bounds(const Affine2D& map, Rect local) {
    if ((detail::float_bits(map.x.y) & ~detail::kSignBit) == 0 && (detail::float_bits(map.y.x) & ~detail::kSignBit) == 0) {
        Rect out;
        axis(map.origin.x, map.x.x, local.x, local.w, out.x, out.w);
        axis(map.origin.y, map.y.y, local.y, local.h, out.y, out.h);
        return out;
    }
    if ((detail::float_bits(map.x.x) & ~detail::kSignBit) == 0 && (detail::float_bits(map.y.y) & ~detail::kSignBit) == 0) {
        // A quarter turn: x comes from the local y and y from the local x, exactly.
        Rect out;
        axis(map.origin.x, map.y.x, local.y, local.h, out.x, out.w);
        axis(map.origin.y, map.x.y, local.x, local.w, out.y, out.h);
        return out;
    }
    const Vec2 corners[4] = {transform_point(map, {local.x, local.y}), transform_point(map, {local.x + local.w, local.y}),
                             transform_point(map, {local.x, local.y + local.h}),
                             transform_point(map, {local.x + local.w, local.y + local.h})};
    float x0 = corners[0].x, x1 = x0, y0 = corners[0].y, y1 = y0;
    for (const Vec2& c : corners) {
        x0 = c.x < x0 ? c.x : x0;
        x1 = c.x > x1 ? c.x : x1;
        y0 = c.y < y0 ? c.y : y0;
        y1 = c.y > y1 ? c.y : y1;
    }
    return {x0, y0, x1 - x0, y1 - y0};
}

} // namespace nnh
