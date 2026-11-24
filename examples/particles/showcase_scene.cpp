#include "showcase_scene.hpp"

#include <cstdio>
#include <cstring>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

void ShowcaseScene::on_enter() {
    frame_ = 0;
    nnh::gfx::set_camera(nnh::Screen::Top, {});
    nnh::gfx::set_camera(nnh::Screen::Bottom, {});
}

/// PARTICLES_SHOT: fixed inputs at a fixed frame for the simulator's screenshots.
void ShowcaseScene::shot(int frame) {
    const char* s = shot_name();
    if (s[0] == '\0' || frame != 10) return;
    if (std::strcmp(s, "explosion") == 0) gallery_.explode();
    if (std::strcmp(s, "touch") == 0) panel_.burst(120, 64);
    if (std::strcmp(s, "local") == 0) gallery_.set_local(true);
    if (std::strcmp(s, "full") == 0) panel_.overfill();
}

void ShowcaseScene::update() {
    namespace in = nnh::input;
    if (in::pressed(nnh::Button::Start)) {
        nnh::scenes::pop();
        return;
    }
    if (in::pressed(nnh::Button::Select)) {
        nnh::scenes::replace(*demos_.check);
        return;
    }
    if (in::pressed(nnh::Button::Y)) {
        nnh::scenes::replace(*demos_.bench);
        return;
    }
    if (frame_ == 0) NNH_LOG_INFO(example, "particles: showcase"); // in a frame a remote script sees (not on_enter's)
    shot(frame_);
    if (in::pressed(nnh::Button::A)) {
        gallery_.explode();
        explosion_log_ = frame_ + 1;
    }
    if (in::pressed(nnh::Button::X)) {
        local_ = !local_;
        gallery_.set_local(local_);
        NNH_LOG_INFO(example, "space: %s", local_ ? "local" : "world");
    }
    if (in::pressed(nnh::Button::B)) {
        rain_front_ = !rain_front_;
        gallery_.set_rain_front(rain_front_);
        NNH_LOG_INFO(example, "layer: %s", rain_front_ ? "front" : "behind");
    }
    if (in::pressed(nnh::Button::Down)) {
        gallery_.toggle_smoke();
        NNH_LOG_INFO(example, "smoke: %s", gallery_.smoke_on() ? "started" : "stopped");
    }
    const nnh::Stick pad = in::circle_pad();
    gallery_.update({pad.x, pad.y}, frame_);
    panel_.update();
    if (frame_ == explosion_log_) {
        const nnh::ParticleEmitter& e = gallery_.emitter(Gallery::kExplosion);
        NNH_LOG_INFO(example, "explosion: %u live %d", static_cast<unsigned>(e.stats().emitted), e.alive());
    }
    ++frame_;
}

void ShowcaseScene::draw_hud() const {
    namespace gfx = nnh::gfx;
    constexpr nnh::Screen kBottom = nnh::Screen::Bottom;
    char line[64];
    std::snprintf(line, sizeof line, "touch: %s  (L/R)", TouchPanel::name(panel_.selected()));
    gfx::draw_text(kBottom, {8, 6}, line, {.color = nnh::colors::yellow, .layer = 6});
    int y = 20;
    for (int i = 0; i < Gallery::kEmitters; ++i) {
        const nnh::ParticleStats s = gallery_.emitter(i).stats();
        std::snprintf(line, sizeof line, "%-9s %4d/%-4d drawn %4d", Gallery::name(i), s.live, s.capacity, s.drawn);
        gfx::draw_text(kBottom, {8, static_cast<float>(y)}, line, {.color = nnh::colors::gray, .layer = 6});
        y += 10;
    }
    const nnh::ParticleStats t = panel_.emitter(panel_.selected()).stats();
    std::snprintf(line, sizeof line, "%-9s %4d/%-4d dropped %u", TouchPanel::name(panel_.selected()), t.live,
                  t.capacity, static_cast<unsigned>(t.dropped));
    gfx::draw_text(kBottom, {8, static_cast<float>(y)}, line, {.color = nnh::colors::gray, .layer = 6});
    const gfx::Stats g = gfx::stats();
    std::snprintf(line, sizeof line, "particles %d/%d  dropped %d", g.particles_last_frame, g.max_particles,
                  g.dropped_particles);
    gfx::draw_text(kBottom, {8, 210}, line, {.layer = 6});
    gfx::draw_text(kBottom, {8, 222}, "A boom X space B rain DOWN smoke", {.color = nnh::colors::gray, .layer = 6});
}

void ShowcaseScene::draw() {
    gallery_.draw();
    panel_.draw(gallery_.sheet());
    draw_hud();
}
