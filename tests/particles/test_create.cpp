// Emitter creation (spec 024 FR-1, FR-24; T1.4): capacities, one block of tagged memory of header + 48 bytes per
// particle, OutOfMemory, a game tag, moves, empty emitters.

#include "nnh/memory.hpp"
#include "particles/particle_block.hpp"
#include "particles_fixture.hpp"

using nnh::ParticleDesc;
using nnh::ParticleEmitter;

NNH_MEM_TAG(particle_test_tag);

NNH_TEST(particle_records_are_32_and_16_bytes) {
    CHECK(sizeof(nnh::detail::ParticleHot) == 32);
    CHECK(sizeof(nnh::detail::ParticleCold) == 16);
    CHECK(nnh::detail::particle_block_bytes(1024) - nnh::detail::particle_block_bytes(0) == 48u * 1024u);
    CHECK(nnh::detail::kParticleHeaderBytes < 2600);
}

NNH_TEST(particle_capacity_is_1_to_8192) {
    CHECK(ParticleEmitter::create({}, 1).ok());
    CHECK(ParticleEmitter::create({}, 8192).ok());
    const auto zero = ParticleEmitter::create({}, 0);
    CHECK(!zero.ok() && zero.error().code == nnh::ErrorCode::InvalidArgument);
    CHECK(!ParticleEmitter::create({}, 8193).ok());
    CHECK(!ParticleEmitter::create({}, -5).ok());
}

NNH_TEST(particle_emitter_is_one_block_of_its_tag) {
    const nnh::MemStats before = nnh::mem::tag_stats(nnh::detail::particle_memory_tag());
    {
        ParticleEmitter e = particles_test::make({}, 100);
        const nnh::MemStats now = nnh::mem::tag_stats(nnh::detail::particle_memory_tag());
        CHECK(now.count == before.count + 1);
        CHECK(now.bytes == before.bytes + nnh::detail::particle_block_bytes(100));
        CHECK(e.capacity() == 100 && e.valid());
    }
    CHECK(nnh::mem::tag_stats(nnh::detail::particle_memory_tag()).bytes == before.bytes);
}

NNH_TEST(particle_emitter_uses_the_game_tag) {
    ParticleEmitter e = particles_test::take(ParticleEmitter::create({}, 10, &NNH_MEM_TAG_REF(particle_test_tag)));
    CHECK(nnh::mem::tag_stats(NNH_MEM_TAG_REF(particle_test_tag)).bytes == nnh::detail::particle_block_bytes(10));
}

NNH_TEST(particle_emitter_without_memory_is_out_of_memory) {
    nnh::platform_fake::reset();
    nnh::platform_fake::set_region_capacity(nnh::MemRegion::Heap, 64 * 1024);
    const auto r = ParticleEmitter::create({}, 8192);
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::OutOfMemory);
    nnh::platform_fake::reset();
}

NNH_TEST(particle_emitter_move_leaves_the_source_empty) {
    ParticleEmitter a = particles_test::make({}, 8);
    a.emit(3);
    ParticleEmitter b(static_cast<ParticleEmitter&&>(a));
    CHECK(!a.valid() && b.valid() && b.alive() == 3);
    ParticleEmitter c = particles_test::make({}, 4);
    c = static_cast<ParticleEmitter&&>(b);
    CHECK(!b.valid() && c.capacity() == 8 && c.alive() == 3);
}

NNH_TEST(empty_particle_emitter_answers_zero) {
    const ParticleEmitter e;
    CHECK(!e.valid() && e.alive() == 0 && e.capacity() == 0 && !e.emitting() && !e.finished());
    const nnh::ParticleStats s = e.stats();
    CHECK(s.live == 0 && s.capacity == 0 && s.emitted == 0);
    const nnh::Rect r = e.bounds();
    CHECK(r.x == 0 && r.y == 0 && r.w == 0 && r.h == 0);
    CHECK(e.desc().rate == 0 && e.desc().seed == 1);
    CHECK(e.transform().position().x == 0);
}

// Regression (spec 024 stage 9, Linux): with `= {}` on ParticleDesc's arrays, GCC 13 and 14 refused every braced
// description argument ("cannot convert '<brace-enclosed initializer list>' to 'const ParticleDesc&'").
template <typename D>
concept CreatesFromBracedDesc = requires(const D& d) {
    ParticleEmitter::create({}, 1);
    ParticleEmitter::create({.rate = 10, .bursts = {{.count = {1, 2}}}, .frames = {{.region = {0, 0, 4, 4}}}}, 1);
    ParticleEmitter::check({.color_keys = {{.at = 0}, {.at = 1}}});
};
static_assert(CreatesFromBracedDesc<ParticleDesc>, "ParticleEmitter::create must accept a braced desc everywhere");

NNH_TEST(braced_desc_leaves_unnamed_array_elements_at_their_defaults) {
    const ParticleDesc braced{.rate = 10, .bursts = {{.count = {1, 2}}}};
    ParticleDesc plain; // default-initialised: each element's own member defaults
    CHECK(braced.bursts[0].count.max == 2 && braced.bursts[1].count.max == 0 && braced.bursts[1].cycles == 1);
    CHECK(braced.color_keys[0].at < 0 && braced.size_keys[7].size == 1 && braced.frames[3].region.w == -1);
    CHECK(plain.bursts[2].cycles == 1 && plain.color_keys[5].at < 0 && plain.frames[0].center_pivot);
}

NNH_TEST(many_particle_emitters_live_together) {
    ParticleEmitter e[16];
    for (int i = 0; i < 16; ++i) {
        e[i] = particles_test::make({}, 4 + i);
        e[i].emit(i % 5);
    }
    for (int i = 0; i < 16; ++i) CHECK(e[i].alive() == i % 5 && e[i].capacity() == 4 + i);
}
