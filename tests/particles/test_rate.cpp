// Rate emission (spec 024 FR-4; T2.3): the 16.16 accumulator is exact over time for fractional and extreme rates; the
// delay, the duration and looping bound it.

#include "particles_fixture.hpp"

using nnh::ParticleDesc;
using nnh::ParticleEmitter;

namespace {

/// Particles created by `frames` updates of a long-lived emitter at `rate`.
std::uint32_t made_in(float rate, int frames, ParticleDesc d = {}) {
    d.rate = rate;
    d.lifetime = {nnh::kMaxFrameTime, nnh::kMaxFrameTime};
    ParticleEmitter e = particles_test::make(d, 8192);
    for (int i = 0; i < frames; ++i) e.update();
    return e.stats().emitted;
}

} // namespace

NNH_TEST(fractional_rates_are_exact_over_time) {
    CHECK(made_in(2.5f, 600) == 25);
    CHECK(made_in(2.5f, 3600) == 150);
    CHECK(made_in(0.5f, 600) == 5);
    CHECK(made_in(0.5f, 119) == 0);
    CHECK(made_in(0.5f, 120) == 1);
    CHECK(made_in(59.9f, 3600) == 3594);
    CHECK(made_in(60, 600) == 600);
}

NNH_TEST(the_largest_rate_makes_8192_particles_a_frame) {
    ParticleDesc d;
    d.rate = 491520;
    d.lifetime = {nnh::FrameTime::from_ticks(1), nnh::FrameTime::from_ticks(1)};
    ParticleEmitter e = particles_test::make(d, 8192);
    e.update();
    CHECK(e.alive() == 8192 && e.stats().dropped == 0);
    e.update(); // the previous ones die first, then 8192 more
    CHECK(e.alive() == 8192 && e.stats().emitted == 16384 && e.stats().dropped == 0);
}

NNH_TEST(rate_waits_for_the_delay) {
    ParticleDesc d;
    d.delay = nnh::Frames{10};
    CHECK(made_in(60, 10, d) == 0);
    CHECK(made_in(60, 11, d) == 1);
    CHECK(made_in(60, 30, d) == 20);
}

NNH_TEST(rate_stops_at_the_end_of_the_duration) {
    ParticleDesc d;
    d.rate = 60;
    d.duration = nnh::Frames{20};
    d.lifetime = {nnh::Frames{5}, nnh::Frames{5}};
    ParticleEmitter e = particles_test::make(d, 64);
    for (int i = 0; i < 19; ++i) e.update();
    CHECK(e.emitting());
    e.update();
    CHECK(!e.emitting() && e.stats().emitted == 20 && !e.finished());
    for (int i = 0; i < 10; ++i) e.update();
    CHECK(e.stats().emitted == 20 && e.alive() == 0 && e.finished());
}

NNH_TEST(looping_rate_keeps_going) {
    ParticleDesc d;
    d.duration = nnh::Frames{20};
    d.looping = true;
    CHECK(made_in(60, 100, d) == 100);
    d.delay = nnh::Frames{5}; // the delay is waited once
    CHECK(made_in(60, 100, d) == 95);
}
