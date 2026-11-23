// Box casts against a box view (spec 019 FR-13): each face, flush contact, touching, starting overlap, two movers.

#include "collision/exact.hpp"
#include "nnh/collision.hpp"
#include "test.hpp"

namespace collision = nnh::collision;
using collision::RayHit;
using nnh::Rect;
using nnh::Vec2;

namespace {

const Rect kWall[1] = {{100, 40, 20, 40}}; // x 100..120, y 40..80
collision::BoxView wall() { return collision::box_view(kWall, 1); }

Rect at(Rect box, Vec2 p) { return {p.x, p.y, box.w, box.h}; }

} // namespace

NNH_TEST(a_box_cast_stops_flush_against_each_face) {
    const Rect box{0, 50, 10, 10};
    RayHit hit;
    CHECK(collision::box_cast(wall(), box, {200, 50}, hit)); // right: contact at x 90
    CHECK(hit.box == 0 && hit.point == Vec2(90, 50) && hit.normal == Vec2(-1, 0) && hit.fraction == 0.45f);
    CHECK(hit.distance == 90);
    CHECK(collision::box_cast(wall(), at(box, {200, 50}), {0, 50}, hit)); // left: contact at x 120
    CHECK(hit.point == Vec2(120, 50) && hit.normal == Vec2(1, 0));
    CHECK(collision::box_cast(wall(), at(box, {105, 0}), {105, 100}, hit)); // down: contact at y 30
    CHECK(hit.point == Vec2(105, 30) && hit.normal == Vec2(0, -1));
    CHECK(collision::box_cast(wall(), at(box, {105, 100}), {105, 0}, hit)); // up: contact at y 80
    CHECK(hit.point == Vec2(105, 80) && hit.normal == Vec2(0, 1));
}

NNH_TEST(the_contact_position_is_flush_and_one_ulp_further_overlaps) {
    const Rect box{0.3f, 50.1f, 9.7f, 10};
    RayHit hit;
    CHECK(collision::box_cast(wall(), box, {300.7f, 61.3f}, hit) && hit.normal == Vec2(-1, 0));
    const Rect placed = at(box, hit.point);
    CHECK(!collision::overlaps(placed, kWall[0]));
    CHECK(collision::overlaps(at(box, {collision::detail::step_up(hit.point.x), hit.point.y}), kWall[0]));
    CHECK(collision::box_cast(wall(), at(box, {300.7f, 61.3f}), {0.3f, 50.1f}, hit) && hit.normal == Vec2(1, 0));
    CHECK(!collision::overlaps(at(box, hit.point), kWall[0]));
    CHECK(collision::overlaps(at(box, {collision::detail::step_down(hit.point.x), hit.point.y}), kWall[0]));
}

NNH_TEST(the_contact_stays_flush_when_the_subtraction_rounds_up) {
    // 37.3f − 0.2507f rounds to a float whose sum with 0.2507f is above 37.3f: the contact steps down until flush.
    const nnh::Rect target[1] = {{37.3f, -5, 10, 10}};
    const nnh::Rect box{0, 0, 0.2507f, 1};
    RayHit hit;
    CHECK(collision::box_cast(collision::box_view(target, 1), box, {100, 0}, hit) && hit.normal == Vec2(-1, 0));
    CHECK(hit.point.x + box.w <= target[0].x && !collision::overlaps(at(box, hit.point), target[0]));
}

NNH_TEST(a_cast_into_a_corner_reports_the_x_face) {
    const Rect box{80, 20, 10, 10}; // its bottom-right corner (90, 30) moves to the wall's corner (100, 40)
    RayHit hit;
    CHECK(collision::box_cast(wall(), box, {100, 40}, hit));
    CHECK(hit.normal == Vec2(-1, 0) && hit.point == Vec2(90, 30) && hit.fraction == 0.5f);
}

NNH_TEST(sliding_along_a_face_or_touching_a_corner_is_not_a_hit) {
    RayHit hit;
    CHECK(!collision::box_cast(wall(), {90, 0, 10, 10}, {90, 200}, hit));   // along the left face
    CHECK(!collision::box_cast(wall(), {0, 30, 10, 10}, {300, 30}, hit));   // along the top face
    CHECK(!collision::box_cast(wall(), {70, 50, 10, 10}, {90, 50}, hit));   // ends flush
    CHECK(!collision::box_cast(wall(), {80, 10, 10, 10}, {100, 30}, hit));  // ends flush on the top face
    CHECK(!collision::box_cast(wall(), {80, 40, 10, 10}, {100, 20}, hit));  // its corner only touches the wall's
    CHECK(collision::box_cast(wall(), {70, 10, 10, 10}, {110, 50}, hit));   // corner into corner: it enters
    CHECK(hit.normal == Vec2(-1, 0) && hit.point == Vec2(90, 30));
}

NNH_TEST(boxes_already_overlapped_at_the_start_are_ignored_unless_asked) {
    const Rect box{95, 50, 10, 10};
    RayHit hit;
    CHECK(!collision::box_cast(wall(), box, {300, 50}, hit));
    CHECK(collision::box_cast(wall(), box, {300, 50}, hit, {.hit_from_inside = true}));
    CHECK(hit.inside && hit.fraction == 0 && hit.point == Vec2(95, 50) && hit.normal == Vec2(0, 0));
}

NNH_TEST(a_cast_box_without_area_or_without_motion_touches_nothing) {
    RayHit hit;
    CHECK(!collision::box_cast(wall(), {0, 50, 0, 10}, {200, 50}, hit));
    CHECK(!collision::box_cast(wall(), {95, 50, 10, 10}, {95, 50}, hit, {.hit_from_inside = true}));
}

NNH_TEST(two_moving_boxes_meet_through_their_relative_motion) {
    // A moves right by 30, B moves left by 30, 50 apart: they meet when the gap (40) is closed, at t = 40/60.
    const Rect a{0, 0, 10, 10};
    const Rect b[1] = {{50, 0, 10, 10}};
    const Vec2 move_a{30, 0}, move_b{-30, 0};
    RayHit hit;
    CHECK(collision::box_cast(collision::box_view(b, 1), a, {a.x + move_a.x - move_b.x, a.y}, hit));
    CHECK(hit.fraction == 40.0f / 60 && hit.normal == Vec2(-1, 0));
    // Moving apart: no meeting.
    CHECK(!collision::box_cast(collision::box_view(b, 1), a, {a.x - 30, a.y}, hit));
}

NNH_TEST(all_contacts_are_sorted_with_the_filter_applied) {
    const Rect boxes[3] = {{60, 0, 10, 10}, {30, 0, 10, 10}, {90, 0, 10, 10}};
    const collision::Layers layers[3] = {1u, 2u, 1u};
    RayHit hits[4];
    const collision::BoxView v = collision::box_view(boxes, layers, 3);
    CHECK(collision::box_cast_all(v, {0, 0, 10, 10}, {200, 0}, hits, 4) == 3);
    CHECK(hits[0].box == 1 && hits[1].box == 0 && hits[2].box == 2);
    CHECK(hits[0].point == Vec2(20, 0) && hits[2].point == Vec2(80, 0));
    CHECK(collision::box_cast_all(v, {0, 0, 10, 10}, {200, 0}, hits, 1, {.mask = 1u}) == 2 && hits[0].box == 0);
    CHECK(collision::box_cast_all(v, {0, 0, 10, 10}, {200, 0}, nullptr, 0, {.ignore = 2}) == 2);
}
