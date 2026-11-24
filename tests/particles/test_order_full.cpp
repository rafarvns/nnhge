// Emission order and the full case (spec 024 FR-18; T2.2, T3.1): as many as fit, the rest dropped and counted with no
// assert (a budget, not a bug); emission order kept through random deaths.

#include "particles_fixture.hpp"

using nnh::ParticleDesc;
using nnh::ParticleEmitter;

namespace {

ParticleEmitter* g_e = nullptr;
int g_made = 0;
void overfill() { g_made = g_e->emit(10); }

} // namespace

NNH_TEST(full_emitter_makes_what_fits_and_counts_the_rest) {
    ParticleEmitter e = particles_test::make({}, 8);
    CHECK(e.emit(5) == 5);
    g_e = &e;
    CHECK(!particles_test::crashes(overfill)); // no assert, in every flavour
    CHECK(g_made == 3 && e.alive() == 8);
    const nnh::ParticleStats s = e.stats();
    CHECK(s.dropped == 7 && s.emitted == 8 && s.peak == 8 && s.live == 8);
    CHECK(e.emit(1) == 0 && e.stats().dropped == 8);
}

NNH_TEST(full_rate_emitter_drops_and_counts) {
    ParticleDesc d;
    d.rate = 600; // 10 per frame
    d.lifetime = {nnh::Frames{100}, nnh::Frames{100}};
    ParticleEmitter e = particles_test::make(d, 25);
    for (int i = 0; i < 4; ++i) e.update();
    CHECK(e.alive() == 25 && e.stats().dropped == 15 && e.stats().emitted == 25);
}

NNH_TEST(particles_keep_their_emission_order_through_random_deaths) {
    ParticleDesc d = particles_test::moving_desc();
    d.lifetime = {nnh::Frames{1}, nnh::Frames{30}};
    d.rate = 300;
    ParticleEmitter e = particles_test::make(d, 512);
    for (int f = 0; f < 90; ++f) {
        e.update();
        // Born earlier = older (rate births of one frame are pre-aged in order: later ones younger).
        for (int i = 1; i < e.alive(); ++i) CHECK(e.particle(i - 1).age.ticks >= e.particle(i).age.ticks);
    }
    CHECK(e.alive() > 20);
}
