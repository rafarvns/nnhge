#include "bench.hpp"

#include <nnh/gfx.hpp>

namespace {

using nnh::Frames;
using nnh::ParticleDesc;

/// AC-7's update workload: every feature the target names.
ParticleDesc steady(float rate) {
    ParticleDesc d;
    d.rate = rate;
    d.spread = nnh::kFullTurn;
    d.speed = {20, 60};
    d.gravity = {0, 40};
    d.damping = 0.5f;
    d.spin = {-3, 3};
    d.lifetime = {nnh::FrameTime(nnh::Duration::from_ms(900)), nnh::FrameTime(nnh::Duration::from_ms(1100))};
    d.color_keys[0] = {0, nnh::colors::white};
    d.color_keys[1] = {0.3f, nnh::colors::yellow};
    d.color_keys[2] = {0.7f, nnh::rgba(255, 80, 0, 200)};
    d.color_keys[3] = {1, nnh::rgba(80, 0, 0, 0)};
    d.size_keys[0] = {0, 0.5f};
    d.size_keys[1] = {0.3f, 1.5f};
    d.size_keys[2] = {1, 0.2f};
    for (int i = 0; i < 4; ++i) d.frames[i].region = {8 * i, 8, 8, 8};
    d.frame_mode = nnh::ParticleFrameMode::OverLife;
    d.prewarm = 120;
    return d;
}

/// emit512's bursts: circle area, every direction, every range random.
ParticleDesc bursty() {
    ParticleDesc d = steady(0);
    d.prewarm = 0;
    d.shape = nnh::ParticleShape::Circle;
    d.radius = 12;
    d.rotation = {0, nnh::kFullTurn};
    d.color_min = nnh::rgba(100, 100, 100, 255);
    d.size = {0.5f, 2};
    d.frame_mode = nnh::ParticleFrameMode::Random;
    return d;
}

bool make(nnh::ParticleEmitter& out, const ParticleDesc& d, int capacity) {
    auto e = nnh::ParticleEmitter::create(d, capacity);
    if (!e) return false;
    out = static_cast<nnh::ParticleEmitter&&>(e.value());
    return true;
}

} // namespace

bool ParticleBench::init(const nnh::Texture* sheet) {
    sheet_ = sheet;
    if (!make(one_, steady(1000), 1024) || !make(burst_, bursty(), 8192)) return false;
    for (nnh::ParticleEmitter& e : many_) {
        if (!make(e, steady(62.5f), 64)) return false;
    }
    one_.set_position({200, 120}, true);
    // 512 frozen particles spread over the top screen (drawn, never updated).
    ParticleDesc d = bursty();
    d.shape = nnh::ParticleShape::Box;
    d.box = {360, 200};
    d.speed = {0, 0};
    if (!make(frozen_, d, kDrawn)) return false;
    frozen_.set_position({200, 120}, true);
    frozen_.emit(kDrawn);
    ParticleDesc live = steady(560);
    live.shape = nnh::ParticleShape::Box;
    live.box = {360, 200};
    if (!make(live_, live, 640)) return false;
    live_.set_position({200, 120}, true);
    return true;
}

void ParticleBench::update1024() { one_.update(); }

void ParticleBench::update16x64() {
    for (nnh::ParticleEmitter& e : many_) e.update();
}

void ParticleBench::clear_emit512() { burst_.clear(); }

int ParticleBench::emit512() {
    int made = 0;
    for (int i = 0; i < 32; ++i) {
        seed_ = seed_ * 1664525u + 1013904223u;
        made += burst_.emit_at(nnh::Vec2{static_cast<float>(seed_ % 400), static_cast<float>((seed_ >> 12) % 240)}, 16);
    }
    return made;
}

void ParticleBench::draw512() const { frozen_.draw(nnh::Screen::Top, *sheet_, {.layer = 2}); }

void ParticleBench::sprite512() const {
    for (int i = 0; i < frozen_.alive(); ++i) {
        const nnh::ParticleView v = frozen_.particle(i);
        nnh::gfx::draw(nnh::Screen::Top, *sheet_, v.position,
                       {.region = {8 * v.frame, 8, 8, 8}, .pivot = {4, 4}, .rotation = v.rotation,
                        .scale = {v.size, v.size}, .tint = v.color, .layer = 2});
    }
}

void ParticleBench::offscreen() const {
    frozen_.draw(nnh::Screen::Top, *sheet_, {.layer = 2, .ignore_camera = false});
}

void ParticleBench::frame(int n) {
    live_.update();
    live_.draw(nnh::Screen::Top, *sheet_, {.layer = 2});
    for (int i = 0; i < kSprites; ++i) {
        const float x = static_cast<float>((i * 37 + n) % 384), y = static_cast<float>((i * 53) % 224);
        nnh::gfx::draw(nnh::Screen::Top, *sheet_, {x, y}, {.region = {0, 32, 16, 16}, .layer = 3});
    }
}
