// Lifecycle (spec 024 FR-6, FR-7; T3.4): stop lets particles finish, start re-arms, clear, restart reproduces the first
// run bit for bit, emit/emit_at on a stopped emitter, finished().

#include "particles_fixture.hpp"

using nnh::Frames;
using nnh::ParticleDesc;
using nnh::ParticleEmitter;

NNH_TEST(stop_lets_live_particles_finish) {
    ParticleDesc d;
    d.rate = 60;
    d.lifetime = {Frames{10}, Frames{10}};
    ParticleEmitter e = particles_test::make(d, 64);
    for (int i = 0; i < 20; ++i) e.update();
    CHECK(e.alive() == 10 && e.emitting());
    e.stop();
    CHECK(!e.emitting() && !e.finished());
    for (int i = 0; i < 9; ++i) e.update();
    CHECK(e.alive() == 1);
    e.update();
    CHECK(e.alive() == 0 && e.finished());
    e.start();
    CHECK(e.emitting() && !e.finished());
    e.update();
    CHECK(e.alive() == 1);
}

NNH_TEST(clear_removes_every_particle_at_once) {
    ParticleEmitter e = particles_test::make({}, 64);
    e.emit(20);
    e.clear();
    CHECK(e.alive() == 0 && e.stats().emitted == 20);
    const nnh::Rect r = e.bounds();
    CHECK(r.w == 0 && r.h == 0);
}

NNH_TEST(stopped_emitter_still_emits_on_demand) {
    ParticleEmitter e = particles_test::make({}, 64);
    e.stop();
    CHECK(e.emit(3) == 3);
    CHECK(e.emit_at(nnh::Vec2{4, 5}, 2) == 2 && e.particle(4).position == (nnh::Vec2{4, 5}));
    CHECK(e.transform().position() == (nnh::Vec2{0, 0}));
}

NNH_TEST(restart_reproduces_the_first_run_bit_for_bit) {
    ParticleDesc d = particles_test::moving_desc();
    d.rate = 120;
    d.shape = nnh::ParticleShape::Circle;
    d.radius = 8;
    d.bursts[0] = {.at = Frames{3}, .count = {4, 9}, .cycles = 3, .interval = Frames{5}};
    ParticleEmitter e = particles_test::make(d, 512);
    std::uint32_t sums[60];
    for (int f = 0; f < 60; ++f) {
        e.set_position({static_cast<float>(f * 3), 0});
        if (f % 11 == 0) e.emit_at(nnh::Vec2{1, 1}, 2);
        e.update();
        sums[f] = particles_test::checksum(e);
    }
    e.set_position({0, 0}, true);
    e.restart();
    for (int f = 0; f < 60; ++f) {
        e.set_position({static_cast<float>(f * 3), 0});
        if (f % 11 == 0) e.emit_at(nnh::Vec2{1, 1}, 2);
        e.update();
        CHECK(particles_test::checksum(e) == sums[f]);
    }
}

NNH_TEST(emitter_with_only_on_demand_emission_is_finished_when_empty) {
    ParticleEmitter e = particles_test::make({}, 8);
    CHECK(e.emitting() && e.finished()); // no rate, no burst: nothing left to emit by itself
    e.emit(1);
    CHECK(!e.finished());
}
