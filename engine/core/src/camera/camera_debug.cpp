// CameraRig::debug_draw() (spec 018 FR-14): the dead zone (yellow), the limits (red) and the target (green cross)
// on the driven screen(s), layer 15, in screen pixels. Linked only by games that call it in Debug/Development (the
// header's inline function is empty in Release).

#include "camera/camera_internal.hpp"

#include <initializer_list>

namespace nnh::detail {
namespace {

constexpr ShapeOptions kOnTop{.layer = 15, .ignore_camera = true};
constexpr float kCross = 4; // half the cross, screen pixels

bool visible(Screen screen, Rect r) {
    return r.x < static_cast<float>(screen_width(screen)) && r.y < cam::kScreenHeight && r.x + r.w > 0 && r.y + r.h > 0;
}

/// A world rectangle in `screen`'s pixels.
Rect on_screen(const CameraRig& rig, Screen screen, Rect world) {
    const float z = rig.settings().zoom;
    const Vec2 a = rig.to_screen(screen, {world.x, world.y});
    return {a.x, a.y, world.w * z, world.h * z};
}

void draw_screen(const CameraRig& rig, Screen screen) {
    const CameraSettings& s = rig.settings();
    const Rect z = s.dead_zone;
    if (z.w > 0 || z.h > 0) {
        // Around the drawn view's centre (in world pixels, so it lands right on both screens of a tall world).
        const Vec2 centre = rig.position() + cam::view_size(s) * 0.5f;
        const Rect zone = on_screen(rig, screen, {centre.x + z.x / s.zoom, centre.y + z.y / s.zoom, z.w / s.zoom,
                                                  z.h / s.zoom});
        if (visible(screen, zone)) gfx::draw_rect(screen, zone, colors::yellow, kOnTop);
    }
    if (s.has_limits) {
        const Rect limits = on_screen(rig, screen, s.limits);
        if (visible(screen, limits)) gfx::draw_rect(screen, limits, colors::red, kOnTop);
    }
    const Vec2 t = rig.to_screen(screen, rig.aim());
    gfx::draw_line(screen, {t.x - kCross, t.y}, {t.x + kCross, t.y}, colors::green, kOnTop);
    gfx::draw_line(screen, {t.x, t.y - kCross}, {t.x, t.y + kCross}, colors::green, kOnTop);
}

} // namespace

void camera_debug_draw(const CameraRig& rig) {
    for (Screen screen : {Screen::Top, Screen::Bottom}) {
        if (rig.drives(screen)) draw_screen(rig, screen);
    }
}

} // namespace nnh::detail
