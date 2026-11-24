// Motion (spec 024 FR-3, FR-11, FR-17; T3.1): semi-implicit Euler per 60 Hz frame in the documented order (velocity +=
// gravity, × keep, flushed; position += velocity; rotation += spin), per-second units, exact lifetimes in updates, the
// velocity flush, no allocation.

#include <bit>

#include "alloc_counter.hpp"
#include "nnh/memory.hpp"
#include "particles/particle_bits.hpp"
#include "particles_fixture.hpp"

using nnh::Frames;
using nnh::ParticleDesc;
using nnh::ParticleEmitter;

NNH_TEST(motion_units_are_per_second_and_per_second_squared) {
    ParticleDesc d;
    d.speed = {60, 60}; // 1 px per frame along +x
    d.gravity = {0, 3600}; // 1 px per frame² down
    d.lifetime = {Frames{100}, Frames{100}};
    ParticleEmitter e = particles_test::make(d, 4);
    e.emit(1);
    e.update();
    CHECK(e.particle(0).position.x == 1 && e.particle(0).velocity.y == 1 && e.particle(0).position.y == 1);
    e.update();
    CHECK(e.particle(0).position.x == 2 && e.particle(0).velocity.y == 2 && e.particle(0).position.y == 3);
    for (int i = 0; i < 58; ++i) e.update();
    CHECK(e.particle(0).position.x == 60); // 60 px/s for one second
}

NNH_TEST(motion_follows_the_documented_order) {
    ParticleDesc d;
    d.speed = {90, 90};
    d.angle = 0.3f;
    d.gravity = {12, 340};
    d.damping = 3.5f;
    d.spin = {2.5f, 2.5f};
    d.rotation = {0.25f, 0.25f};
    d.lifetime = {Frames{200}, Frames{200}};
    ParticleEmitter e = particles_test::make(d, 4);
    e.emit(1);
    const nnh::ParticleView start = e.particle(0);
    float px = start.position.x, py = start.position.y, vx = start.velocity.x, vy = start.velocity.y;
    float rot = start.rotation;
    const float gx = 12.0f / 3600.0f, gy = 340.0f / 3600.0f, keep = 1.0f - 3.5f / 60.0f, spin = 2.5f / 60.0f;
    for (int f = 0; f < 120; ++f) {
        e.update();
        vx = (vx + gx) * keep;
        vy = (vy + gy) * keep;
        px = px + vx;
        py = py + vy;
        rot = rot + spin;
        const nnh::ParticleView v = e.particle(0);
        CHECK(particles_test::bits(v.velocity.x) == particles_test::bits(vx));
        CHECK(particles_test::bits(v.velocity.y) == particles_test::bits(vy));
        CHECK(particles_test::bits(v.position.x) == particles_test::bits(px));
        CHECK(particles_test::bits(v.position.y) == particles_test::bits(py));
        CHECK(particles_test::bits(v.rotation) == particles_test::bits(rot));
        CHECK(v.age.ticks == (f + 1) * 4096);
    }
}

NNH_TEST(lifetimes_are_exact_in_updates) {
    for (nnh::FrameTime life : {nnh::FrameTime(Frames{24}), nnh::FrameTime(nnh::Duration::from_ms(400)),
                                nnh::FrameTime(Frames{1})}) {
        ParticleDesc d;
        d.lifetime = {life, life};
        ParticleEmitter e = particles_test::make(d, 4);
        e.emit(1);
        const int frames = life.ticks / 4096;
        for (int i = 0; i < frames - 1; ++i) e.update();
        CHECK(e.alive() == 1);
        e.update();
        CHECK(e.alive() == 0);
    }
}

NNH_TEST(tiny_velocities_flush_to_zero_under_drag) {
    ParticleDesc d;
    d.speed = {1e-15f, 1e-15f};
    d.damping = 59;
    d.lifetime = {Frames{100}, Frames{100}};
    ParticleEmitter e = particles_test::make(d, 4);
    e.emit(1); // 1.7e-17 px per frame: kept (above 2^-60)
    CHECK(particles_test::bits(e.particle(0).velocity.x) != 0);
    e.update(); // × (1 − 59/60) = 2.8e-19: below 2^-60, so +0 (a normal float on the PC, but the rule is the same)
    CHECK(particles_test::bits(e.particle(0).velocity.x) == 0);
    for (int i = 0; i < 40; ++i) {
        e.update();
        const nnh::ParticleView v = e.particle(0);
        CHECK(particles_test::bits(v.velocity.x) == 0); // +0, never a subnormal
        CHECK(particles_test::bits(v.velocity.y) == 0);
    }
}

NNH_TEST(flush_threshold_is_two_to_the_minus_60) {
    const float at = std::bit_cast<float>(0x21800000u);    // 2^-60
    const float below = std::bit_cast<float>(0x217FFFFFu); // just under it
    CHECK(nnh::detail::flush_tiny(at) == at && nnh::detail::flush_tiny(-at) == -at);
    CHECK(particles_test::bits(nnh::detail::flush_tiny(below)) == 0);
    CHECK(particles_test::bits(nnh::detail::flush_tiny(-below)) == 0);
}

NNH_TEST(rate_births_come_before_bursts_in_a_frame) {
    ParticleDesc d;
    d.rate = 120; // two a frame, at fractions 0.5 and 1
    d.bursts[0] = {.count = {3, 3}};
    d.lifetime = {Frames{100}, Frames{100}};
    ParticleEmitter e = particles_test::make(d, 16);
    e.set_position({100, 0});
    e.update();
    CHECK(e.alive() == 5);
    CHECK(e.particle(0).position.x > 49.99f && e.particle(0).position.x < 50.01f); // the rate's, spread
    CHECK(e.particle(1).position.x == 100);
    for (int i = 2; i < 5; ++i) CHECK(e.particle(i).position.x == 100 && e.particle(i).age.ticks == 0); // the burst
}

NNH_TEST(update_allocates_nothing) {
    ParticleDesc d = particles_test::moving_desc();
    d.rate = 400;
    d.damping = 1;
    d.spin = {1, 2};
    d.bursts[0] = {.count = {5, 9}, .cycles = 0, .interval = Frames{7}};
    ParticleEmitter e = particles_test::make(d, 512);
    const auto news = alloc_counter::news();
    const int live = nnh::mem::live_allocations();
    for (int f = 0; f < 600; ++f) {
        e.set_position({static_cast<float>(f), 0});
        e.update();
        e.emit_at({1, 2}, 3);
    }
    CHECK(alloc_counter::news() == news && nnh::mem::live_allocations() == live);
}

NNH_TEST(bounds_cover_every_live_position) {
    ParticleEmitter e = particles_test::make(particles_test::moving_desc(), 256);
    e.set_position({40, 30});
    for (int f = 0; f < 30; ++f) {
        e.emit(5);
        e.update();
        const nnh::Rect r = e.bounds();
        for (int i = 0; i < e.alive(); ++i) {
            const nnh::Vec2 p = e.particle(i).position;
            CHECK(p.x >= r.x && p.y >= r.y);
            CHECK(p.x <= r.x + r.w + 1e-4f && p.y <= r.y + r.h + 1e-4f); // x + w rounds once
        }
        // The extremes are reached: bounds are the positions' own minimum and maximum.
        bool min_x = false;
        for (int i = 0; i < e.alive(); ++i) min_x = min_x || e.particle(i).position.x == r.x;
        CHECK(min_x || e.alive() == 0);
    }
}
