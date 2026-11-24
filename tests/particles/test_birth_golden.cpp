// Births bit for bit (spec 024 T10.3): every shape, both directions, rings, arcs, huge angles, world and
// local space, turned and mirrored emitters, emit / emit_at / rate births with sub-frame pre-ageing and
// inherited velocity. Each case hashes the raw hot and cold records and the bounds keys after each step; the
// goldens were recorded from the births before the T10.3 rewrite (shape-specialised, branch-free sine), so
// any change of a single bit fails here.

#include "particles/particle_block.hpp"
#include "particles_fixture.hpp"

#include <cstdio>
#include <initializer_list>

using nnh::ParticleDesc;
using nnh::ParticleEmitter;
using nnh::ParticleShape;
using nnh::Vec2;

namespace {

std::uint32_t raw_checksum(const ParticleEmitter& e, std::uint32_t h) {
    particles_test::Fnv fnv{h};
    const nnh::detail::ParticleBlock& b = *nnh::ParticleAccess::block(e);
    fnv.word(static_cast<std::uint32_t>(b.alive));
    for (int i = 0; i < b.alive; ++i) {
        const auto& hot = b.hot[i];
        const auto& cold = b.cold[i];
        for (float f : {hot.px, hot.py, hot.vx, hot.vy, hot.rotation, hot.spin, cold.inv_life, cold.size})
            fnv.f(f);
        fnv.word(hot.age);
        fnv.word(hot.life);
        fnv.word(cold.color);
        fnv.word(cold.frame);
    }
    if (b.alive > 0) {
        for (std::int32_t k : {b.key_min_x, b.key_min_y, b.key_max_x, b.key_max_y})
            fnv.word(static_cast<std::uint32_t>(k));
    }
    return fnv.h;
}

ParticleDesc base_desc() {
    ParticleDesc d;
    d.speed = {-20, 140};
    d.angle = 0.7f;
    d.spread = 2.5f;
    d.rotation = {-2, 5};
    d.spin = {-4, 3};
    d.lifetime = {nnh::Frames{10}, nnh::Frames{50}};
    d.color_min = nnh::rgba(3, 90, 200, 10);
    d.color_max = nnh::rgba(250, 91, 20, 255);
    d.size = {0.25f, 3};
    for (int i = 0; i < 5; ++i)
        d.frames[i].region = {4 * i, 0, 4, 4};
    d.frame_mode = nnh::ParticleFrameMode::Random;
    d.inherit_velocity = 0.75f;
    return d;
}

/// One case: emit through a turned and mirrored transform, emit_at a point, then rate births on a moving,
/// turning emitter (sub-frame transforms and pre-ageing in world space).
std::uint32_t run(const ParticleDesc& desc) {
    ParticleDesc d = desc;
    d.rate = 0;
    ParticleEmitter e = particles_test::make(d, 512);
    std::uint32_t h = 2166136261u;
    e.set_transform(nnh::Transform2D{.position = {120, 80}, .rotation = 0.6f, .scale = {-1.5f, 0.75f}}, true);
    e.emit(37);
    h = raw_checksum(e, h);
    e.emit_at(Vec2{-31.25f, 207.5f}, 23);
    h = raw_checksum(e, h);
    e.emit_at(nnh::WorldTransform2D{}, 5);
    h = raw_checksum(e, h);
    e.clear();
    d.rate = 437;
    e.set_desc(d);
    for (int f = 0; f < 12; ++f) {
        const float t = static_cast<float>(f);
        e.set_transform(nnh::Transform2D{.position = {50 + 7 * t, 90 - 3 * t},
                                         .rotation = f < 6 ? 0.25f * t : 1.5f,
                                         .scale = {1 + 0.125f * t, f % 4 == 3 ? -1.0f : 1.0f}});
        e.update();
        h = raw_checksum(e, h);
    }
    return h;
}

struct Case {
    const char* name;
    std::uint32_t golden;
};

} // namespace

NNH_TEST(births_of_every_shape_and_direction_keep_their_bits) {
    constexpr int kCases = 16;
    const Case cases[kCases] = {
        {"point cone", 0x86389ACEu},       {"point outward", 0x86389ACEu},
        {"box area cone", 0x23532845u},    {"box area outward", 0xBA694AE4u},
        {"box edge cone", 0xDA263F99u},    {"box edge outward", 0x208D9DDDu},
        {"circle area cone", 0xF1C63885u}, {"circle ring outward", 0x40408707u},
        {"circle arc cone", 0x87069004u},  {"circle on edge", 0x0CA3A0C8u},
        {"circle zero", 0x86389ACEu},      {"line cone", 0x1BF979A2u},
        {"line outward", 0xA5918756u},     {"line zero outward", 0x86389ACEu},
        {"huge angles", 0x717DC125u},      {"local mirrored", 0x53FC4127u},
    };
    for (int i = 0; i < kCases; ++i) {
        ParticleDesc d = base_desc();
        const bool outward = i == 1 || i == 3 || i == 5 || i == 7 || i == 9 || i == 12 || i == 13;
        d.direction = outward ? nnh::ParticleDirection::Outward : nnh::ParticleDirection::Cone;
        switch (i) {
        case 2:
        case 3:
            d.shape = ParticleShape::Box;
            d.box = {60, 25};
            break;
        case 4:
        case 5:
            d.shape = ParticleShape::Box;
            d.box = {60, 25};
            d.edge = true;
            break;
        case 6:
            d.shape = ParticleShape::Circle;
            d.radius = 30;
            d.spread = nnh::kFullTurn;
            break;
        case 7:
            d.shape = ParticleShape::Circle;
            d.radius = 30;
            d.inner_radius = 12;
            break;
        case 8:
            d.shape = ParticleShape::Circle;
            d.radius = 30;
            d.inner_radius = 5;
            d.arc_start = -2.75f;
            d.arc = 1.25f;
            break;
        case 9:
            d.shape = ParticleShape::Circle;
            d.radius = 17;
            d.inner_radius = 17;
            break;
        case 10:
            d.shape = ParticleShape::Circle;
            break;
        case 11:
        case 12:
            d.shape = ParticleShape::Line;
            d.length = 80;
            break;
        case 13:
            d.shape = ParticleShape::Line;
            break;
        case 14:
            d.shape = ParticleShape::Circle;
            d.radius = 9;
            d.arc_start = 3.0e9f; // |q| ≥ 2²³: the sine's whole-quarter path
            d.angle = -1.0e8f;
            d.spread = 0.001f;
            break;
        case 15:
            d.shape = ParticleShape::Circle;
            d.radius = 30;
            d.inner_radius = 10;
            d.space = nnh::ParticleSpace::Local;
            break;
        default:
            break;
        }
        const std::uint32_t got = run(d);
        if (got != cases[i].golden)
            std::printf("  births %-20s 0x%08Xu\n", cases[i].name, static_cast<unsigned>(got));
        CHECK(got == cases[i].golden);
    }
}
