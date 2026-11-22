// CameraRig construction, settings and their validation (spec 018 FR-1, FR-15), setters, zoom about the
// centre.

#include "camera/camera_internal.hpp"
#include "nnh/assert.hpp"

#include <initializer_list>

namespace nnh {

NNH_LOG_CHANNEL(camera);

namespace {

using detail::cam::finite;

/// Checks and repairs `s` (Release values), `prev` giving the value kept for NaN. Each problem asserts in
/// Debug/Development (through `rig.misuse`).
template <typename Misuse> void sanitize(CameraSettings& s, const CameraSettings& prev, Misuse&& misuse) {
    if (misuse(finite(s.zoom) && s.zoom >= detail::cam::kMinZoom && s.zoom <= detail::cam::kMaxZoom,
               "zoom must be 0.5..4")) {
        s.zoom = !finite(s.zoom)
                     ? prev.zoom
                     : (s.zoom < detail::cam::kMinZoom ? detail::cam::kMinZoom : detail::cam::kMaxZoom);
    }
    if (misuse(s.dead_zone.w >= 0 && s.dead_zone.h >= 0 && finite(s.dead_zone.x) && finite(s.dead_zone.y) &&
                   finite(s.dead_zone.w) && finite(s.dead_zone.h),
               "the dead zone must be finite with a size >= 0")) {
        s.dead_zone = prev.dead_zone;
    }
    for (float* f : {&s.smoothing.x, &s.smoothing.y}) {
        if (misuse(*f >= kCameraMinSmoothing && *f <= 1, "smoothing must be in (0, 1]")) {
            *f = !(*f == *f) ? 1.0f : (*f > 1 ? 1.0f : kCameraMinSmoothing);
        }
    }
    if (misuse(s.trauma_decay > 0 && finite(s.trauma_decay), "trauma_decay must be > 0")) {
        s.trauma_decay = prev.trauma_decay;
    }
    if (misuse(s.shake_speed > 0 && s.shake_speed <= 1, "shake_speed must be in (0, 1]")) {
        s.shake_speed = s.shake_speed > 1 ? 1.0f : prev.shake_speed;
    }
    if (misuse(s.gap >= 0 && finite(s.gap), "gap must be >= 0")) s.gap = !(s.gap == s.gap) ? prev.gap : 0;
    if (misuse(finite(s.offset), "the offset must be finite")) s.offset = prev.offset;
    if (misuse(finite(s.max_shake) && s.max_shake.x >= 0 && s.max_shake.y >= 0,
               "max_shake must be finite, >= 0")) {
        s.max_shake = prev.max_shake;
    }
    if (misuse(finite(s.limits.x) && finite(s.limits.y) && finite(s.limits.w) && finite(s.limits.h) &&
                   s.limits.w >= 0 && s.limits.h >= 0,
               "limits must be finite with a size >= 0")) {
        s.limits = prev.limits;
    }
}

} // namespace

CameraRig::CameraRig(const CameraSettings& settings) {
    set(settings);
}

bool CameraRig::misuse(bool ok, const char* what) {
    if (ok) return false;
    NNH_ASSERT(false, "camera: %s", what);
    if ((flags_ & detail::cam::kFlagMisuseLogged) == 0)
        NNH_LOG_ERROR(camera, "%s (clamped or ignored)", what);
    flags_ |= detail::cam::kFlagMisuseLogged;
    return true;
}

void CameraRig::move_all(Vec2 delta) {
    desired_ += delta;
    followed_ += delta;
    final_ += delta;
}

void CameraRig::set(const CameraSettings& settings) {
    CameraSettings s = settings;
    sanitize(s, settings_, [this](bool ok, const char* what) { return misuse(ok, what); });
    if (s.zoom != settings_.zoom) {
        // Zoom about the view's centre (FR-9): the old half view minus the new one, at the new layout and
        // gap.
        CameraSettings old = s;
        old.zoom = settings_.zoom;
        move_all(detail::cam::view_size(old) * 0.5f - detail::cam::view_size(s) * 0.5f);
    }
    settings_ = s;
}

void CameraRig::set_offset(Vec2 offset) {
    if (misuse(finite(offset), "the offset must be finite")) return;
    settings_.offset = offset;
}

void CameraRig::set_zoom(float zoom) {
    CameraSettings s = settings_;
    s.zoom = zoom;
    set(s);
}

void CameraRig::set_layout(CameraLayout layout, Screen screen) {
    settings_.layout = layout;
    settings_.screen = screen;
}

} // namespace nnh
