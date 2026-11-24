// Shapes (spec 024 FR-8; T2.1): every shape's births inside it, on the outline, in the ring, on the arc; uniform by
// area (disc and ring by equal-area annuli, box by quadrants) or by length (box outlines, lines), within fixed bounds.

#include <cmath>

#include "particles/particle_shapes.hpp"
#include "particles_fixture.hpp"

using nnh::ParticleDesc;
using nnh::ParticleEmitter;
using nnh::ParticleShape;
using nnh::Vec2;

namespace {

constexpr int kBirths = 8000;

/// Births of `d` (no motion) one by one at the origin; calls `visit` with each position.
template <typename Visit>
void births(const ParticleDesc& d, Visit visit) {
    ParticleEmitter e = particles_test::make(d, 8192);
    CHECK(e.emit(kBirths) == kBirths);
    for (int i = 0; i < kBirths; ++i) visit(e.particle(i).position);
}

ParticleDesc shape(ParticleShape s) {
    ParticleDesc d;
    d.shape = s;
    d.seed = 12345;
    return d;
}

} // namespace

NNH_TEST(point_shape_births_at_the_emitter) {
    ParticleEmitter e = particles_test::make(shape(ParticleShape::Point), 16);
    e.set_position({5, 7});
    e.emit(4);
    for (int i = 0; i < 4; ++i) CHECK(e.particle(i).position == (Vec2{5, 7}));
}

NNH_TEST(box_area_births_inside_uniform_by_quadrant) {
    ParticleDesc d = shape(ParticleShape::Box);
    d.box = {40, 20};
    int quadrant[4] = {};
    births(d, [&](Vec2 p) {
        CHECK(p.x >= -20 && p.x <= 20 && p.y >= -10 && p.y <= 10);
        ++quadrant[(p.x < 0 ? 0 : 1) + (p.y < 0 ? 0 : 2)];
    });
    for (int q : quadrant) CHECK(q > 2000 - 230 && q < 2000 + 230);
}

NNH_TEST(box_edge_births_on_the_outline_uniform_by_length) {
    ParticleDesc d = shape(ParticleShape::Box);
    d.box = {30, 10};
    d.edge = true;
    int horizontal = 0, vertical = 0;
    births(d, [&](Vec2 p) {
        const bool on_x = std::fabs(std::fabs(p.x) - 15) < 1e-4f && std::fabs(p.y) <= 5;
        const bool on_y = std::fabs(std::fabs(p.y) - 5) < 1e-4f && std::fabs(p.x) <= 15;
        CHECK(on_x || on_y);
        if (on_y) ++horizontal; else ++vertical;
    });
    // Perimeter 80: the top and bottom are 60 of it.
    CHECK(horizontal > 6000 - 250 && horizontal < 6000 + 250);
    CHECK(horizontal + vertical == kBirths);
}

NNH_TEST(circle_area_births_inside_uniform_by_equal_area_rings) {
    ParticleDesc d = shape(ParticleShape::Circle);
    d.radius = 16;
    int rings[4] = {};
    births(d, [&](Vec2 p) {
        const float r = std::sqrt(p.x * p.x + p.y * p.y);
        CHECK(r <= 16.001f);
        const int k = static_cast<int>(4 * (r * r) / 256.0f); // equal areas: r² in quarters of R²
        ++rings[k > 3 ? 3 : k];
    });
    for (int c : rings) CHECK(c > 2000 - 230 && c < 2000 + 230);
}

NNH_TEST(ring_births_between_the_radii_uniform_by_area) {
    ParticleDesc d = shape(ParticleShape::Circle);
    d.inner_radius = 10;
    d.radius = 20;
    int rings[4] = {};
    births(d, [&](Vec2 p) {
        const float r = std::sqrt(p.x * p.x + p.y * p.y);
        CHECK(r >= 9.999f && r <= 20.001f);
        const int k = static_cast<int>(4 * (r * r - 100) / 300.0f);
        ++rings[k < 0 ? 0 : (k > 3 ? 3 : k)];
    });
    for (int c : rings) CHECK(c > 2000 - 230 && c < 2000 + 230);
}

NNH_TEST(ring_of_zero_width_births_on_the_circle) {
    ParticleDesc d = shape(ParticleShape::Circle);
    d.inner_radius = d.radius = 12;
    births(d, [&](Vec2 p) { CHECK(std::fabs(std::sqrt(p.x * p.x + p.y * p.y) - 12) < 1e-3f); });
}

NNH_TEST(arc_births_within_the_arc) {
    ParticleDesc d = shape(ParticleShape::Circle);
    d.radius = 10;
    d.inner_radius = 9;
    d.arc_start = 0.5f;
    d.arc = 1.0f;
    births(d, [&](Vec2 p) {
        const float a = std::atan2(p.y, p.x);
        CHECK(a >= 0.5f - 1e-3f && a <= 1.5f + 1e-3f);
    });
}

NNH_TEST(line_births_along_local_x_uniform) {
    ParticleDesc d = shape(ParticleShape::Line);
    d.length = 50;
    int halves[2] = {};
    births(d, [&](Vec2 p) {
        CHECK(p.y == 0 && p.x >= -25 && p.x <= 25);
        ++halves[p.x < 0 ? 0 : 1];
    });
    CHECK(halves[0] > 4000 - 230 && halves[0] < 4000 + 230);
}

NNH_TEST(shapes_follow_the_emitter_transform) {
    ParticleDesc d = shape(ParticleShape::Line);
    d.length = 10;
    ParticleEmitter e = particles_test::make(d, 64);
    e.set_transform(nnh::Transform2D{.position = {100, 50}, .rotation = nnh::kQuarterTurn});
    e.emit(32);
    for (int i = 0; i < 32; ++i) {
        const Vec2 p = e.particle(i).position;
        CHECK(p.x == 100 && p.y >= 45 && p.y <= 55); // the line turned a quarter: along +y
    }
}

NNH_TEST(reciprocal_sqrt_is_within_float_precision) {
    for (float x : {1e-20f, 0.001f, 0.5f, 1.0f, 2.0f, 3.0f, 1234.5f, 9.9e20f}) {
        const double exact = 1.0 / std::sqrt(static_cast<double>(x));
        CHECK(std::fabs(nnh::detail::reciprocal_sqrt(x) - exact) / exact < 1e-6);
    }
}
