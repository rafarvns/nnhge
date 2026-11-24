// Directions and speeds (spec 024 FR-3, FR-9; T2.1): the cone's limits, a full-turn spread, outward from circles,
// boxes and lines (unit vectors within 2⁻²⁰), the centre and point fallbacks, negative speeds, inherited velocity.

#include <cmath>

#include "particles_fixture.hpp"

using nnh::ParticleDesc;
using nnh::ParticleEmitter;
using nnh::ParticleShape;
using nnh::Vec2;

namespace {

/// 60 px/s: one pixel per frame, so a velocity is its direction.
ParticleDesc unit_speed() {
    ParticleDesc d;
    d.speed = {60, 60};
    d.seed = 4242;
    return d;
}

float length(Vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }

} // namespace

NNH_TEST(cone_directions_stay_within_angle_plus_minus_half_spread) {
    ParticleDesc d = unit_speed();
    d.angle = -nnh::kQuarterTurn; // up on the y-down screen
    d.spread = 1.0f;
    ParticleEmitter e = particles_test::make(d, 2000);
    e.emit(2000);
    float lo = 10, hi = -10;
    for (int i = 0; i < 2000; ++i) {
        const Vec2 v = e.particle(i).velocity;
        CHECK(std::fabs(length(v) - 1) < 1e-5f);
        CHECK(v.y < 0);
        const float a = std::atan2(v.y, v.x);
        lo = a < lo ? a : lo;
        hi = a > hi ? a : hi;
    }
    CHECK(lo >= -nnh::kQuarterTurn - 0.5f - 1e-4f && hi <= -nnh::kQuarterTurn + 0.5f + 1e-4f);
    CHECK(lo < -nnh::kQuarterTurn - 0.49f && hi > -nnh::kQuarterTurn + 0.49f); // the limits are reached
}

NNH_TEST(full_turn_spread_goes_every_way) {
    ParticleDesc d = unit_speed();
    d.spread = nnh::kFullTurn;
    ParticleEmitter e = particles_test::make(d, 4000);
    e.emit(4000);
    int octant[8] = {};
    for (int i = 0; i < 4000; ++i) {
        const Vec2 v = e.particle(i).velocity;
        const int k = static_cast<int>((std::atan2(v.y, v.x) + 3.14159265f) / (6.2831853f / 8));
        ++octant[k > 7 ? 7 : k];
    }
    for (int c : octant) CHECK(c > 500 - 110 && c < 500 + 110);
}

NNH_TEST(outward_from_a_circle_points_away_from_the_centre) {
    ParticleDesc d = unit_speed();
    d.shape = ParticleShape::Circle;
    d.radius = 20;
    d.inner_radius = 5;
    d.direction = nnh::ParticleDirection::Outward;
    ParticleEmitter e = particles_test::make(d, 500);
    e.emit(500);
    for (int i = 0; i < 500; ++i) {
        const nnh::ParticleView p = e.particle(i);
        const float cross = p.position.x * p.velocity.y - p.position.y * p.velocity.x;
        const float dot = p.position.x * p.velocity.x + p.position.y * p.velocity.y;
        CHECK(std::fabs(cross) < 1e-3f && dot > 0);
        CHECK(std::fabs(length(p.velocity) - 1) < 1e-5f);
    }
}

NNH_TEST(outward_from_a_box_or_line_is_a_unit_vector_from_the_centre) {
    for (ParticleShape s : {ParticleShape::Box, ParticleShape::Line}) {
        ParticleDesc d = unit_speed();
        d.shape = s;
        d.box = {30, 12};
        d.length = 30;
        d.direction = nnh::ParticleDirection::Outward;
        ParticleEmitter e = particles_test::make(d, 500);
        e.emit(500);
        for (int i = 0; i < 500; ++i) {
            const nnh::ParticleView p = e.particle(i);
            CHECK(std::fabs(length(p.velocity) - 1) < 1.0f / (1 << 20));
            const float dot = p.position.x * p.velocity.x + p.position.y * p.velocity.y;
            CHECK(dot > 0);
        }
    }
}

NNH_TEST(outward_from_a_point_falls_back_to_the_cone) {
    ParticleDesc d = unit_speed();
    d.direction = nnh::ParticleDirection::Outward;
    d.angle = nnh::kHalfTurn; // left
    ParticleEmitter e = particles_test::make(d, 8);
    e.emit(8);
    for (int i = 0; i < 8; ++i) CHECK(e.particle(i).velocity.x == -1 && std::fabs(e.particle(i).velocity.y) < 1e-6f);
}

NNH_TEST(negative_speeds_go_the_other_way) {
    ParticleDesc d = unit_speed();
    d.speed = {-60, -60};
    ParticleEmitter e = particles_test::make(d, 4);
    e.emit(1);
    CHECK(e.particle(0).velocity.x == -1);
}

NNH_TEST(speed_ranges_are_per_second) {
    ParticleDesc d;
    d.speed = {60, 120};
    ParticleEmitter e = particles_test::make(d, 1000);
    e.emit(1000);
    for (int i = 0; i < 1000; ++i) {
        const float s = length(e.particle(i).velocity);
        CHECK(s >= 1 - 1e-5f && s <= 2 + 1e-5f);
    }
}

NNH_TEST(inherited_velocity_adds_part_of_the_emitter_movement) {
    ParticleDesc d;
    d.inherit_velocity = 0.5f;
    ParticleEmitter e = particles_test::make(d, 8);
    e.set_position({10, 0});
    e.update(); // the emitter moved 10 px this frame
    e.emit(1);
    CHECK(e.particle(0).velocity.x == 5 && e.particle(0).velocity.y == 0);
    e.update(); // no movement now
    e.emit(1);
    CHECK(e.particle(1).velocity.x == 0);
}
