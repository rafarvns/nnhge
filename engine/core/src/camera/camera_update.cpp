// update() and snap() (spec 018 FR-5, FR-12): one 60 Hz frame in the plan's fixed order — target, desired (dead
// zone), clamp, smoothing, trauma decay, shake, final clamp.

#include "camera/camera_internal.hpp"
#include "nnh/profile.hpp"

namespace nnh {
namespace {

using namespace detail::cam;

/// The shaken position, clamped to the limits when asked (step 7).
Vec2 shaken(const CameraSettings& s, Vec2 followed, float trauma, std::uint32_t t, Vec2 view) {
    Vec2 v = followed;
    if (trauma > 0) v = followed + shake_offset(s, trauma, t);
    if (s.has_limits && s.shake_inside_limits) {
        v.x = final_clamp_axis(v.x, followed.x, s.limits.x, s.limits.w, view.x);
        v.y = final_clamp_axis(v.y, followed.y, s.limits.y, s.limits.h, view.y);
    }
    return v;
}

} // namespace

void CameraRig::place(Vec2 target, bool instant) {
    if (misuse(finite(target), "the target must be finite")) return; // Release: this update is skipped
    const CameraSettings& s = settings_;
    const Vec2 view = view_size(s);
    const Vec2 half = view * 0.5f;
    const Vec2 c = target + s.offset;
    aim_ = c;
    Vec2 d = desired_;
    if (instant) {
        d = c - half;
    } else {
        bool clamped = false;
        const Zone zx = zone_axis(s.dead_zone.x, s.dead_zone.w, s.zoom, half.x, clamped);
        const Zone zy = zone_axis(s.dead_zone.y, s.dead_zone.h, s.zoom, half.y, clamped);
        if (s.follow_x) d.x = desired_axis(d.x, c.x, half.x, zx);
        if (s.follow_y) d.y = desired_axis(d.y, c.y, half.y, zy);
        if (clamped && (flags_ & kFlagZoneLogged) == 0) {
            if constexpr (build::kDevToolsEnabled) NNH_LOG_WARN(camera, "dead zone larger than the view: clamped to it");
            flags_ |= kFlagZoneLogged;
        }
    }
    if (s.has_limits) {
        d.x = clamp_axis(d.x, s.limits.x, s.limits.w, view.x, s.small_world);
        d.y = clamp_axis(d.y, s.limits.y, s.limits.h, view.y, s.small_world);
    }
    desired_ = d;
    if (instant) {
        followed_ = d;
    } else {
        followed_ = {smooth_axis(followed_.x, d.x, s.smoothing.x), smooth_axis(followed_.y, d.y, s.smoothing.y)};
        float t = trauma_ - s.trauma_decay;
        trauma_ = t > 0 ? t : 0;
        noise_time_ += noise_step(s.shake_speed);
    }
    final_ = shaken(s, followed_, trauma_, noise_time_, view);
}

void CameraRig::update(Vec2 target) {
    NNH_PROFILE_SCOPE("camera");
    place(target, false);
}

void CameraRig::update(Rect target) { update(box_centre(target)); }

void CameraRig::snap(Vec2 target) { place(target, true); }

void CameraRig::snap(Rect target) { snap(box_centre(target)); }

} // namespace nnh
