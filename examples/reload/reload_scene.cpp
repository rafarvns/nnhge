// The hot reload showcase (reload_scene.hpp).

#include "reload_scene.hpp"

#include <cstdio>
#include <nnh/app.hpp>
#include <nnh/audio.hpp>
#include <nnh/gfx.hpp>
#include <nnh/gfx3d.hpp>
#include <nnh/input.hpp>
#include <nnh/localization.hpp>
#include <nnh/log.hpp>



namespace {

float wrap(float x, float speed) {
    x += speed;
    return x > 400 ? x - 400 : x;
}

} // namespace

void ReloadScene::on_enter() {
    reloaded_ = nnh::events::subscribe<nnh::AssetsReloaded>([this](const nnh::AssetsReloaded&) {
        if (!world_.refresh()) NNH_LOG_ERROR(example, "reload example: a reloaded asset could not be loaded again");
        report_at_ = frame_ + 2; // the frame after the batch's: a script's expect-reload matched that frame
    });
    (void)nnh::music::play("music/loop"); // reloads at the same sample (spec 031 FR-9)
    nnh::Camera3D camera;
    camera.position = {0, 0, 4};
    nnh::gfx3d::set_camera(nnh::Screen::Top, camera);
}

void ReloadScene::report() {
    char line[64];
    format_reload_line(line, sizeof line, reload_sum(world_.snapshot()), static_cast<int>(nnh::assets::reload_count()));
    NNH_LOG_INFO(example, "%s", line);
    if (world_.last[0] != '\0') NNH_LOG_INFO(example, "reload: changed %s", world_.last);
}

void ReloadScene::update() {
    world_.walking.update();
    if (++frame_ == report_at_) report(); // the first at frame 60: once a script listens (it attaches in the first frames)
    goblin_x_ = wrap(goblin_x_, world_.goblin.speed);
    bat_x_ = wrap(bat_x_, world_.bat.speed);
    if (nnh::input::pressed(nnh::Button::A)) (void)nnh::audio::play(world_.blip);
    if (bench_ != nullptr && nnh::input::pressed(nnh::Button::X)) nnh::scenes::push(*bench_);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void ReloadScene::draw() {
    using nnh::Screen;
    nnh::gfx::draw(Screen::Top, world_.backdrop, {0, 0}, {.layer = 0});
    nnh::gfx::draw_tilemap(Screen::Top, world_.room);
    if (world_.platform >= 0) {
        const nnh::Rect b = world_.room.object(world_.platform).bounds;
        nnh::gfx::fill_rect(Screen::Top, b, nnh::rgb(250, 220, 90), {.layer = 9});
    }
    nnh::gfx::draw(Screen::Top, world_.hero, {184, 176}, {.layer = 10});
    nnh::DrawOptions walker = world_.walking.draw_options();
    walker.layer = 10;
    walker.scale = {2, 2};
    nnh::gfx::draw(Screen::Top, world_.walker_art, {300, 176}, walker);
    nnh::gfx::draw_text(Screen::Bottom, {8, 200}, "12 34", {.font = world_.hud});
    nnh::gfx::fill_rect(Screen::Top, {goblin_x_, 200, 12, 12}, world_.goblin.tint, {.layer = 10});
    nnh::gfx::fill_rect(Screen::Top, {bat_x_, 60, 10, 6}, world_.bat.tint, {.layer = 10});
    const nnh::Quat spin = nnh::quat_axis_angle({0, 1, 0}, static_cast<float>(frame_ % 360) * 0.0174533f);
    nnh::gfx3d::draw(Screen::Top, world_.gem,
                     nnh::Transform3D{.position = {1.6f, 0.9f, 0}, .rotation = spin, .scale = {0.4f, 0.4f, 0.4f}});
    nnh::gfx::draw_text(Screen::Bottom, {8, 8}, nnh::loc::tr(nnh::loc::key("demo.hello")));
    nnh::gfx::draw_text(Screen::Bottom, {8, 24}, nnh::loc::tr(nnh::loc::key("demo.hint")));
    char line[128];
    std::snprintf(line, sizeof line, "%s: %u  %s", nnh::loc::tr(nnh::loc::key("demo.count")),
                  static_cast<unsigned>(nnh::assets::reload_count()), world_.last);
    nnh::gfx::draw_text(Screen::Bottom, {8, 220}, line);
}
