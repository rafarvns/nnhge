// set_desc (spec 024 FR-2, FR-25; T3.4): a new description changes later births and the curves, live particles keep
// their drawn values, a bad description asserts and changes nothing, the capacity stays.

#include "particles_fixture.hpp"

using nnh::ParticleDesc;
using nnh::ParticleEmitter;

namespace {

ParticleEmitter* g_e = nullptr;
void set_bad_desc() {
    ParticleDesc bad;
    bad.damping = 99;
    g_e->set_desc(bad);
}

} // namespace

NNH_TEST(set_desc_changes_later_births_and_keeps_live_particles) {
    ParticleDesc a;
    a.speed = {60, 60};
    a.lifetime = {nnh::Frames{100}, nnh::Frames{100}};
    ParticleEmitter e = particles_test::make(a, 16);
    e.emit(1);
    ParticleDesc b = a;
    b.speed = {120, 120};
    b.color_keys[0] = {0, nnh::rgba(10, 20, 30, 40)};
    e.set_desc(b);
    e.emit(1);
    CHECK(e.particle(0).velocity.x == 1 && e.particle(1).velocity.x == 2);
    CHECK(e.particle(0).color.r == 10); // the curve is the emitter's, for every particle
    CHECK(e.desc().speed.min == 120 && e.capacity() == 16);
}

NNH_TEST(set_desc_with_a_bad_description_changes_nothing) {
    ParticleDesc a;
    a.rate = 30;
    ParticleEmitter e = particles_test::make(a, 16);
    g_e = &e;
    CHECK(particles_test::asserts_or_is_ignored(set_bad_desc));
    CHECK(e.desc().rate == 30 && e.desc().damping == 0);
}

NNH_TEST(set_desc_arms_a_new_burst) {
    ParticleEmitter e = particles_test::make({}, 16);
    e.update();
    ParticleDesc b;
    b.bursts[0] = {.count = {3, 3}};
    e.set_desc(b);
    e.update();
    CHECK(e.alive() == 3);
}
