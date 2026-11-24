// Queries (spec 024 FR-22; T3.5): description, transform, capacity, live count, bounds, particle views (oldest first)
// and the counters.

#include "particles_fixture.hpp"

using nnh::ParticleDesc;
using nnh::ParticleEmitter;
using nnh::Vec2;

NNH_TEST(particle_queries_answer_the_emitter_state) {
    ParticleDesc d;
    d.rate = 30;
    d.seed = 77;
    ParticleEmitter e = particles_test::make(d, 32);
    CHECK(e.desc().rate == 30 && e.desc().seed == 77 && e.capacity() == 32);
    e.set_transform(nnh::Transform2D{.position = {5, 6}, .rotation = 0.5f});
    CHECK(e.transform().position() == (Vec2{5, 6}) && e.transform().rotation == 0.5f);
    const nnh::Rect empty = e.bounds();
    CHECK(empty.x == 5 && empty.y == 6 && empty.w == 0 && empty.h == 0);
    e.emit_at(Vec2{1, 2}, 1);
    e.emit_at(Vec2{9, -4}, 1);
    const nnh::Rect r = e.bounds();
    CHECK(r.x == 1 && r.y == -4 && r.w == 8 && r.h == 6);
    CHECK(e.particle(0).position == (Vec2{1, 2}) && e.particle(1).position == (Vec2{9, -4})); // oldest first
    const nnh::ParticleStats s = e.stats();
    CHECK(s.live == 2 && s.peak == 2 && s.capacity == 32 && s.emitted == 2 && s.dropped == 0);
}

NNH_TEST(particle_peak_stays_after_deaths) {
    ParticleDesc d;
    d.lifetime = {nnh::Frames{2}, nnh::Frames{2}};
    ParticleEmitter e = particles_test::make(d, 32);
    e.emit(20);
    e.update();
    e.update();
    e.emit(3);
    const nnh::ParticleStats s = e.stats();
    CHECK(s.live == 3 && s.peak == 20 && s.emitted == 23);
}

NNH_TEST(particle_view_reports_age_and_lifetime) {
    ParticleDesc d;
    d.lifetime = {nnh::Frames{30}, nnh::Frames{30}};
    ParticleEmitter e = particles_test::make(d, 4);
    e.emit(1);
    for (int i = 0; i < 7; ++i) e.update();
    CHECK(e.particle(0).age == nnh::FrameTime(nnh::Frames{7}));
    CHECK(e.particle(0).lifetime == nnh::FrameTime(nnh::Frames{30}));
}
