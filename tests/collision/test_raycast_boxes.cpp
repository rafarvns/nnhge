// Rays against a box view (spec 019 FR-8, FR-10, FR-11): hit fields, filter, inside starts, first and all hits.

#include "nnh/collision.hpp"
#include "test.hpp"

namespace collision = nnh::collision;
using collision::Layers;
using collision::RayHit;
using nnh::Rect;
using nnh::Vec2;

namespace {

// Three boxes on a row along y 10..20: 0 at x 10..20, 1 at 40..50, 2 at 70..80; 3 overlaps 1 (x 45..55).
const Rect kBoxes[4] = {{10, 10, 10, 10}, {40, 10, 10, 10}, {70, 10, 10, 10}, {45, 12, 10, 4}};
const Layers kLayers[4] = {1u, 2u, 1u, 4u};

collision::BoxView view() { return collision::box_view(kBoxes, kLayers, 4); }

} // namespace

NNH_TEST(a_ray_reports_the_first_box_with_point_normal_fraction_and_distance) {
    RayHit hit;
    CHECK(collision::raycast(view(), {0, 15}, {100, 15}, hit));
    CHECK(hit.box == 0 && hit.fraction == 0.1f && hit.distance == 10);
    CHECK(hit.point == Vec2(10, 15) && hit.normal == Vec2(-1, 0) && !hit.inside);
}

NNH_TEST(each_face_has_its_normal_and_a_flush_point) {
    const Rect box[1] = {{10, 10, 10, 10}};
    const collision::BoxView v = collision::box_view(box, 1);
    RayHit hit;
    CHECK(collision::raycast(v, {30, 13}, {0, 13}, hit) && hit.normal == Vec2(1, 0) && hit.point == Vec2(20, 13));
    CHECK(collision::raycast(v, {12, 0}, {12, 30}, hit) && hit.normal == Vec2(0, -1) && hit.point == Vec2(12, 10));
    CHECK(collision::raycast(v, {17, 30}, {17, 0}, hit) && hit.normal == Vec2(0, 1) && hit.point == Vec2(17, 20));
    // A slanted ray: the face coordinate is exact, the other one rounded once.
    CHECK(collision::raycast(v, {0, 11}, {30, 18}, hit) && hit.normal == Vec2(-1, 0));
    CHECK(hit.point.x == 10 && hit.point.y > 13.33f && hit.point.y < 13.34f);
    CHECK(collision::raycast(v, {0, 0}, {30, 20}, hit) && hit.normal == Vec2(0, -1) && hit.point == Vec2(15, 10));
    CHECK(!collision::raycast(v, {0, 0}, {30, 6}, hit)); // passes above
}

NNH_TEST(a_ray_through_a_box_corner_reports_the_x_face) {
    const Rect box[1] = {{10, 10, 10, 10}};
    RayHit hit;
    CHECK(collision::raycast(collision::box_view(box, 1), {0, 0}, {20, 20}, hit));
    CHECK(hit.normal == Vec2(-1, 0) && hit.point == Vec2(10, 10) && hit.fraction == 0.5f);
}

NNH_TEST(the_distance_is_the_fraction_times_the_length) {
    const Rect box[1] = {{20, 40, 20, 10}};
    RayHit hit;
    // 3-4-5: from (0, 0) to (60, 80), length 100; enters the top face y = 40 at t 0.5 (x = 20 was at t 1/3).
    CHECK(collision::raycast(collision::box_view(box, 1), {0, 0}, {60, 80}, hit));
    CHECK(hit.fraction == 0.5f && hit.distance == 50 && hit.point == Vec2(30, 40) && hit.normal == Vec2(0, -1));
}

NNH_TEST(the_mask_and_the_ignore_index_skip_boxes) {
    RayHit hit;
    CHECK(collision::raycast(view(), {0, 15}, {100, 15}, hit, {.mask = 2u}) && hit.box == 1);
    CHECK(collision::raycast(view(), {0, 15}, {100, 15}, hit, {.ignore = 0}) && hit.box == 1);
    CHECK(collision::raycast(view(), {0, 15}, {100, 15}, hit, {.mask = 1u, .ignore = 0}) && hit.box == 2);
    CHECK(!collision::raycast(view(), {0, 15}, {100, 15}, hit, {.mask = 8u}));
}

NNH_TEST(boxes_containing_the_start_are_ignored_unless_asked) {
    RayHit hit;
    CHECK(collision::raycast(view(), {15, 15}, {100, 15}, hit) && hit.box == 1);
    CHECK(collision::raycast(view(), {15, 15}, {100, 15}, hit, {.hit_from_inside = true}));
    CHECK(hit.box == 0 && hit.inside && hit.fraction == 0 && hit.distance == 0);
    CHECK(hit.point == Vec2(15, 15) && hit.normal == Vec2(0, 0));
}

NNH_TEST(ties_at_one_fraction_go_to_the_lowest_index) {
    const Rect boxes[3] = {{20, 0, 5, 10}, {10, 5, 5, 10}, {10, 0, 5, 10}}; // 1 and 2: left face at x = 10
    RayHit hit;
    CHECK(collision::raycast(collision::box_view(boxes, 3), {0, 7}, {30, 7}, hit));
    CHECK(hit.box == 1 && hit.point == Vec2(10, 7));
    CHECK(collision::raycast(collision::box_view(boxes, 3), {0, 2}, {30, 2}, hit) && hit.box == 2);
}

NNH_TEST(all_hits_are_sorted_by_fraction_with_their_indices) {
    RayHit hits[8];
    const int n = collision::raycast_all(view(), {100, 14}, {0, 14}, hits, 8);
    CHECK(n == 4);
    CHECK(hits[0].box == 2 && hits[1].box == 3 && hits[2].box == 1 && hits[3].box == 0);
    CHECK(hits[1].point == Vec2(55, 14) && hits[2].point == Vec2(50, 14));
    CHECK(hits[3].distance == 80 && hits[3].normal == Vec2(1, 0));
    for (int i = 1; i < n; ++i) CHECK(hits[i - 1].fraction <= hits[i].fraction);
}

NNH_TEST(all_hits_keep_the_nearest_when_the_buffer_is_short_and_count_with_null) {
    RayHit hits[2];
    CHECK(collision::raycast_all(view(), {0, 14}, {100, 14}, hits, 2) == 4);
    CHECK(hits[0].box == 0 && hits[1].box == 1);
    CHECK(collision::raycast_all(view(), {0, 14}, {100, 14}, nullptr, 0) == 4);
    CHECK(collision::raycast_all(view(), {0, 14}, {100, 14}, hits, 2, {.mask = 1u}) == 2);
}

NNH_TEST(all_hits_include_the_start_box_only_when_asked) {
    RayHit hits[4];
    CHECK(collision::raycast_all(view(), {47, 14}, {100, 14}, hits, 4) == 1 && hits[0].box == 2);
    CHECK(collision::raycast_all(view(), {47, 14}, {100, 14}, hits, 4, {.hit_from_inside = true}) == 3);
    CHECK(hits[0].box == 1 && hits[1].box == 3 && hits[0].inside && hits[1].inside && hits[2].box == 2);
}

NNH_TEST(zero_length_rays_and_empty_views_hit_nothing) {
    RayHit hit;
    CHECK(!collision::raycast(view(), {15, 15}, {15, 15}, hit, {.hit_from_inside = true}));
    CHECK(!collision::raycast(collision::BoxView{}, {0, 15}, {100, 15}, hit));
    CHECK(collision::raycast_all(collision::BoxView{}, {0, 15}, {100, 15}, nullptr, 0) == 0);
}
