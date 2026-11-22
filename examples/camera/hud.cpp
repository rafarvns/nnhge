#include "hud.hpp"

#include "camera_scene.hpp"

#include <cstdio>
#include <nnh/gfx.hpp>

namespace {

const char* mode_name(CameraScene::Mode mode) {
    switch (mode) {
    case CameraScene::Mode::Single:
        return "SINGLE VIEW";
    case CameraScene::Mode::Tall:
        return "TALL WORLD";
    case CameraScene::Mode::Two:
        return "TWO VIEWS";
    }
    return "";
}

constexpr nnh::TextOptions kOnTop{.shadow = true, .layer = 15, .ignore_camera = true};

} // namespace

void draw_hud(const CameraScene& scene) {
    namespace gfx = nnh::gfx;
    const nnh::CameraRig& rig = scene.rig();
    char line[512];
    std::snprintf(line, sizeof line, "%s  zoom %.2f%s", mode_name(scene.mode()),
                  static_cast<double>(scene.zoom()), rig.settings().gap > 0 ? "  gap 24" : "");
    if (scene.mode() != CameraScene::Mode::Single) {
        gfx::draw_text(nnh::Screen::Top, {4, 4}, line, kOnTop);
        return;
    }
    const nnh::Vec2 p = rig.position();
    std::snprintf(line, sizeof line,
                  "CAMERA HELPERS (spec 018)\n\n%s\nzoom       %.2f\nsmoothing  %s\ntrauma     %.2f\n"
                  "camera     %.1f, %.1f\n\n"
                  "Pad    move       A  shake\nX      zoom       Y  smoothing\nR      mode       L  gap\n"
                  "touch  marker (tall world, two views)\nSELECT self-check  START quit\n\n"
                  "yellow: dead zone  red: limits",
                  mode_name(scene.mode()), static_cast<double>(scene.zoom()),
                  scene.smoothing() ? "on" : "off", static_cast<double>(rig.trauma()),
                  static_cast<double>(p.x), static_cast<double>(p.y));
    gfx::draw_text(nnh::Screen::Bottom, {8, 8}, line);
}
