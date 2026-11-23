// The slab test of one box (spec 019 FR-10): entry faces, touching is not hitting, corners, inside starts.

#include "collision/ray_box.hpp"
#include "test.hpp"

namespace collision = nnh::collision;
using collision::detail::compare;
using collision::detail::make_segment;
using collision::detail::Ratio;
using collision::detail::slab_cast;
using collision::detail::SlabHit;
using nnh::Rect;
using nnh::Vec2;

namespace {

const Rect kBox{10, 10, 10, 10}; // (10, 20) on both axes

SlabHit cast(Vec2 from, Vec2 to, Rect box = kBox) {
    return slab_cast(make_segment(from, to), collision::detail::slab_x(box), collision::detail::slab_y(box));
}

bool at(const SlabHit& h, Ratio t) { return h.hit && !h.inside && compare(h.t, t) == 0; }

} // namespace

NNH_TEST(a_ray_enters_each_face) {
    SlabHit h = cast({0, 15}, {30, 15}); // from the left
    CHECK(at(h, {10, 30}) && h.face_x);
    h = cast({30, 15}, {0, 15}); // from the right
    CHECK(at(h, {10, 30}) && h.face_x);
    h = cast({15, 0}, {15, 40}); // from the top
    CHECK(at(h, {10, 40}) && !h.face_x);
    h = cast({15, 40}, {15, 0}); // from the bottom
    CHECK(at(h, {20, 40}) && !h.face_x);
}

NNH_TEST(a_diagonal_ray_enters_through_the_later_face) {
    SlabHit h = cast({0, 5}, {20, 25}); // x = 10 at t 0.5, y = 10 at t 0.25: the x face
    CHECK(at(h, {10, 20}) && h.face_x);
    h = cast({5, 0}, {25, 20}); // y face later
    CHECK(at(h, {10, 20}) && !h.face_x);
}

NNH_TEST(a_ray_through_a_corner_enters_through_the_x_face) {
    const SlabHit h = cast({0, 0}, {20, 20});
    CHECK(at(h, {10, 20}) && h.face_x);
}

NNH_TEST(touching_is_not_hitting) {
    CHECK(!cast({0, 10}, {30, 10}).hit);  // along the top edge
    CHECK(!cast({20, 0}, {20, 30}).hit);  // along the right edge
    CHECK(!cast({0, 20}, {20, 0}).hit);   // only the corner (10, 10)
    CHECK(!cast({0, 15}, {10, 15}).hit);  // ends on the left face
    CHECK(!cast({20, 15}, {30, 15}).hit); // starts on the right face going out
    CHECK(!cast({0, 15}, {5, 15}).hit);   // stops short
}

NNH_TEST(a_ray_starting_flush_on_a_face_going_in_hits_at_zero) {
    SlabHit h = cast({10, 15}, {30, 15});
    CHECK(at(h, collision::detail::kRatioZero) && h.face_x);
    h = cast({15, 20}, {15, 0});
    CHECK(at(h, collision::detail::kRatioZero) && !h.face_x);
    h = cast({10, 10}, {20, 20}); // from the corner, inwards: the x face
    CHECK(at(h, collision::detail::kRatioZero) && h.face_x);
}

NNH_TEST(a_start_inside_is_reported_as_inside) {
    SlabHit h = cast({15, 15}, {50, 15});
    CHECK(h.hit && h.inside && compare(h.t, collision::detail::kRatioZero) == 0);
    h = cast({15, 15}, {16, 16}); // ends inside too
    CHECK(h.hit && h.inside);
}

NNH_TEST(an_axis_without_motion_needs_the_ray_strictly_inside_that_slab) {
    CHECK(cast({15, 0}, {15, 30}).hit);
    CHECK(!cast({10, 0}, {10, 30}).hit); // on the left edge line
    CHECK(!cast({9, 0}, {9, 30}).hit);
}

NNH_TEST(boxes_without_area_and_nan_boxes_are_never_hit) {
    CHECK(!cast({0, 15}, {30, 15}, {10, 10, 0, 10}).hit);
    CHECK(!cast({15, 0}, {15, 30}, {10, 10, 10, 0}).hit);
    CHECK(!cast({0, 15}, {30, 15}, {__builtin_nanf(""), 10, 10, 10}).hit);
    CHECK(!cast({0, 15}, {30, 15}, {10, 10, __builtin_nanf(""), 10}).hit);
}

NNH_TEST(a_zero_length_ray_hits_nothing_even_inside) {
    CHECK(!cast({15, 15}, {15, 15}).hit);
}

NNH_TEST(a_hit_exactly_at_the_end_is_not_a_hit_but_just_before_is) {
    CHECK(!cast({0, 15}, {10, 15}).hit);
    CHECK(cast({0, 15}, {10.001f, 15}).hit);
}
