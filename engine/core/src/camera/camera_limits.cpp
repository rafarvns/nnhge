// Limits (spec 018 FR-6): the desired position clamped before smoothing, the final one after the shake; the
// small-world alignment on an axis where the limits are smaller than the view.

#include "camera/camera_internal.hpp"

namespace nnh::detail::cam {

float clamp_axis(float d, float lo, float size, float view, CameraAlign align) {
    const float hi = (lo + size) - view;
    if (hi >= lo) {
        // The order of the platformer's hand-written camera (high edge first), so -0 and edge bits match it.
        if (d > hi) d = hi;
        if (d < lo) d = lo;
        return d;
    }
    switch (align) {
    case CameraAlign::Start: return lo;
    case CameraAlign::End: return hi;
    case CameraAlign::Center: break;
    }
    return lo + (size - view) * 0.5f;
}

float final_clamp_axis(float v, float followed, float lo, float size, float view) {
    const float hi = (lo + size) - view;
    if (!(hi >= lo)) return v; // a small axis already shows past the level: the shake isn't clamped
    const float low = followed < lo ? followed : lo;
    const float high = followed > hi ? followed : hi;
    if (v > high) v = high;
    if (v < low) v = low;
    return v;
}

} // namespace nnh::detail::cam

namespace nnh {

void CameraRig::set_limits(Rect limits) {
    const bool ok = detail::cam::finite(limits.x) && detail::cam::finite(limits.y) && detail::cam::finite(limits.w) &&
                    detail::cam::finite(limits.h) && limits.w >= 0 && limits.h >= 0;
    if (misuse(ok, "limits must be finite with a size >= 0")) return;
    settings_.limits = limits;
    settings_.has_limits = true;
}

void CameraRig::clear_limits() { settings_.has_limits = false; }

} // namespace nnh
