// The particles' counter-based generator (spec 024 FR-15; T1.5, T2.2): values in [0, 1) uniform within fixed bounds,
// properties independent, integer ranges hitting both ends; particles independent of capacity (drops), of other
// emitters and of draw order; restart() replays them.

#include "particles/particle_random.hpp"
#include "particles_fixture.hpp"

using namespace nnh::detail;
using nnh::ParticleEmitter;

NNH_TEST(particle_unit_floats_are_in_0_1_and_uniform) {
    int bins[16] = {};
    for (std::uint32_t n = 0; n < 10000; ++n) {
        const float u = unit_float(particle_hash(particle_base(7, n), kPropSpeed));
        CHECK(u >= 0 && u < 1);
        ++bins[static_cast<int>(u * 16)];
    }
    for (int b : bins) CHECK(b > 625 - 120 && b < 625 + 120); // 10 000 / 16 = 625, about ±5 sigma
}

NNH_TEST(particle_unit_float_reaches_its_ends_exactly) {
    CHECK(unit_float(0) == 0.0f);
    CHECK(unit_float(0xFFFFFFFFu) == 1.0f - 1.0f / 16777216.0f);
}

NNH_TEST(particle_properties_are_independent) {
    // The same base, two properties: a 4x4 joint histogram stays flat.
    int joint[4][4] = {};
    for (std::uint32_t n = 0; n < 16000; ++n) {
        const std::uint32_t base = particle_base(3, n);
        const int a = static_cast<int>(unit_float(particle_hash(base, kPropSpeed)) * 4);
        const int b = static_cast<int>(unit_float(particle_hash(base, kPropDirection)) * 4);
        ++joint[a][b];
    }
    for (auto& row : joint) {
        for (int c : row) CHECK(c > 1000 - 160 && c < 1000 + 160);
    }
}

NNH_TEST(particle_integer_ranges_hit_both_ends) {
    bool seen[5] = {};
    for (std::uint32_t n = 0; n < 1000; ++n) {
        const std::uint32_t v = hash_below(particle_hash(particle_base(1, n), kPropLifetime), 5);
        CHECK(v < 5);
        seen[v] = true;
    }
    for (bool s : seen) CHECK(s);
    CHECK(hash_below(0, 7) == 0 && hash_below(0xFFFFFFFFu, 7) == 6);
}

NNH_TEST(particle_seeds_give_different_values) {
    CHECK(particle_base(1, 0) != particle_base(2, 0));
    CHECK(particle_base(1, 0) != particle_base(1, 1));
    CHECK(burst_hash(1, 0, 0) != burst_hash(1, 1, 0) && burst_hash(1, 0, 0) != burst_hash(1, 0, 1));
}

namespace {

nnh::ParticleDesc random_desc() {
    nnh::ParticleDesc d = particles_test::moving_desc();
    d.shape = nnh::ParticleShape::Circle;
    d.radius = 20;
    d.spread = nnh::kFullTurn;
    d.rotation = {0, 3};
    d.spin = {-1, 1};
    d.color_min = nnh::rgba(10, 20, 30, 40);
    d.color_max = nnh::rgba(200, 210, 220, 230);
    d.size = {1, 4};
    return d;
}

} // namespace

NNH_TEST(particle_drops_never_shift_later_values) {
    // A small emitter drops some particles; the ones it keeps are the same as a large emitter's with those numbers.
    ParticleEmitter small = particles_test::make(random_desc(), 4);
    ParticleEmitter large = particles_test::make(random_desc(), 64);
    CHECK(small.emit(10) == 4 && large.emit(10) == 10);
    CHECK(small.stats().dropped == 6);
    for (int i = 0; i < 40; ++i) {
        small.update();
        large.update();
    }
    CHECK(small.alive() == 0 && large.alive() == 0);
    CHECK(small.emit(3) == 3 && large.emit(3) == 3); // numbers 10..12 on both
    for (int i = 0; i < 3; ++i) {
        const nnh::ParticleView a = small.particle(i), b = large.particle(i);
        CHECK(particles_test::bits(a.position.x) == particles_test::bits(b.position.x));
        CHECK(particles_test::bits(a.velocity.y) == particles_test::bits(b.velocity.y));
        CHECK(a.color.r == b.color.r && a.lifetime == b.lifetime);
    }
}

NNH_TEST(particle_emitters_never_share_randomness) {
    ParticleEmitter a = particles_test::make(random_desc());
    ParticleEmitter alone = particles_test::make(random_desc());
    ParticleEmitter other = particles_test::make(random_desc());
    for (int f = 0; f < 50; ++f) {
        other.emit(f % 7); // another emitter's calls in between
        a.emit(3);
        alone.emit(3);
        other.update();
        a.update();
        alone.update();
    }
    CHECK(particles_test::same_particles(a, alone));
}

NNH_TEST(particle_restart_replays_the_random_sequence) {
    nnh::ParticleDesc d = random_desc();
    d.rate = 90;
    ParticleEmitter e = particles_test::make(d);
    std::uint32_t first[30];
    for (int f = 0; f < 30; ++f) {
        e.update();
        first[f] = particles_test::checksum(e);
    }
    e.restart();
    for (int f = 0; f < 30; ++f) {
        e.update();
        CHECK(particles_test::checksum(e) == first[f]);
    }
}

NNH_TEST(particle_seed_changes_the_particles) {
    ParticleEmitter a = particles_test::make(random_desc());
    ParticleEmitter b = particles_test::make(random_desc());
    b.set_seed(99);
    a.emit(5);
    b.emit(5);
    CHECK(!particles_test::same_particles(a, b));
}
