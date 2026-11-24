// The path follower (spec 025 FR-19; T4.3): advancing within the radius, several waypoints in one call, overshooting the
// last point then arriving, an empty list, NaN positions and radii.

#include <initializer_list>

#include "path_fixture.hpp"

using nnh::PathFollower;
using nnh::Vec2;

namespace {

const Vec2 kPoints[4] = {{0, 0}, {10, 0}, {12, 0}, {12, 20}};

} // namespace

NNH_TEST(path_follower_walks_toward_the_next_point_and_advances_within_the_radius) {
    PathFollower f(kPoints, 4, 2);
    CHECK(f.target({0, 0}) == (Vec2{10, 0}) && f.index() == 1 && !f.arrived()); // (0, 0) is within 2 of point 0
    CHECK(f.target({5, 0}) == (Vec2{10, 0}));
    CHECK(f.target({8.5f, 1}) == (Vec2{12, 0}) && f.index() == 2); // 1.5^2 + 1^2 = 3.25 <= 4: advance
    CHECK(f.target({0, 0}) == (Vec2{12, 0}) && f.index() == 2);      // never goes back
}

NNH_TEST(path_follower_passes_several_waypoints_in_one_call) {
    PathFollower f(kPoints, 4, 3);
    CHECK(f.target({0, 0}) == (Vec2{10, 0}));
    CHECK(f.target({11, 0}) == (Vec2{12, 20}) && f.index() == 3); // within 3 of points 1 and 2
}

NNH_TEST(path_follower_arrives_only_at_the_last_point_after_an_overshoot) {
    PathFollower f(kPoints, 4, 1);
    f.target({0, 0});
    f.target({10, 0});
    f.target({12, 0});
    CHECK(f.index() == 3);
    CHECK(f.target({12, 10}) == (Vec2{12, 20}) && !f.arrived());
    CHECK(f.target({12, 25}) == (Vec2{12, 20}) && !f.arrived()); // overshot: walk back
    CHECK(f.target({12, 20.5f}) == (Vec2{12, 20}) && f.arrived());
    CHECK(f.index() == 3);
    CHECK(f.target({12, 23}) == (Vec2{12, 20}) && !f.arrived()); // pushed away: not arrived any more
}

NNH_TEST(path_follower_with_an_empty_list_is_arrived_and_returns_the_position) {
    PathFollower f;
    CHECK(f.arrived() && f.target({3, 4}) == (Vec2{3, 4}));
    f.reset(kPoints, 0, 5);
    CHECK(f.arrived() && f.target({7, 8}) == (Vec2{7, 8}) && f.index() == 0);
    f.reset(kPoints, 1, 0);
    CHECK(!f.arrived());
    CHECK(f.target({0, 0}) == (Vec2{0, 0}) && f.arrived()); // radius 0: exactly on it
}

NNH_TEST(path_follower_uses_squared_distances_on_the_radius_edge) {
    const Vec2 pts[2] = {{0, 0}, {3, 4}};
    PathFollower f(pts, 2, 5);
    CHECK(f.target({0, 0}) == (Vec2{3, 4}));
    CHECK(f.target({0, 0}) == (Vec2{3, 4}) && f.arrived()); // |(3, 4)| = 5: on the edge counts
    const Vec2 line[3] = {{0, 0}, {10, 0}, {20, 0}};
    PathFollower g(line, 3, 5);
    CHECK(g.target({4, 3}) == (Vec2{10, 0}) && g.index() == 1); // exactly 5 from point 0: passed
}

namespace {

PathFollower* g_f = nullptr;
Vec2 g_t{};
void nan_position() { g_t = g_f->target({__builtin_nanf(""), 0}); }
void inf_position() { g_t = g_f->target({0, __builtin_inff()}); }
void nan_radius() { g_f->reset(kPoints, 4, __builtin_nanf("")); }
void negative_count() { g_f->reset(kPoints, -1, 2); }
void null_points() { g_f->reset(nullptr, 3, 2); }

} // namespace

NNH_TEST(path_follower_misuse_asserts_and_changes_nothing_in_release) {
    PathFollower f(kPoints, 4, 2);
    f.target({0, 0});
    g_f = &f;
    for (auto fn : {nan_position, inf_position}) {
        g_t = {};
        CHECK(path_test::asserts_or_is_ignored(fn));
        if (!nnh::build::kAssertsEnabled) CHECK(g_t == (Vec2{10, 0}) && f.index() == 1);
    }
    PathFollower done(kPoints, 1, 2); // arrived at its only point: a NaN position leaves it arrived
    CHECK(done.target({0, 0}) == (Vec2{0, 0}) && done.arrived());
    g_f = &done;
    CHECK(path_test::asserts_or_is_ignored(nan_position));
    CHECK(done.arrived() && done.index() == 0);
    g_f = &f;
    for (auto fn : {nan_radius, negative_count, null_points}) {
        CHECK(path_test::asserts_or_is_ignored(fn));
        if (!nnh::build::kAssertsEnabled) CHECK(f.arrived() && f.target({1, 1}) == (Vec2{1, 1})); // emptied
    }
}
