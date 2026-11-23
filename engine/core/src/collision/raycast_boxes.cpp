// Rays against a box view (spec 019 FR-8, FR-10, FR-11): a slab test per box passing the filter, after a float
// reject by the segment's bounding box (exact: it only drops boxes the segment can't reach).

#include "collision/box_view.hpp"
#include "collision/hit_list.hpp"
#include "collision/query_check.hpp"
#include "collision/ray_box.hpp"
#include "collision/ray_hit.hpp"

namespace nnh::collision {

namespace {

float min_of(float a, float b) { return a < b ? a : b; }
float max_of(float a, float b) { return a > b ? a : b; }

/// Box `box` hit by from → to: fills everything but the distance and the index.
bool box_hit(const detail::Segment& s, Vec2 from, Vec2 to, Rect box, bool from_inside, RayHit& hit) {
    const float x1 = box.x + box.w;
    const float y1 = box.y + box.h;
    if (x1 <= min_of(from.x, to.x) || box.x >= max_of(from.x, to.x) || y1 <= min_of(from.y, to.y) ||
        box.y >= max_of(from.y, to.y)) {
        return false;
    }
    const detail::SlabHit h = detail::slab_cast(s, detail::slab_x(box), detail::slab_y(box));
    if (!h.hit || (h.inside && !from_inside)) return false;
    if (h.inside) {
        hit = detail::inside_hit(from);
        return true;
    }
    hit = {};
    hit.fraction = detail::to_fraction(h.t);
    hit.normal = detail::face_normal(s, h.face_x);
    if (h.face_x) {
        hit.point = {s.sx > 0 ? box.x : x1, detail::clamp_to(detail::along(s.py, s.dy, h.t), box.y, y1)};
    } else {
        hit.point = {detail::clamp_to(detail::along(s.px, s.dx, h.t), box.x, x1), s.sy > 0 ? box.y : y1};
    }
    return true;
}

bool inputs_ok(const BoxView& boxes, Vec2 from, Vec2 to, const char* who) {
    return detail::view_ok(boxes, who) && detail::point_ok(from, who) && detail::point_ok(to, who);
}

} // namespace

namespace detail {

bool any_box_on_ray(const BoxView& boxes, Vec2 from, Vec2 to, const QueryFilter& filter) {
    const Segment s = make_segment(from, to);
    if (zero_length(s)) return false;
    Rect box;
    RayHit hit;
    for (int i = 0; i < boxes.count; ++i) {
        if (passes(boxes, filter, i, box) && box_hit(s, from, to, box, false, hit)) return true;
    }
    return false;
}

} // namespace detail

bool raycast(const BoxView& boxes, Vec2 from, Vec2 to, RayHit& hit, const QueryFilter& filter) {
    if (!inputs_ok(boxes, from, to, "raycast")) return false;
    const detail::Segment s = detail::make_segment(from, to);
    if (detail::zero_length(s)) return false;
    bool found = false;
    RayHit candidate;
    Rect box;
    for (int i = 0; i < boxes.count; ++i) {
        if (!detail::passes(boxes, filter, i, box) || !box_hit(s, from, to, box, filter.hit_from_inside, candidate)) {
            continue;
        }
        if (found && !(candidate.fraction < hit.fraction)) continue; // ties: the lowest index
        candidate.box = i;
        hit = candidate;
        found = true;
    }
    if (found) hit.distance = hit.fraction * detail::ray_length(from, to);
    return found;
}

int raycast_all(const BoxView& boxes, Vec2 from, Vec2 to, RayHit* hits, int capacity, const QueryFilter& filter) {
    if (!inputs_ok(boxes, from, to, "raycast_all") || !detail::buffer_ok(hits, capacity, "raycast_all")) return 0;
    const detail::Segment s = detail::make_segment(from, to);
    if (detail::zero_length(s)) return 0;
    const float length = detail::ray_length(from, to);
    int total = 0;
    RayHit candidate;
    Rect box;
    for (int i = 0; i < boxes.count; ++i) {
        if (!detail::passes(boxes, filter, i, box) || !box_hit(s, from, to, box, filter.hit_from_inside, candidate)) {
            continue;
        }
        candidate.box = i;
        candidate.distance = candidate.fraction * length;
        total = detail::insert_hit(hits, capacity, total, candidate);
    }
    return total;
}

} // namespace nnh::collision
