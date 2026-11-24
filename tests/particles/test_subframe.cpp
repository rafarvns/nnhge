// Sub-frame spreading (spec 024 FR-10, FR-14; T2.4): rate births at their frame fraction along the emitter's movement
// and turn, pre-aged; a jump breaks it; local space and bursts are not spread.

#include <cmath>

#include "particles_fixture.hpp"

using nnh::ParticleDesc;
using nnh::ParticleEmitter;

namespace {

ParticleDesc ten_per_frame() {
    ParticleDesc d;
    d.rate = 600;
    d.lifetime = {nnh::Frames{100}, nnh::Frames{100}};
    return d;
}

} // namespace

NNH_TEST(fast_emitter_births_are_evenly_spaced_along_its_movement) {
    ParticleEmitter e = particles_test::make(ten_per_frame(), 64);
    e.set_position({100, 0});
    e.update();
    CHECK(e.alive() == 10);
    for (int j = 0; j < 10; ++j) {
        const nnh::ParticleView p = e.particle(j);
        CHECK(std::fabs(p.position.x - 10.0f * static_cast<float>(j + 1)) < 1e-3f);
        CHECK(std::abs(p.age.ticks - static_cast<int>(std::lround((9 - j) * 409.6))) <= 1); // pre-aged by 1 − f
    }
    CHECK(e.particle(9).position.x == 100 && e.particle(9).age.ticks == 0); // f = 1: the current place exactly
}

NNH_TEST(pre_aged_births_moved_by_their_part_of_the_frame) {
    ParticleDesc d = ten_per_frame();
    d.speed = {600, 600}; // 10 px per frame along +x
    ParticleEmitter e = particles_test::make(d, 64);
    e.update();
    for (int j = 0; j < 10; ++j) {
        CHECK(std::fabs(e.particle(j).position.x - 10.0f * (1.0f - 0.1f * static_cast<float>(j + 1))) < 1e-3f);
    }
}

NNH_TEST(turning_emitter_births_along_the_turn) {
    ParticleDesc d = ten_per_frame();
    d.speed = {60, 60};
    ParticleEmitter e = particles_test::make(d, 64);
    e.set_transform(nnh::Transform2D{.rotation = 1.0f});
    e.update();
    for (int j = 0; j < 10; ++j) {
        const nnh::ParticleView p = e.particle(j);
        CHECK(std::fabs(std::atan2(p.velocity.y, p.velocity.x) - 0.1f * static_cast<float>(j + 1)) < 1e-4f);
    }
}

NNH_TEST(jump_breaks_the_spreading) {
    ParticleEmitter e = particles_test::make(ten_per_frame(), 64);
    e.set_position({100, 0}, true);
    e.update();
    for (int j = 0; j < 10; ++j) CHECK(e.particle(j).position.x == 100);
}

NNH_TEST(local_space_births_are_not_spread) {
    ParticleDesc d = ten_per_frame();
    d.space = nnh::ParticleSpace::Local;
    ParticleEmitter e = particles_test::make(d, 64);
    e.set_position({100, 0});
    e.update();
    for (int j = 0; j < 10; ++j) CHECK(e.particle(j).position.x == 0 && e.particle(j).age.ticks == 0);
}

NNH_TEST(bursts_are_not_spread) {
    ParticleDesc d;
    d.bursts[0] = {.count = {6, 6}};
    ParticleEmitter e = particles_test::make(d, 64);
    e.set_position({50, 20});
    e.update();
    for (int j = 0; j < 6; ++j) CHECK(e.particle(j).position == (nnh::Vec2{50, 20}) && e.particle(j).age.ticks == 0);
}
