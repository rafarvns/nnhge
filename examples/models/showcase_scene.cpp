#include "showcase_scene.hpp"

#include <cstdio>
#include <nnh/debug.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/system.hpp>
#include <nnh/time.hpp>
#include <nnh/transform.hpp>

namespace {

struct Button3D {
    const char* name;
    nnh::Rect rect;
};
constexpr Button3D kButtons[6] = {{"texture", {10, 40, 95, 50}},  {"lighting", {115, 40, 95, 50}},
                                  {"point light", {220, 40, 95, 50}}, {"culling", {10, 110, 95, 50}},
                                  {"tint", {115, 110, 95, 50}},     {"debug draw", {10, 180, 95, 50}}};

bool inside(nnh::Rect r, nnh::TouchPoint p) {
    return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

const char* on_off(bool b) { return b ? "on" : "off"; }

} // namespace

void ShowcaseScene::on_enter() {
    NNH_LOG_INFO(example, "models: showcase");
    nnh::gfx3d::set_world_layer(nnh::Screen::Top, 8);
    slider_ = -1;
    eyes_ = -1;
}

// The slider when it moves, and the eyes drawn when their number changes (a frame later: stats are the last frame's),
// so a script can check stereo (spec 026 FR-14/FR-16).
void ShowcaseScene::log_stereo() {
    const float s = nnh::system::stereo_slider();
    if (s != slider_) {
        slider_ = s;
        NNH_LOG_INFO(example, "slider: %.2f", static_cast<double>(s));
    }
    const int eyes = nnh::gfx3d::stats().eyes;
    if (eyes > 0 && eyes != eyes_) {
        eyes_ = eyes;
        NNH_LOG_INFO(example, "eyes: %d", eyes);
    }
}

nnh::Camera3D ShowcaseScene::camera() const {
    nnh::Camera3D cam;
    const nnh::SinCos y = nnh::sin_cos(yaw_), p = nnh::sin_cos(pitch_);
    const float horizontal = distance_ * p.cos;
    cam.position = {horizontal * y.sin, 0.8f + distance_ * p.sin, horizontal * y.cos};
    nnh::gfx3d::point_at(cam, {0, 0.8f, 0});
    cam.far_plane = 60;
    if (ortho_) {
        cam.projection = nnh::Projection::Orthographic;
        cam.ortho_height = distance_ * 0.9f;
        cam.near_plane = 0.1f;
    }
    return cam;
}

void ShowcaseScene::log_tag() {
    const nnh::ScreenPoint p = nnh::gfx3d::to_screen(camera(), {0, 1.9f, 0});
    NNH_LOG_INFO(example, "tag: %.0f,%.0f depth %.2f", static_cast<double>(p.pixel.x), static_cast<double>(p.pixel.y),
                 static_cast<double>(p.depth));
}

void ShowcaseScene::touch_buttons() {
    if (!nnh::input::touch_started()) return;
    const nnh::TouchPoint t = nnh::input::touch_position();
    for (int i = 0; i < 6; ++i) {
        if (!inside(kButtons[i].rect, t)) continue;
        bool* flags[6] = {&textures_, &lighting_, &point_light_, &culling_, nullptr, &debug_};
        if (flags[i] != nullptr) {
            *flags[i] = !*flags[i];
            NNH_LOG_INFO(example, "%s: %s", kButtons[i].name, on_off(*flags[i]));
        } else {
            flash_ = 20;
            NNH_LOG_INFO(example, "tint: flash");
        }
        nnh::gfx3d::set_debug_draw(debug_);
        log_tag();
    }
}

void ShowcaseScene::update() {
    ++frame_;
    if (nnh::input::pressed(nnh::Button::Start)) return nnh::scenes::pop();
    if (nnh::input::pressed(nnh::Button::Select) && demos_.check != nullptr) return nnh::scenes::replace(*demos_.check);
    if (nnh::input::pressed(nnh::Button::Y) && demos_.bench != nullptr) return nnh::scenes::replace(*demos_.bench);
    if (nnh::input::pressed(nnh::Button::Down) && demos_.smoke != nullptr) return nnh::scenes::replace(*demos_.smoke);
    const nnh::Stick pad = nnh::input::circle_pad();
    const bool moved = pad.x != 0 || pad.y != 0;
    yaw_ = yaw_ + pad.x * 0.03f;
    pitch_ = pitch_ + pad.y * 0.02f;
    pitch_ = pitch_ < 0.05f ? 0.05f : (pitch_ > 1.3f ? 1.3f : pitch_);
    if (nnh::input::held(nnh::Button::L)) distance_ = distance_ < 14 ? distance_ + 0.1f : distance_;
    if (nnh::input::held(nnh::Button::R)) distance_ = distance_ > 3 ? distance_ - 0.1f : distance_;
    if (nnh::input::pressed(nnh::Button::X)) {
        ortho_ = !ortho_;
        NNH_LOG_INFO(example, "projection: %s", ortho_ ? "orthographic" : "perspective");
        log_tag();
    }
    if (moved_ && !moved) log_tag(); // the stick came back to rest: where the tag is now
    moved_ = moved;
    touch_buttons();
    log_stereo();
    if (flash_ > 0) --flash_;
}

void ShowcaseScene::draw() {
    nnh::gfx3d::set_camera(nnh::Screen::Top, camera());
    draw_world();
    draw_sky_and_hud();
    draw_bottom();
}
