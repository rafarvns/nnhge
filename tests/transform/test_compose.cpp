// Transform values and composition (spec 022 FR-1, FR-2, FR-4, T1.3): the identity, the order scale → rotate → move, a
// root's world equal to its local bit for bit, the pure-translation rule, quarter turns on whole pixels, mirrors and
// sheared chains (drawing values: accumulated rotation, product of scales).

#include "nnh/transform.hpp"
#include "test.hpp"

#include <bit>
#include <cmath>
#include <cstdint>

using nnh::Transform2D;
using nnh::Vec2;
using nnh::WorldTransform2D;

namespace {

std::uint32_t bits(float f) { return std::bit_cast<std::uint32_t>(f); }
bool same(Vec2 a, Vec2 b) { return bits(a.x) == bits(b.x) && bits(a.y) == bits(b.y); }
bool near(Vec2 a, Vec2 b, float eps = 1e-4f) { return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps; }

} // namespace

NNH_TEST(transform_defaults_are_the_identity) {
    const Transform2D t;
    CHECK(t.position == Vec2{} && t.rotation == 0.0f && t.scale == (Vec2{1, 1}));
    const WorldTransform2D w;
    CHECK(w.map.x == (Vec2{1, 0}) && w.map.y == (Vec2{0, 1}) && w.position() == Vec2{});
    CHECK(w.rotation == 0.0f && w.scale == (Vec2{1, 1}));
    const WorldTransform2D r = nnh::to_world(t);
    CHECK(r.map.x == (Vec2{1, 0}) && r.map.y == (Vec2{0, 1}) && r.position() == Vec2{});
}

NNH_TEST(transform_scales_then_rotates_then_moves) {
    // (1, 0) scaled by (2, 3) → (2, 0), turned a quarter clockwise (y down) → (0, 2), moved by (10, 20) → (10, 22).
    const Transform2D t{.position = {10, 20}, .rotation = nnh::kQuarterTurn, .scale = {2, 3}};
    const WorldTransform2D w = nnh::to_world(t);
    CHECK(nnh::transform_point(w.map, {1, 0}) == (Vec2{10, 22}));
    CHECK(nnh::transform_point(w.map, {0, 1}) == (Vec2{7, 20})); // (0, 3) turned → (−3, 0)
}

NNH_TEST(transform_root_world_equals_its_local_bit_for_bit) {
    const Transform2D t{.position = {-0.1f, 1234.5678f}, .rotation = 0.73f, .scale = {-1.5f, 0.25f}};
    const WorldTransform2D w = nnh::to_world(t);
    CHECK(same(w.position(), t.position) && bits(w.rotation) == bits(t.rotation) && same(w.scale, t.scale));
}

NNH_TEST(transform_child_of_an_unmoved_parent_is_the_sum_of_positions) {
    const Transform2D parent{.position = {100.25f, -7.5f}};
    const Transform2D child{.position = {0.1f, 3.3f}};
    const WorldTransform2D w = nnh::compose(nnh::to_world(parent), child);
    CHECK(w.position() == (Vec2{100.25f + 0.1f, -7.5f + 3.3f}));
    // A chain of unrotated, unscaled nodes: hand-added positions, as IEEE values.
    WorldTransform2D at = nnh::to_world(parent);
    float x = parent.position.x, y = parent.position.y;
    for (int i = 0; i < 15; ++i) {
        const Transform2D link{.position = {0.37f * static_cast<float>(i), -1.1f}};
        at = nnh::compose(at, link);
        x = x + link.position.x;
        y = y + link.position.y;
    }
    CHECK(at.position() == (Vec2{x, y}));
}

NNH_TEST(transform_pure_translation_keeps_the_parent_axes_and_drawing_values) {
    const WorldTransform2D p = nnh::to_world({.position = {5, 6}, .rotation = 0.4f, .scale = {2, -3}});
    const WorldTransform2D c = nnh::compose(p, {.position = {1, 2}});
    CHECK(same(c.map.x, p.map.x) && same(c.map.y, p.map.y));
    CHECK(bits(c.rotation) == bits(p.rotation) && same(c.scale, p.scale));
    CHECK(near(c.position(), nnh::transform_point(p.map, {1, 2})));
}

NNH_TEST(transform_full_rule_agrees_with_the_translation_rule_on_points) {
    // A local with rotation −0 (not the translation rule's +0 bits) computes the same points.
    const WorldTransform2D p = nnh::to_world({.position = {5, 6}, .rotation = 0.4f, .scale = {2, 3}});
    const WorldTransform2D a = nnh::compose(p, {.position = {1, 2}});
    const WorldTransform2D b = nnh::compose(p, {.position = {1, 2}, .rotation = -0.0f});
    CHECK(near(a.position(), b.position(), 1e-5f) && near(a.map.x, b.map.x, 1e-6f) && near(a.map.y, b.map.y, 1e-6f));
}

NNH_TEST(transform_quarter_turns_land_on_whole_pixels) {
    WorldTransform2D at = nnh::to_world({.position = {64, 32}, .rotation = nnh::kQuarterTurn});
    at = nnh::compose(at, {.position = {10, 0}, .rotation = nnh::kQuarterTurn}); // (64, 42), facing left
    CHECK(at.position() == (Vec2{64, 42}));
    at = nnh::compose(at, {.position = {5, 0}}); // 5 px to its left
    CHECK(at.position() == (Vec2{59, 42}));
    at = nnh::compose(at, {.position = {3, 4}, .rotation = nnh::kHalfTurn});
    CHECK(at.position() == (Vec2{56, 38}));
    CHECK(at.map.x == (Vec2{1, 0}) && at.map.y == (Vec2{0, 1})); // three quarters + a half = a full turn
}

NNH_TEST(transform_mirror_flips_children_positions_and_rotations) {
    // A character facing left: scale.x = −1 on the root.
    const WorldTransform2D root = nnh::to_world({.position = {100, 50}, .scale = {-1, 1}});
    const WorldTransform2D sword = nnh::compose(root, {.position = {8, -2}, .rotation = 0.5f});
    CHECK(sword.position() == (Vec2{92, 48}));
    CHECK(sword.rotation == -0.5f);                     // turns the other way on screen
    CHECK(sword.scale == (Vec2{-1, 1}));                // the drawing scale carries the flip
    // The drawing values reproduce the map: R(rotation)·diag(scale) == map's axes.
    const WorldTransform2D drawn = nnh::to_world({.position = sword.position(), .rotation = sword.rotation,
                                                  .scale = sword.scale});
    CHECK(near(drawn.map.x, sword.map.x, 1e-6f) && near(drawn.map.y, sword.map.y, 1e-6f));
}

NNH_TEST(transform_double_mirror_cancels) {
    const WorldTransform2D root = nnh::to_world({.position = {0, 0}, .scale = {-1, -1}});
    const WorldTransform2D child = nnh::compose(root, {.position = {3, 4}, .rotation = 0.25f});
    CHECK(child.position() == (Vec2{-3, -4}) && child.rotation == 0.25f); // a half turn, not a mirror
}

NNH_TEST(transform_shear_keeps_points_exact_and_drawing_values_approximate) {
    // A squash parent (2, ½) with a child turned a quarter: the child's square becomes a sheared shape.
    const WorldTransform2D squash = nnh::to_world({.position = {10, 10}, .scale = {2, 0.5f}});
    const WorldTransform2D child = nnh::compose(squash, {.position = {4, 4}, .rotation = nnh::kQuarterTurn});
    CHECK(child.position() == (Vec2{18, 12}));
    CHECK(nnh::transform_point(child.map, {1, 0}) == (Vec2{18, 12.5f})); // child's x axis = parent's y, squashed
    CHECK(nnh::transform_point(child.map, {0, 1}) == (Vec2{16, 12}));    // child's y axis = parent's −x, stretched
    CHECK(child.rotation == nnh::kQuarterTurn && child.scale == (Vec2{2, 0.5f})); // accumulated, product (lossy)
}

NNH_TEST(transform_compose_is_associative_on_points_within_rounding) {
    const WorldTransform2D a = nnh::to_world({.position = {3, 4}, .rotation = 0.3f, .scale = {1.5f, 1.5f}});
    const WorldTransform2D b = nnh::compose(a, {.position = {10, -2}, .rotation = -1.1f, .scale = {0.5f, 0.5f}});
    const WorldTransform2D c = nnh::compose(b, {.position = {7, 7}, .rotation = 2.0f});
    const Vec2 through = nnh::transform_point(a.map, nnh::transform_point(
                                                         nnh::to_world({.position = {10, -2}, .rotation = -1.1f,
                                                                        .scale = {0.5f, 0.5f}}).map,
                                                         {7, 7}));
    CHECK(near(c.position(), through, 1e-4f));
}
