// The particles self-check (see self_check.hpp; plan 024 "Self-check"): every call from a fixed table or an FNV-1a of
// (seed, frame, salt). Built with -ffp-contract=off (examples/particles/CMakeLists.txt, tests/CMakeLists.txt): its few
// float operations give the same bits everywhere, like the engine's.

#include "self_check.hpp"

#include <bit>

namespace particles_check {
namespace {

using nnh::Frames;
using nnh::ParticleDesc;

constexpr int kCapacity[kEmitters] = {512, 256, 256, 256, 256, 512, 256, 256};

std::uint32_t fnv(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    std::uint32_t h = 2166136261u;
    for (std::uint32_t w : {a, b, c}) {
        for (int i = 0; i < 4; ++i) {
            h ^= (w >> (8 * i)) & 0xFFu;
            h *= 16777619u;
        }
    }
    return h;
}

void frames_of(ParticleDesc& d, int n) {
    for (int i = 0; i < n; ++i) d.frames[i].region = {8 * i, 0, 8, 8};
}

ParticleDesc e1() { // point, rate, a cone up, gravity, damping, a moving and turning emitter, inherit, curves
    ParticleDesc d;
    d.rate = 120;
    d.angle = -nnh::kQuarterTurn;
    d.spread = 1.0471976f;
    d.speed = {40, 90};
    d.gravity = {0, 60};
    d.damping = 1.5f;
    d.inherit_velocity = 0.5f;
    d.lifetime = {Frames{40}, Frames{80}};
    d.color_keys[0] = {0, nnh::rgba(255, 255, 160, 255)};
    d.color_keys[1] = {0.3f, nnh::rgba(255, 200, 40, 255)};
    d.color_keys[2] = {0.7f, nnh::rgba(220, 60, 20, 200)};
    d.color_keys[3] = {1, nnh::rgba(80, 20, 20, 0)};
    d.size_keys[0] = {0, 0.5f};
    d.size_keys[1] = {0.2f, 1.5f};
    d.size_keys[2] = {1, 0.25f};
    frames_of(d, 4);
    d.frame_mode = nnh::ParticleFrameMode::OverLife;
    d.frame_repeats = 2;
    d.seed = 101;
    return d;
}

ParticleDesc e2() { // box area, a high rate, kept full
    ParticleDesc d;
    d.rate = 600;
    d.shape = nnh::ParticleShape::Box;
    d.box = {120, 30};
    d.speed = {0, 30};
    d.spread = nnh::kFullTurn;
    d.lifetime = {Frames{30}, Frames{60}};
    d.color_min = nnh::rgba(100, 100, 255, 255);
    d.color_max = nnh::rgba(200, 220, 255, 255);
    d.seed = 202;
    return d;
}

ParticleDesc e3() { // box edge, three bursts, a looping duration
    ParticleDesc d;
    d.shape = nnh::ParticleShape::Box;
    d.box = {50, 40};
    d.edge = true;
    d.direction = nnh::ParticleDirection::Outward;
    d.speed = {20, 50};
    d.duration = Frames{120};
    d.looping = true;
    for (int i = 0; i < 3; ++i) {
        d.bursts[i] = {.at = Frames{5 + 17 * i}, .count = {8, 16}, .cycles = 4, .interval = Frames{30}};
    }
    d.lifetime = {Frames{20}, Frames{50}};
    d.seed = 303;
    return d;
}

ParticleDesc e4() { // a ring on an arc, outward, one burst
    ParticleDesc d;
    d.shape = nnh::ParticleShape::Circle;
    d.radius = 30;
    d.inner_radius = 20;
    d.arc_start = -2.0f;
    d.arc = 2.5f;
    d.direction = nnh::ParticleDirection::Outward;
    d.speed = {60, 140};
    d.damping = 4;
    d.bursts[0] = {.at = Frames{2}, .count = {40, 60}};
    d.lifetime = {Frames{30}, Frames{70}};
    d.size = {1, 3};
    d.seed = 404;
    return d;
}

ParticleDesc e5() { // a line in local space, spin, frames at a rate
    ParticleDesc d;
    d.rate = 45;
    d.shape = nnh::ParticleShape::Line;
    d.length = 40;
    d.space = nnh::ParticleSpace::Local;
    d.angle = 0.3f;
    d.spread = 0.6f;
    d.speed = {10, 30};
    d.rotation = {0, 6.28f};
    d.spin = {-4, 4};
    d.lifetime = {Frames{30}, Frames{90}};
    frames_of(d, 3);
    d.frame_mode = nnh::ParticleFrameMode::Rate;
    d.frame_rate = 12;
    d.seed = 505;
    return d;
}

ParticleDesc e7a() { // prewarmed, local space with world gravity
    ParticleDesc d;
    d.rate = 30;
    d.space = nnh::ParticleSpace::Local;
    d.world_gravity = true;
    d.gravity = {0, 90};
    d.speed = {20, 40};
    d.spread = 2;
    d.angle = -nnh::kQuarterTurn;
    d.lifetime = {Frames{60}, Frames{100}};
    d.prewarm = 120;
    d.order = nnh::ParticleOrder::NewestFirst;
    d.seed = 707;
    return d;
}

ParticleDesc e7b() { // its replacement at frame 1800
    ParticleDesc d = e7a();
    d.rate = 75;
    d.gravity = {30, -40};
    d.size = {2, 5};
    d.color_keys[0] = {0.5f, nnh::rgba(10, 200, 90, 255)};
    d.seed = 708;
    return d;
}

ParticleDesc e8() { // a delay and a duration without a loop: it finishes
    ParticleDesc d;
    d.rate = 50;
    d.delay = Frames{60};
    d.duration = Frames{600};
    d.speed = {5, 25};
    d.spread = nnh::kFullTurn;
    d.lifetime = {Frames{20}, Frames{40}};
    d.inherit_velocity = 1;
    d.seed = 808;
    return d;
}

nnh::Transform2D circle_path(int f) {
    const nnh::SinCos sc = nnh::sin_cos(static_cast<float>(f) * 0.02f);
    return {.position = {200.0f + 80.0f * sc.cos, 120.0f + 60.0f * sc.sin},
            .rotation = static_cast<float>(f) * (nnh::kQuarterTurn / 64.0f)};
}

} // namespace

ParticleDesc sparks_desc() {
    ParticleDesc d;
    d.shape = nnh::ParticleShape::Circle;
    d.radius = 6;
    d.speed = {50, 110};
    d.spread = nnh::kFullTurn;
    d.gravity = {0, 150};
    d.lifetime = {Frames{20}, Frames{35}};
    d.color_min = nnh::rgba(255, 220, 80, 255);
    d.color_max = nnh::rgba(255, 255, 200, 255);
    d.color_keys[0] = {0.6f, nnh::colors::white};
    d.color_keys[1] = {1, nnh::rgba(255, 120, 0, 0)};
    d.size = {1, 2};
    frames_of(d, 4);
    d.frame_mode = nnh::ParticleFrameMode::Random;
    d.seed = 606;
    return d;
}

bool SelfCheck::begin(const ParticleDesc* e6) {
    const ParticleDesc descs[kEmitters] = {e1(), e2(), e3(), e4(), e5(), e6 != nullptr ? *e6 : sparks_desc(), e7a(), e8()};
    for (int i = 0; i < kEmitters; ++i) {
        auto r = nnh::ParticleEmitter::create(descs[i], kCapacity[i]);
        if (!r) return false;
        emitters_[i] = static_cast<nnh::ParticleEmitter&&>(r.value());
    }
    emitters_[1].set_position({200, 40});
    emitters_[2].set_position({90, 150});
    emitters_[3].set_position({300, 160});
    emitters_[6].set_transform(nnh::Transform2D{.position = {60, 60}, .rotation = 0.7f});
    frame_ = 0;
    sum_ = 0;
    return true;
}

void SelfCheck::script(int f) {
    nnh::ParticleEmitter* e = emitters_;
    e[0].set_transform(circle_path(f));
    if (f % 300 == 299) e[3].restart();
    const nnh::Transform2D line{.position = {320, 60}, .rotation = static_cast<float>(f) * 0.01f,
                                .scale = {(f / 400) % 2 == 0 ? 1.0f : -1.0f, 1.0f}};
    e[4].set_transform(line);
    if (f % 7 == 3) {
        const std::uint32_t h = fnv(606, static_cast<std::uint32_t>(f), 1);
        const nnh::Vec2 at{static_cast<float>(h % 400), static_cast<float>((h >> 9) % 240)};
        const int n = 1 + static_cast<int>((h >> 17) % 20);
        if ((h >> 23) % 3 == 0) {
            e[5].emit_at(nnh::to_world({.position = at, .rotation = static_cast<float>(h % 628) * 0.01f}), n);
        } else {
            e[5].emit_at(at, n);
        }
    }
    if (f % 250 == 125) e[5].stop();
    if (f % 250 == 0) e[5].start();
    if (f == 1800) e[6].set_desc(e7b());
    e[6].set_position({60.0f + static_cast<float>(f % 100), 60});
    if (f == 2400) e[7].restart();
    e[7].set_position({static_cast<float>((f * 3) % 400), 200}, f % 500 == 0);
}

bool SelfCheck::step() {
    script(frame_);
    for (nnh::ParticleEmitter& e : emitters_) e.update();
    ++frame_;
    if (frame_ % kCheckpointEvery != 0) return false;
    sum_ = hash_now();
    return true;
}

std::uint32_t SelfCheck::hash_now() const {
    std::uint32_t h = 2166136261u;
    auto word = [&h](std::uint32_t w) {
        for (int i = 0; i < 4; ++i) {
            h ^= (w >> (8 * i)) & 0xFFu;
            h *= 16777619u;
        }
    };
    auto f = [&word](float v) { word(std::bit_cast<std::uint32_t>(v)); };
    for (const nnh::ParticleEmitter& e : emitters_) {
        const nnh::ParticleStats s = e.stats();
        word(static_cast<std::uint32_t>(s.live));
        word(s.emitted);
        word(s.dropped);
        word(e.finished() ? 1u : 0u);
        for (int i = 0; i < e.alive(); ++i) {
            const nnh::ParticleView v = e.particle(i);
            f(v.position.x);
            f(v.position.y);
            f(v.velocity.x);
            f(v.velocity.y);
            f(v.rotation);
            f(v.size);
            word(static_cast<std::uint32_t>(v.color.r) | (static_cast<std::uint32_t>(v.color.g) << 8) |
                 (static_cast<std::uint32_t>(v.color.b) << 16) | (static_cast<std::uint32_t>(v.color.a) << 24));
            word(static_cast<std::uint32_t>(v.frame));
            word(static_cast<std::uint32_t>(v.age.ticks));
            word(static_cast<std::uint32_t>(v.lifetime.ticks));
        }
    }
    return h;
}

int SelfCheck::total_alive() const {
    int n = 0;
    for (const nnh::ParticleEmitter& e : emitters_) n += e.alive();
    return n;
}

} // namespace particles_check
