// Description checks (spec 024 FR-2, FR-3, FR-25; T1.3): the default is valid, every limit both ways, keys and frames
// used first, keys ascending, NaN anywhere refused, the first bad field named.

#include <cmath>
#include <cstring>

#include "particles_fixture.hpp"

using nnh::ParticleDesc;
using nnh::ParticleEmitter;

namespace {

bool ok(const ParticleDesc& d) { return ParticleEmitter::check(d).ok(); }

/// Refused, with `field` in the message.
bool refused(const ParticleDesc& d, const char* field) {
    const nnh::Result<void> r = ParticleEmitter::check(d);
    return !r.ok() && r.error().code == nnh::ErrorCode::InvalidArgument && std::strstr(r.error().message, field) != nullptr;
}

constexpr float kNaN = NAN;

} // namespace

NNH_TEST(default_particle_desc_is_valid) { CHECK(ok(ParticleDesc{})); }

NNH_TEST(default_emitter_makes_white_one_pixel_still_particles_for_60_frames) {
    ParticleEmitter e = particles_test::make(ParticleDesc{}, 4);
    CHECK(e.emit(1) == 1);
    const nnh::ParticleView v = e.particle(0);
    CHECK(v.position.x == 0 && v.position.y == 0 && v.velocity.x == 0 && v.velocity.y == 0);
    CHECK(v.size == 1 && v.color.r == 255 && v.color.g == 255 && v.color.b == 255 && v.color.a == 255);
    CHECK(v.lifetime == nnh::FrameTime(nnh::Frames{60}) && v.age.ticks == 0);
    for (int i = 0; i < 59; ++i) e.update();
    CHECK(e.alive() == 1);
    e.update();
    CHECK(e.alive() == 0);
}

NNH_TEST(particle_desc_rate_limits) {
    ParticleDesc d;
    d.rate = 491520;
    CHECK(ok(d));
    d.rate = 491521;
    CHECK(refused(d, "rate"));
    d.rate = -1;
    CHECK(refused(d, "rate"));
}

NNH_TEST(particle_desc_shape_limits) {
    ParticleDesc d;
    d.radius = 10;
    d.inner_radius = 10;
    CHECK(ok(d));
    d.inner_radius = 10.5f;
    CHECK(refused(d, "inner_radius"));
    d = {};
    d.box = {-1, 2};
    CHECK(refused(d, "box.x"));
    d = {};
    d.arc = nnh::kFullTurn;
    CHECK(ok(d));
    d.arc = nnh::kFullTurn * 1.01f;
    CHECK(refused(d, "arc"));
    d = {};
    d.length = -0.5f;
    CHECK(refused(d, "length"));
    d = {};
    d.shape = static_cast<nnh::ParticleShape>(4);
    CHECK(refused(d, "shape"));
}

NNH_TEST(particle_desc_motion_limits) {
    ParticleDesc d;
    d.spread = nnh::kFullTurn;
    d.inherit_velocity = 1;
    d.damping = 60;
    CHECK(ok(d));
    d.spread = 7;
    CHECK(refused(d, "spread"));
    d = {};
    d.inherit_velocity = 1.01f;
    CHECK(refused(d, "inherit_velocity"));
    d = {};
    d.damping = 60.5f;
    CHECK(refused(d, "damping"));
    d = {};
    d.direction = static_cast<nnh::ParticleDirection>(2);
    CHECK(refused(d, "direction"));
}

NNH_TEST(particle_desc_lifetime_needs_one_tick_and_min_not_above_max) {
    ParticleDesc d;
    d.lifetime = {nnh::FrameTime::from_ticks(1), nnh::FrameTime::from_ticks(1)};
    CHECK(ok(d));
    d.lifetime.min = nnh::FrameTime::from_ticks(0);
    CHECK(refused(d, "lifetime.min"));
    d.lifetime = {nnh::Frames{10}, nnh::Frames{9}};
    CHECK(refused(d, "lifetime"));
    d.lifetime = {nnh::Frames{-1}, nnh::Frames{9}};
    CHECK(refused(d, "lifetime.min"));
}

NNH_TEST(particle_desc_burst_limits) {
    ParticleDesc d;
    d.bursts[3] = {.at = nnh::Frames{10}, .count = {2, 8192}, .cycles = 3, .interval = nnh::Frames{1}};
    CHECK(ok(d));
    d.bursts[3].count = {5, 4};
    CHECK(refused(d, "bursts[3].count"));
    d.bursts[3].count = {1, 8193};
    CHECK(refused(d, "bursts[3].count"));
    d.bursts[3].count = {1, 2};
    d.bursts[3].interval = nnh::FrameTime::from_ticks(4095);
    CHECK(refused(d, "bursts[3].interval"));
    d.bursts[3].cycles = 1; // a single firing needs no interval
    CHECK(ok(d));
    d.bursts[3].at = nnh::Frames{-2};
    CHECK(refused(d, "bursts[3].at"));
}

NNH_TEST(particle_desc_keys_used_first_in_range_and_ascending) {
    ParticleDesc d;
    d.color_keys[0] = {0, nnh::colors::red};
    d.color_keys[1] = {0.5f, nnh::colors::red};
    d.color_keys[2] = {0.5f, nnh::colors::red}; // equal: a step
    CHECK(ok(d));
    d.color_keys[2].at = 0.25f;
    const nnh::Result<void> r = ParticleEmitter::check(d);
    CHECK(!r.ok() && std::strcmp(r.error().message, "color_keys[2].at: keys must be ascending") == 0);
    d.color_keys[2].at = 1.5f;
    CHECK(refused(d, "color_keys[2].at"));
    d.color_keys[2].at = -1;
    d.color_keys[3].at = 0.75f; // used after unused
    CHECK(refused(d, "color_keys[3].at"));
    d = {};
    d.size_keys[0] = {0.2f, -1};
    CHECK(refused(d, "size_keys[0].size"));
}

NNH_TEST(particle_desc_frames_used_first_with_valid_regions) {
    ParticleDesc d;
    d.frames[0].region = {0, 0, 8, 8};
    d.frames[1].region = {8, 0, 0, 0}; // empty: the whole texture
    CHECK(ok(d));
    d.frames[3].region = {0, 0, 8, 8};
    CHECK(refused(d, "frames[3]"));
    d.frames[3].region.w = -1;
    d.frames[1].region.x = -8;
    CHECK(refused(d, "frames[1].region"));
}

NNH_TEST(particle_desc_appearance_limits) {
    ParticleDesc d;
    d.frame_repeats = 0;
    CHECK(refused(d, "frame_repeats"));
    d = {};
    d.frame_rate = 600;
    CHECK(ok(d));
    d.frame_rate = 600.5f;
    CHECK(refused(d, "frame_rate"));
    d = {};
    d.size = {-1, 2};
    CHECK(refused(d, "size.min"));
    d = {};
    d.frame_mode = static_cast<nnh::ParticleFrameMode>(4);
    CHECK(refused(d, "frame_mode"));
    d = {};
    d.space = static_cast<nnh::ParticleSpace>(2);
    CHECK(refused(d, "space"));
    d = {};
    d.order = static_cast<nnh::ParticleOrder>(2);
    CHECK(refused(d, "order"));
    d = {};
    d.prewarm = 600;
    CHECK(ok(d));
    d.prewarm = 601;
    CHECK(refused(d, "prewarm"));
}

NNH_TEST(particle_desc_refuses_nan_anywhere) {
    ParticleDesc d;
    d.gravity.x = kNaN;
    CHECK(refused(d, "gravity.x"));
    d = {};
    d.speed.max = INFINITY;
    CHECK(refused(d, "speed.max"));
    d = {};
    d.color_keys[0].at = kNaN;
    CHECK(refused(d, "color_keys[0].at"));
    d = {};
    d.frames[0].region = {0, 0, 4, 4};
    d.frames[0].pivot.y = kNaN;
    CHECK(refused(d, "frames[0].pivot.y"));
    d = {};
    d.angle = -INFINITY;
    CHECK(refused(d, "angle"));
    d = {};
    d.rotation.min = kNaN;
    CHECK(refused(d, "rotation.min"));
}

NNH_TEST(particle_create_refuses_what_check_refuses) {
    ParticleDesc d;
    d.damping = 61;
    const auto r = ParticleEmitter::create(d, 8);
    CHECK(!r.ok() && std::strstr(r.error().message, "damping") != nullptr);
}
