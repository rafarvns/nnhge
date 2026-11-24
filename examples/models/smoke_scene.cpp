#include "smoke_scene.hpp"

#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

void SmokeScene::on_enter() {
    lit_ = false;
    NNH_LOG_INFO(example, "smoke: flat");
}

void SmokeScene::update() {
    ++frame_;
    if (nnh::input::pressed(nnh::Button::Start)) return nnh::scenes::pop();
    if (nnh::input::pressed(nnh::Button::A) && !lit_) {
        lit_ = true;
        NNH_LOG_INFO(example, "smoke: lit");
    }
}

void SmokeScene::draw() {
    nnh::Camera3D cam;
    cam.position = {2.2f, 1.8f, 3.2f};
    nnh::gfx3d::point_at(cam, {});
    nnh::gfx3d::set_camera(nnh::Screen::Top, cam);
    const float spin = static_cast<float>(frame_) * 0.01f;
    const nnh::Material3D flat{.base_color = {230, 120, 40, 255}, .lit = false};
    const nnh::Material3D lit{.base_color = {255, 255, 255, 255}, .specular = {255, 255, 255, 255},
                              .texture = nnh::texture_id(world_.checker), .shininess = 4};
    nnh::gfx3d::draw(nnh::Screen::Top, world_.cube,
                     nnh::Transform3D{.rotation = nnh::quat_axis_angle({0, 1, 0}, spin), .scale = {1.6f, 1.6f, 1.6f}},
                     {.material = lit_ ? &lit : &flat});
    nnh::gfx::draw_text(nnh::Screen::Bottom, {8, 8}, lit_ ? "smoke: lit textured cube" : "smoke: flat cube\nA: lit", {});
}
