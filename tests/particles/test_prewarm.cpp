// Prewarm (spec 024 FR-7; T3.4): `prewarm` frames at creation and at each restart() equal the same updates run by
// hand; 600 accepted, 601 refused.

#include "particles_fixture.hpp"

using nnh::ParticleDesc;
using nnh::ParticleEmitter;

namespace {

ParticleDesc smoke() {
    ParticleDesc d = particles_test::moving_desc();
    d.rate = 90;
    d.damping = 2;
    d.bursts[0] = {.at = nnh::Frames{10}, .count = {3, 6}, .cycles = 0, .interval = nnh::Frames{25}};
    return d;
}

} // namespace

NNH_TEST(prewarm_equals_the_same_updates_by_hand) {
    ParticleDesc warm = smoke();
    warm.prewarm = 120;
    ParticleEmitter a = particles_test::make(warm, 512);
    ParticleEmitter b = particles_test::make(smoke(), 512);
    for (int i = 0; i < 120; ++i) b.update();
    CHECK(a.alive() > 0 && particles_test::same_particles(a, b));
    a.update();
    b.update();
    CHECK(particles_test::same_particles(a, b));
}

NNH_TEST(restart_prewarms_again) {
    ParticleDesc warm = smoke();
    warm.prewarm = 50;
    ParticleEmitter a = particles_test::make(warm, 512);
    const std::uint32_t first = particles_test::checksum(a);
    for (int i = 0; i < 33; ++i) a.update();
    a.restart();
    CHECK(particles_test::checksum(a) == first);
}

NNH_TEST(prewarm_is_at_most_600_frames) {
    ParticleDesc d = smoke();
    d.prewarm = 600;
    CHECK(ParticleEmitter::create(d, 512).ok());
    d.prewarm = 601;
    CHECK(!ParticleEmitter::create(d, 512).ok());
}
