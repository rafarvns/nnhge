// Box queries of <nnh/collision.hpp>: overlaps, contains, intersection, separation (spec 012 FR-9).

#include "nnh/collision.hpp"
#include "test.hpp"

using nnh::Rect;
using nnh::Vec2;
namespace collision = nnh::collision;

namespace {
bool same(Rect a, Rect b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }
} // namespace

NNH_TEST(collision_boxes_sharing_area_overlap) {
    CHECK(collision::overlaps({0, 0, 10, 10}, {5, 5, 10, 10}));
    CHECK(collision::overlaps({5, 5, 10, 10}, {0, 0, 10, 10}));
    CHECK(collision::overlaps({0, 0, 10, 10}, {2, 2, 2, 2}));
}

NNH_TEST(collision_boxes_touching_edges_do_not_overlap) {
    CHECK(!collision::overlaps({0, 0, 10, 10}, {10, 0, 10, 10}));
    CHECK(!collision::overlaps({0, 0, 10, 10}, {0, 10, 10, 10}));
    CHECK(!collision::overlaps({0, 0, 10, 10}, {-10, 0, 10, 10}));
    CHECK(!collision::overlaps({0, 0, 10, 10}, {10, 10, 5, 5}));
}

NNH_TEST(collision_separated_boxes_do_not_overlap) {
    CHECK(!collision::overlaps({0, 0, 10, 10}, {20, 0, 10, 10}));
    CHECK(!collision::overlaps({0, 0, 10, 10}, {0, 10.5f, 10, 10}));
}

NNH_TEST(collision_boxes_without_area_never_overlap) {
    CHECK(!collision::overlaps({0, 0, 10, 10}, {5, 5, 0, 3}));
    CHECK(!collision::overlaps({0, 0, 10, 10}, {5, 5, 3, 0}));
    CHECK(!collision::overlaps({5, 5, 0, 0}, {0, 0, 10, 10}));
}

NNH_TEST(collision_boxes_overlap_at_negative_coordinates) {
    CHECK(collision::overlaps({-20, -20, 10, 10}, {-15, -12, 10, 10}));
    CHECK(!collision::overlaps({-20, -20, 10, 10}, {-10, -20, 10, 10}));
}

NNH_TEST(collision_contains_includes_top_left_edges_only) {
    const Rect r{10, 20, 30, 40};
    CHECK(collision::contains(r, {10, 20}));
    CHECK(collision::contains(r, {39.5f, 59.5f}));
    CHECK(!collision::contains(r, {40, 30}));
    CHECK(!collision::contains(r, {20, 60}));
    CHECK(!collision::contains(r, {9.99f, 30}));
    CHECK(!collision::contains(r, {20, 19.99f}));
}

NNH_TEST(collision_empty_box_contains_nothing) { CHECK(!collision::contains({10, 10, 0, 5}, {10, 10})); }

NNH_TEST(collision_intersection_is_the_shared_area) {
    CHECK(same(collision::intersection({0, 0, 10, 10}, {5, 2, 10, 4}), {5, 2, 5, 4}));
    CHECK(same(collision::intersection({5, 2, 10, 4}, {0, 0, 10, 10}), {5, 2, 5, 4}));
    CHECK(same(collision::intersection({0, 0, 10, 10}, {2, 3, 4, 5}), {2, 3, 4, 5}));
}

NNH_TEST(collision_intersection_of_disjoint_or_touching_boxes_is_empty) {
    CHECK(same(collision::intersection({0, 0, 10, 10}, {10, 0, 10, 10}), {}));
    CHECK(same(collision::intersection({0, 0, 10, 10}, {30, 30, 1, 1}), {}));
}

NNH_TEST(collision_separation_pushes_along_the_shallower_axis) {
    // a overlaps b by 2 px on x and 8 px on y: pushed 2 px left.
    CHECK(collision::separation({0, 0, 10, 10}, {8, 2, 10, 10}) == (Vec2{-2, 0}));
    // a overlaps b by 8 px on x and 3 px on y, a is below: pushed 3 px down.
    CHECK(collision::separation({0, 7, 10, 10}, {2, 0, 10, 10}) == (Vec2{0, 3}));
}

NNH_TEST(collision_separation_sign_points_away_from_the_other_box) {
    CHECK(collision::separation({8, 0, 10, 10}, {0, 0, 10, 10}) == (Vec2{2, 0}));
    CHECK(collision::separation({0, -7, 10, 10}, {0, 0, 10, 10}) == (Vec2{0, -3}));
}

NNH_TEST(collision_separation_ends_the_overlap) {
    const Rect a{3, 4, 10, 10};
    const Rect b{9, 1, 6, 30};
    const Vec2 push = collision::separation(a, b);
    CHECK(collision::overlaps(a, b));
    CHECK(!collision::overlaps({a.x + push.x, a.y + push.y, a.w, a.h}, b));
}

NNH_TEST(collision_separation_is_zero_without_overlap) {
    CHECK(collision::separation({0, 0, 10, 10}, {10, 0, 10, 10}) == (Vec2{}));
    CHECK(collision::separation({0, 0, 10, 10}, {50, 50, 10, 10}) == (Vec2{}));
}

NNH_TEST(collision_separation_ties_prefer_x_and_the_negative_direction) {
    // Same box: equal depth on both axes, same centre -> pushed left by its width.
    CHECK(collision::separation({0, 0, 4, 4}, {0, 0, 4, 4}) == (Vec2{-4, 0}));
}
