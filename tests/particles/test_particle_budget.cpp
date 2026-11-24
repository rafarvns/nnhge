// The particle budget (spec 024 FR-20; T4.1, T4.3): no memory with max_particles 0 (and drawing then asserts with the
// fix), storage when > 0, items sent in chunks between the pass's other calls, more than kMaxParticleBatches draws,
// over the cap (assert; Release drops and warns once per frame), the same drops however the batches are cut, stats.

#include <cstring>

#include "draw_fixture.hpp"
#include "gfx/particle_batches.hpp"
#include "nnh/memory.hpp"

using nnh::ParticleDesc;
using nnh::ParticleEmitter;
using nnh::Screen;
using nnh::Vec2;
using nnh::detail::ParticleBatches;
using nnh::detail::ParticleBatchRecord;
using nnh::detail::ParticleExpandRequest;
using nnh::detail::ParticleExpandResult;
namespace fake = nnh::platform_fake;

namespace nnh {
NNH_MEM_TAG_EXTERN(gfx); // gfx_frame.cpp
}

namespace {

ParticleDesc squares() {
    ParticleDesc d;
    d.size = {2, 2};
    d.lifetime = {nnh::Frames{100}, nnh::Frames{100}};
    return d;
}

/// A stand-in expand function: `*(int*)source` visible particles.
ParticleExpandResult fake_expand(const ParticleExpandRequest& q) {
    ParticleExpandResult r;
    const int n = *static_cast<const int*>(q.source);
    for (int k = q.first; k < n; ++k) {
        if (q.capacity == 0) {
            ++r.dropped;
            continue;
        }
        if (r.written == q.capacity) {
            r.next = k;
            return r;
        }
        q.out[r.written++] = nnh::platform::DrawItem2D{};
    }
    return r;
}

int g_sent = 0;
void count_sent(Screen, const nnh::platform::DrawItem2D*, int n) { g_sent += n; }

/// Drawn and dropped for batches of sizes `sizes` under a budget of `max`.
void run(const int* sizes, int count, int max, int& drawn, int& dropped) {
    ParticleBatchRecord records[8];
    nnh::platform::DrawItem2D scratch[16];
    ParticleBatches b;
    b.reset(records, 8, scratch, 16, max);
    for (int i = 0; i < count; ++i) {
        ParticleBatchRecord r;
        r.expand = fake_expand;
        r.source = &sizes[i];
        CHECK(b.add(r) == i);
    }
    g_sent = 0;
    for (int i = 0; i < count; ++i) CHECK(b.submit(i, Screen::Top, {}, false, 0, 0, 0, 0, count_sent));
    drawn = b.drawn();
    dropped = b.dropped();
    CHECK(g_sent == drawn);
}

ParticleEmitter* g_e = nullptr;
void draw_without_budget() { g_e->draw(Screen::Top); }
void frame_over_budget() {
    g_e->draw(Screen::Top);
    (void)nnh::frame();
}
void draw_once_more() { g_e->draw(Screen::Top); }

} // namespace

NNH_TEST(particle_budget_drops_the_same_however_batches_are_cut) {
    const int one[1] = {300};
    const int three[3] = {100, 120, 80};
    int drawn = 0, dropped = 0;
    run(one, 1, 250, drawn, dropped);
    CHECK(drawn == 250 && dropped == 50);
    run(three, 3, 250, drawn, dropped);
    CHECK(drawn == 250 && dropped == 50);
}

NNH_TEST(particle_batches_table_is_full_at_its_capacity_and_forgets_sources) {
    ParticleBatchRecord records[2];
    nnh::platform::DrawItem2D scratch[4];
    ParticleBatches b;
    b.reset(records, 2, scratch, 4, 100);
    const int n = 3;
    ParticleBatchRecord r;
    r.expand = fake_expand;
    r.source = &n;
    CHECK(b.add(r) == 0 && b.add(r) == 1 && b.add(r) == -1);
    b.forget(&n);
    CHECK(b.get(0) == nullptr && b.get(1) == nullptr);
    b.clear();
    CHECK(b.count() == 0 && b.drawn() == 0);
}

NNH_TEST(max_particles_zero_costs_no_memory) {
    gfx_test::restart({});
    const std::size_t off = nnh::mem::tag_stats(nnh::NNH_MEM_TAG_REF(gfx)).bytes;
    CHECK(nnh::gfx::stats().max_particles == 0);
    gfx_test::restart({.gfx = {.max_particles = 100}});
    const std::size_t on = nnh::mem::tag_stats(nnh::NNH_MEM_TAG_REF(gfx)).bytes;
    const std::size_t expected = sizeof(ParticleBatchRecord) * nnh::detail::kMaxParticleBatches +
                                 sizeof(nnh::platform::DrawItem2D) * nnh::detail::kParticleScratchItems;
    CHECK(on - off == expected && expected < 10 * 1024);
    CHECK(nnh::gfx::stats().max_particles == 100);
    nnh::shutdown();
}

NNH_TEST(drawing_particles_without_max_particles_asserts_with_the_fix) {
    gfx_test::restart({});
    ParticleEmitter e = particles_test::make(squares(), 8);
    e.emit(2);
    g_e = &e;
    CHECK(nnh::frame());
    const bool crashed = fake::run_expecting_crash(draw_without_budget);
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (crashed) CHECK(std::strstr(fake::last_crash().top, "max_particles") != nullptr);
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 0);
    nnh::shutdown();
}

NNH_TEST(particles_over_max_particles_assert_or_are_dropped_and_logged) {
    gfx_test::restart({.gfx = {.max_particles = 16}});
    ParticleEmitter e = particles_test::make(squares(), 64);
    e.emit_at(Vec2{100, 100}, 20);
    g_e = &e;
    CHECK(nnh::frame());
    const bool crashed = fake::run_expecting_crash(frame_over_budget);
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (crashed) CHECK(std::strstr(fake::last_crash().top, "max_particles") != nullptr);
    if (!nnh::build::kAssertsEnabled) {
        CHECK(fake::state().last_frame_item_count == 16);
        const nnh::gfx::Stats s = nnh::gfx::stats();
        CHECK(s.particles_last_frame == 16 && s.dropped_particles == 4 && s.max_particles == 16);
        CHECK(std::strstr(fake::state().last_console_line, "max_particles") != nullptr);
    }
    nnh::shutdown();
}

NNH_TEST(more_particle_draws_than_a_frame_holds_assert_or_are_dropped) {
    gfx_test::restart({.gfx = {.max_draws = 64, .max_particles = 64}});
    ParticleEmitter e = particles_test::make(squares(), 8);
    g_e = &e;
    CHECK(nnh::frame());
    for (int i = 0; i < nnh::detail::kMaxParticleBatches; ++i) e.draw(Screen::Top);
    const bool crashed = fake::run_expecting_crash(draw_once_more);
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (crashed) CHECK(std::strstr(fake::last_crash().top, "particle draws") != nullptr);
    nnh::shutdown();
}
