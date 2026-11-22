// Following (spec 018 FR-2..FR-4): the target's centre, the dead zone and smoothing, one axis at a time.

#include "camera/camera_internal.hpp"

namespace nnh::detail::cam {

Vec2 box_centre(Rect r) {
    return {r.x + r.w / 2, r.y + r.h / 2};
}

Zone zone_axis(float zone_pos, float zone_size, float zoom, float half, bool& clamped) {
    Zone z{zone_pos / zoom, zone_size / zoom};
    if (z.pos >= -half && z.pos + z.size <= half) return z; // the usual case: untouched bits
    float lo = z.pos < -half ? -half : z.pos;
    if (lo > half) lo = half;
    float hi = z.pos + z.size > half ? half : z.pos + z.size;
    if (hi < lo) hi = lo;
    clamped = true;
    return {lo, hi - lo};
}

float desired_axis(float prev, float c, float half, Zone zone) {
    if (zone.size == 0) return c - half - zone.pos;
    const float left = prev + half + zone.pos;
    const float right = left + zone.size;
    if (c < left) return prev + (c - left);
    if (c > right) return prev + (c - right);
    return prev;
}

float smooth_axis(float followed, float desired, float f) {
    if (f >= 1) return desired; // instant: an assignment, never followed + (desired − followed)
    const float d = desired - followed;
    if (-kCameraSnapDistance < d && d < kCameraSnapDistance) return desired;
    return followed + d * f;
}

} // namespace nnh::detail::cam
