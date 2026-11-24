// Keep-world math (spec 022 FR-10, T1.5): compose(parent, relative(parent, w)) puts a node back at w — its points
// within FR-3's bound, its drawing values exactly for power-of-two scales and quarter turns; mirrored parents; a
// parent that can't be inverted is refused.

#include "nnh/transform.hpp"
#include "test.hpp"

#include <cmath>

using nnh::Transform2D;
using nnh::Vec2;
using nnh::WorldTransform2D;

namespace {

constexpr float kBound = 1.0f / 32.0f;

bool near(Vec2 a, Vec2 b, float eps) { return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps; }

/// The parent's world composed with the relative local reproduces `w`.
bool round_trip(const WorldTransform2D& parent, const WorldTransform2D& w, bool exact_drawing) {
    Transform2D local;
    if (!nnh::relative(parent, w, local)) return false;
    const WorldTransform2D back = nnh::compose(parent, local);
    bool ok = near(back.position(), w.position(), kBound);
    ok = ok && near(nnh::transform_point(back.map, {16, -9}), nnh::transform_point(w.map, {16, -9}), kBound);
    if (exact_drawing) ok = ok && back.rotation == w.rotation && back.scale == w.scale;
    return ok;
}

} // namespace

NNH_TEST(relative_reproduces_the_world_under_rotated_scaled_parents) {
    for (int i = 0; i < 64; ++i) {
        const float f = static_cast<float>(i);
        const WorldTransform2D parent =
            nnh::compose(nnh::to_world({.position = {f * 10 - 300, 120 - f * 3}, .rotation = f * 0.21f}),
                         {.position = {5, 5}, .rotation = -0.3f * f, .scale = {1.5f, 1.5f}});
        const WorldTransform2D w = nnh::to_world({.position = {f * 7 - 100, f}, .rotation = 1.3f - f * 0.05f,
                                                  .scale = {0.75f, 0.75f}});
        CHECK(round_trip(parent, w, false));
    }
}

NNH_TEST(relative_is_exact_for_quarter_turns_and_powers_of_two) {
    const float turns[] = {0.0f, nnh::kQuarterTurn, nnh::kHalfTurn, nnh::kThreeQuarterTurn};
    const float scales[] = {0.5f, 1.0f, 2.0f, 4.0f};
    for (float pr : turns) {
        for (float ps : scales) {
            for (float wr : turns) {
                const WorldTransform2D parent = nnh::to_world({.position = {64, -32}, .rotation = pr, .scale = {ps, ps}});
                const WorldTransform2D w = nnh::to_world({.position = {100, 20}, .rotation = wr + pr, .scale = {2, 2}});
                CHECK(round_trip(parent, w, true));
                Transform2D local;
                CHECK(nnh::relative(parent, w, local));
                CHECK(local.scale == (Vec2{2 / ps, 2 / ps}));
            }
        }
    }
}

NNH_TEST(relative_under_a_mirrored_parent_turns_the_other_way) {
    const WorldTransform2D parent = nnh::to_world({.position = {100, 50}, .scale = {-1, 1}});
    const WorldTransform2D w = nnh::to_world({.position = {92, 48}, .rotation = -0.5f, .scale = {-1, 1}});
    Transform2D local;
    CHECK(nnh::relative(parent, w, local));
    CHECK(local.position == (Vec2{8, -2}) && local.rotation == 0.5f && local.scale == (Vec2{1, 1}));
    CHECK(round_trip(parent, w, true));
}

NNH_TEST(relative_refuses_a_parent_that_cannot_be_inverted) {
    const WorldTransform2D parent = nnh::to_world({.position = {1, 1}, .scale = {0, 2}});
    Transform2D local{.position = {9, 9}};
    CHECK(!nnh::relative(parent, nnh::to_world({}), local));
    CHECK(local.position == (Vec2{9, 9}));
}
