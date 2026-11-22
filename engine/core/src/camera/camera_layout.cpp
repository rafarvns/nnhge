// Views per layout, the per-screen cameras, conversions and pixel snap (spec 018 FR-10, FR-11, FR-13).

#include "camera/camera_internal.hpp"
#include "nnh/assert.hpp"

namespace nnh::detail::cam {

Vec2 view_size(const CameraSettings& s) {
    if (s.layout == CameraLayout::TallWorld) return {kTallWidth / s.zoom, (2 * kScreenHeight) / s.zoom + s.gap};
    return {static_cast<float>(screen_width(s.screen)) / s.zoom, kScreenHeight / s.zoom};
}

Screen primary(const CameraSettings& s) { return s.layout == CameraLayout::TallWorld ? Screen::Top : s.screen; }

float round_half_up(float v) {
    // Truncation towards zero, then down one for negative non-integers: floor(v + 0.5) for |v| < 2^30.
    const float x = v + 0.5f;
    if (!(x > -1073741824.0f && x < 1073741824.0f)) return x; // out of range (or NaN): unchanged
    const auto t = static_cast<float>(static_cast<std::int32_t>(x));
    return t > x ? t - 1 : t;
}

} // namespace nnh::detail::cam

namespace nnh {

bool CameraRig::drives(Screen screen) const {
    return settings_.layout == CameraLayout::TallWorld || screen == settings_.screen;
}

Camera2D CameraRig::camera(Screen screen) const {
    if (!drives(screen)) {
        NNH_ASSERT(false, "camera: this rig doesn't drive the %s screen", screen == Screen::Top ? "top" : "bottom");
        screen = detail::cam::primary(settings_);
    }
    const float z = settings_.zoom;
    Vec2 p = final_;
    if (settings_.layout == CameraLayout::TallWorld && screen == Screen::Bottom) {
        p += Vec2{static_cast<float>(kTallWorldInset) / z, detail::cam::kScreenHeight / z + settings_.gap};
    }
    if (settings_.pixel_snap) {
        p = {detail::cam::round_half_up(p.x * z) / z, detail::cam::round_half_up(p.y * z) / z};
    }
    Camera2D c;
    c.position = p;
    return c;
}

Rect CameraRig::view(Screen screen) const {
    const Camera2D c = camera(screen); // asserts for a screen the rig doesn't drive
    const Screen s = drives(screen) ? screen : detail::cam::primary(settings_);
    const float z = settings_.zoom;
    return {c.position.x, c.position.y, static_cast<float>(screen_width(s)) / z, detail::cam::kScreenHeight / z};
}

Vec2 CameraRig::to_world(Screen screen, Vec2 p) const { return camera(screen).position + p / settings_.zoom; }

Vec2 CameraRig::to_screen(Screen screen, Vec2 w) const { return (w - camera(screen).position) * settings_.zoom; }

} // namespace nnh
