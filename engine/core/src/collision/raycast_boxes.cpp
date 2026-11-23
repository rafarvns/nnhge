// Rays against a box view (spec 019 FR-8, FR-10, FR-11): a slab test per box passing the filter, after a reject by
// the segment's bounding box (exact: it only drops boxes the segment can't reach). The reject is integer first
// (order keys, box_view.hpp), before the filter: a box starting at or past the segment's far side never needs a
// float; one starting at or before its near side needs its float sum x + w, as the slab test computes it.

#include "collision/box_view.hpp"
#include "collision/hit_list.hpp"
#include "collision/query_check.hpp"
#include "collision/ray_box.hpp"
#include "collision/ray_hit.hpp"

namespace nnh::collision {

namespace {

float min_of(float a, float b) { return a < b ? a : b; }
float max_of(float a, float b) { return a > b ? a : b; }

/// The segment's bounding box: its low sides as floats, all four sides as order keys.
struct Bounds {
    float x0 = 0, y0 = 0;
    std::int32_t kx0 = 0, ky0 = 0, kx1 = 0, ky1 = 0;
};

Bounds bounds_of(Vec2 from, Vec2 to) {
    Bounds b;
    b.x0 = min_of(from.x, to.x);
    b.y0 = min_of(from.y, to.y);
    b.kx0 = detail::order_key(detail::to_bits(b.x0));
    b.ky0 = detail::order_key(detail::to_bits(b.y0));
    b.kx1 = detail::order_key(detail::to_bits(max_of(from.x, to.x)));
    b.ky1 = detail::order_key(detail::to_bits(max_of(from.y, to.y)));
    return b;
}

/// The rest of the bounding-box reject, for a candidate whose x, y keys are below the segment's high sides: false
/// when x + w ≤ min x or y + h ≤ min y. x > min x already means x + w > min x (w > 0, round to nearest), so a sum is
/// only made for a box starting at or before the low side.
bool reaches(const Bounds& s, const detail::BoxBits& box) {
    if (detail::order_key(box.x) <= s.kx0 && !(detail::from_bits(box.x) + detail::from_bits(box.w) > s.x0)) {
        return false;
    }
    return detail::order_key(box.y) > s.ky0 || detail::from_bits(box.y) + detail::from_bits(box.h) > s.y0;
}

/// Box `box` (past may_reach) hit by from → to: fills everything but the distance and the index.
bool box_hit(const detail::Segment& s, Vec2 from, Rect box, bool from_inside, RayHit& hit) {
    const detail::SlabHit h = detail::slab_cast(s, detail::slab_x(box), detail::slab_y(box));
    if (!h.hit || (h.inside && !from_inside)) return false;
    if (h.inside) {
        hit = detail::inside_hit(from);
        return true;
    }
    const float x1 = box.x + box.w;
    const float y1 = box.y + box.h;
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

/// One scan of the view in index order: calls `found(i, hit)` for each box the ray hits; stops when it returns
/// true.
template <typename Found>
void scan(const BoxView& boxes, const detail::Segment& s, Vec2 from, Vec2 to, const QueryFilter& filter,
          bool from_inside, Found found) {
    const Bounds b = bounds_of(from, to);
    const QueryFilter f = filter; // locals: what `found` writes can't alias them
    const int count = boxes.count;
    detail::BoxCursor cursor(boxes);
    detail::BoxBits box;
    RayHit hit;
    for (int i = 0; i < count; ++i, cursor.next()) {
        if (cursor.x_key() >= b.kx1 || cursor.y_key() >= b.ky1) continue; // x ≥ max x or y ≥ max y
        if (!detail::candidate(cursor, f, i, box) || !reaches(b, box) ||
            !box_hit(s, from, detail::to_rect(box), from_inside, hit)) {
            continue;
        }
        if (found(i, hit)) return;
    }
}

bool inputs_ok(const BoxView& boxes, Vec2 from, Vec2 to, const char* who) {
    return detail::view_ok(boxes, who) && detail::point_ok(from, who) && detail::point_ok(to, who);
}

} // namespace

namespace detail {

bool any_box_on_ray(const BoxView& boxes, Vec2 from, Vec2 to, const QueryFilter& filter) {
    const Segment s = make_segment(from, to);
    if (zero_length(s)) return false;
    bool any = false;
    scan(boxes, s, from, to, filter, false, [&any](int, const RayHit&) { return any = true; });
    return any;
}

} // namespace detail

bool raycast(const BoxView& boxes, Vec2 from, Vec2 to, RayHit& hit, const QueryFilter& filter) {
    if (!inputs_ok(boxes, from, to, "raycast")) return false;
    const detail::Segment s = detail::make_segment(from, to);
    if (detail::zero_length(s)) return false;
    bool found = false;
    scan(boxes, s, from, to, filter, filter.hit_from_inside, [&](int i, const RayHit& candidate) {
        if (found && !(candidate.fraction < hit.fraction)) return false; // ties: the lowest index
        hit = candidate;
        hit.box = i;
        found = true;
        return false;
    });
    if (found) hit.distance = hit.fraction * detail::ray_length(from, to);
    return found;
}

int raycast_all(const BoxView& boxes, Vec2 from, Vec2 to, RayHit* hits, int capacity, const QueryFilter& filter) {
    if (!inputs_ok(boxes, from, to, "raycast_all") || !detail::buffer_ok(hits, capacity, "raycast_all")) return 0;
    const detail::Segment s = detail::make_segment(from, to);
    if (detail::zero_length(s)) return 0;
    const float length = detail::ray_length(from, to);
    int total = 0;
    scan(boxes, s, from, to, filter, filter.hit_from_inside, [&](int i, const RayHit& candidate) {
        RayHit h = candidate;
        h.box = i;
        h.distance = h.fraction * length;
        total = detail::insert_hit(hits, capacity, total, h);
        return false;
    });
    return total;
}

} // namespace nnh::collision
