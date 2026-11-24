// Bursts (spec 024 FR-5, FR-7; T2.3): times, cycles (1, 4, for ever), intervals, count ranges, eight bursts, a
// one-shot that finishes by itself.

#include "particles_fixture.hpp"

using nnh::Frames;
using nnh::ParticleDesc;
using nnh::ParticleEmitter;

namespace {

ParticleDesc long_lived() {
    ParticleDesc d;
    d.lifetime = {Frames{1000}, Frames{1000}};
    return d;
}

} // namespace

NNH_TEST(burst_fires_at_its_time) {
    ParticleDesc d = long_lived();
    d.bursts[0] = {.at = Frames{5}, .count = {4, 4}};
    ParticleEmitter e = particles_test::make(d, 64);
    for (int i = 0; i < 5; ++i) e.update();
    CHECK(e.alive() == 0);
    e.update(); // the frame [5, 6)
    CHECK(e.alive() == 4);
    for (int i = 0; i < 20; ++i) e.update();
    CHECK(e.alive() == 4); // once
}

NNH_TEST(burst_cycles_fire_every_interval) {
    ParticleDesc d = long_lived();
    d.bursts[0] = {.at = Frames{0}, .count = {2, 2}, .cycles = 4, .interval = Frames{10}};
    ParticleEmitter e = particles_test::make(d, 64);
    int counts[45];
    for (int f = 0; f < 45; ++f) {
        e.update();
        counts[f] = e.alive();
    }
    CHECK(counts[0] == 2 && counts[9] == 2 && counts[10] == 4 && counts[20] == 6 && counts[30] == 8 && counts[44] == 8);
}

NNH_TEST(endless_burst_keeps_firing) {
    ParticleDesc d;
    d.lifetime = {Frames{3}, Frames{3}};
    d.bursts[0] = {.at = Frames{0}, .count = {1, 1}, .cycles = 0, .interval = Frames{2}};
    ParticleEmitter e = particles_test::make(d, 64);
    for (int f = 0; f < 1000; ++f) e.update();
    CHECK(e.stats().emitted == 500 && e.emitting() && !e.finished());
}

NNH_TEST(burst_counts_stay_in_their_range_and_vary) {
    ParticleDesc d;
    d.lifetime = {Frames{1}, Frames{1}};
    d.bursts[0] = {.at = Frames{0}, .count = {8, 16}, .cycles = 0, .interval = Frames{1}};
    ParticleEmitter e = particles_test::make(d, 64);
    bool seen[17] = {};
    for (int f = 0; f < 400; ++f) {
        e.update();
        CHECK(e.alive() >= 8 && e.alive() <= 16);
        seen[e.alive()] = true;
    }
    for (int c = 8; c <= 16; ++c) CHECK(seen[c]);
}

NNH_TEST(eight_bursts_all_fire) {
    ParticleDesc d = long_lived();
    for (int i = 0; i < nnh::kMaxParticleBursts; ++i) d.bursts[i] = {.at = Frames{i}, .count = {1, 1}};
    ParticleEmitter e = particles_test::make(d, 64);
    for (int f = 0; f < 10; ++f) e.update();
    CHECK(e.alive() == 8);
}

NNH_TEST(one_shot_finishes_by_itself) {
    ParticleDesc d;
    d.lifetime = {Frames{10}, Frames{10}};
    d.bursts[0] = {.count = {12, 12}};
    ParticleEmitter e = particles_test::make(d, 64);
    CHECK(!e.finished());
    e.update();
    CHECK(e.alive() == 12 && !e.finished());
    for (int f = 0; f < 9; ++f) e.update();
    CHECK(e.alive() == 12 && !e.finished()); // born at the end of update 1: ten updates of life
    e.update();
    CHECK(e.alive() == 0 && e.finished());
}

NNH_TEST(bursts_past_the_duration_never_fire_and_looping_rearms_them) {
    ParticleDesc d = long_lived();
    d.duration = Frames{10};
    d.bursts[0] = {.at = Frames{2}, .count = {1, 1}};
    d.bursts[1] = {.at = Frames{12}, .count = {5, 5}};
    ParticleEmitter once = particles_test::make(d, 64);
    for (int f = 0; f < 30; ++f) once.update();
    CHECK(once.alive() == 1);
    d.looping = true;
    ParticleEmitter loop = particles_test::make(d, 64);
    for (int f = 0; f < 30; ++f) loop.update();
    CHECK(loop.alive() == 3); // frames 2, 12, 22
}
