// Box casts against a box view (spec 019 FR-13): the cast box's top-left as a ray against each target grown by the
// cast box's size, the numerators in double from the three floats (no rounded grown box). The contact position is
// placed flush with float sums as overlaps() makes them, stepping down from the exact value as move() does. Targets
// out of the cast's reach are dropped first on order keys (box_view.hpp), as rays do.

#include "collision/box_view.hpp"
#include "collision/hit_list.hpp"
#include "collision/query_check.hpp"
#include "collision/ray_box.hpp"
#include "collision/ray_hit.hpp"

namespace nnh::collision {

namespace {

float abs_of(float v) { return v < 0 ? -v : v; }

/// The largest start `p` found below `wall − len` whose far end p + len (in float) stays at or before `wall`:
/// steps start at one ulp of the largest magnitude and double, as move()'s place_before.
float place_before(float wall, float len) {
    float p = wall - len;
    float m = abs_of(wall) > abs_of(p) ? abs_of(wall) : abs_of(p);
    if (len > m) m = len;
    float step = detail::step_up(m) - m;
    for (int k = 0; k < 64 && p + len > wall; ++k, step *= 2) p -= step;
    return p;
}

/// The cast box's position on one axis at contact with the target's face [lo, hi] (as floats), moving by `s`,
/// kept between the start and the end of the cast.
float flush(float start, float end, float lo, float hi, float len, int s) {
    float p = s > 0 ? place_before(lo, len) : hi;
    const float a = start < end ? start : end;
    const float b = start < end ? end : start;
    return detail::clamp_to(p, a, b);
}

bool cast_hit(const detail::Segment& s, Rect box, Vec2 to, Rect target, bool from_inside, RayHit& hit) {
    const float x1 = target.x + target.w;
    const float y1 = target.y + target.h;
    const detail::Slab sx{static_cast<double>(target.x) - box.w, x1};
    const detail::Slab sy{static_cast<double>(target.y) - box.h, y1};
    const detail::SlabHit h = detail::slab_cast(s, sx, sy);
    if (!h.hit || (h.inside && !from_inside)) return false;
    if (h.inside) {
        hit = detail::inside_hit({box.x, box.y});
        return true;
    }
    hit = {};
    hit.fraction = detail::to_fraction(h.t);
    hit.normal = detail::face_normal(s, h.face_x);
    if (h.face_x) {
        hit.point = {flush(box.x, to.x, target.x, x1, box.w, s.sx), detail::along(s.py, s.dy, h.t)};
    } else {
        hit.point = {detail::along(s.px, s.dx, h.t), flush(box.y, to.y, target.y, y1, box.h, s.sy)};
    }
    return true;
}

/// Where the cast box's top-left goes: its lowest x, y (floats and keys) and the keys of its highest x, y plus the
/// cast box's size, rounded to float. A target whose x key is above that rounded reach starts beyond the exact one,
/// so its grown slab's low side x − w is past every position of the top-left.
struct Reach {
    float x0 = 0, y0 = 0;
    std::int32_t kx0 = 0, ky0 = 0, kx1 = 0, ky1 = 0;
};

Reach reach_of(Rect box, Vec2 to) {
    Reach r;
    r.x0 = box.x < to.x ? box.x : to.x;
    r.y0 = box.y < to.y ? box.y : to.y;
    r.kx0 = detail::order_key(detail::to_bits(r.x0));
    r.ky0 = detail::order_key(detail::to_bits(r.y0));
    r.kx1 = detail::order_key(detail::to_bits((box.x > to.x ? box.x : to.x) + box.w));
    r.ky1 = detail::order_key(detail::to_bits((box.y > to.y ? box.y : to.y) + box.h));
    return r;
}

/// The target under the cursor starts beyond the reach (its keys alone: the scans' first test).
bool beyond(const Reach& r, const detail::BoxCursor& c) { return c.x_key() > r.kx1 || c.y_key() > r.ky1; }

/// False when the candidate ends (x + w as the slab test sums it) at or before the top-left's lowest position.
/// x > that position already means x + w > it (w > 0, round to nearest).
bool reaches(const Reach& r, const detail::BoxBits& t) {
    if (detail::order_key(t.x) <= r.kx0 && !(detail::from_bits(t.x) + detail::from_bits(t.w) > r.x0)) return false;
    return detail::order_key(t.y) > r.ky0 || detail::from_bits(t.y) + detail::from_bits(t.h) > r.y0;
}

/// Checks the inputs; false (nothing to cast) also for a cast box without area or a zero-length cast.
bool cast_ok(const BoxView& boxes, Rect box, Vec2 to, const char* who, detail::Segment& s) {
    if (!detail::view_ok(boxes, who) || !detail::rect_ok(box, who) || !detail::point_ok(to, who)) return false;
    s = detail::make_segment({box.x, box.y}, to);
    return box.w > 0 && box.h > 0 && !detail::zero_length(s);
}

} // namespace

bool box_cast(const BoxView& boxes, Rect box, Vec2 to, RayHit& hit, const QueryFilter& filter) {
    detail::Segment s;
    if (!cast_ok(boxes, box, to, "box_cast", s)) return false;
    bool found = false;
    RayHit candidate;
    const Reach reach = reach_of(box, to);
    const QueryFilter f = filter; // locals: the stores into `hit` can't alias them
    const int count = boxes.count;
    detail::BoxCursor cursor(boxes);
    detail::BoxBits target;
    for (int i = 0; i < count; ++i, cursor.next()) {
        if (beyond(reach, cursor) || !detail::candidate(cursor, f, i, target) || !reaches(reach, target) ||
            !cast_hit(s, box, to, detail::to_rect(target), f.hit_from_inside, candidate)) {
            continue;
        }
        if (found && !(candidate.fraction < hit.fraction)) continue; // ties: the lowest index
        candidate.box = i;
        hit = candidate;
        found = true;
    }
    if (found) hit.distance = hit.fraction * detail::ray_length({box.x, box.y}, to);
    return found;
}

int box_cast_all(const BoxView& boxes, Rect box, Vec2 to, RayHit* hits, int capacity, const QueryFilter& filter) {
    detail::Segment s;
    if (!detail::buffer_ok(hits, capacity, "box_cast_all") || !cast_ok(boxes, box, to, "box_cast_all", s)) return 0;
    const float length = detail::ray_length({box.x, box.y}, to);
    int total = 0;
    RayHit candidate;
    const Reach reach = reach_of(box, to);
    const QueryFilter f = filter;
    const int count = boxes.count;
    detail::BoxCursor cursor(boxes);
    detail::BoxBits target;
    for (int i = 0; i < count; ++i, cursor.next()) {
        if (beyond(reach, cursor) || !detail::candidate(cursor, f, i, target) || !reaches(reach, target) ||
            !cast_hit(s, box, to, detail::to_rect(target), f.hit_from_inside, candidate)) {
            continue;
        }
        candidate.box = i;
        candidate.distance = candidate.fraction * length;
        total = detail::insert_hit(hits, capacity, total, candidate);
    }
    return total;
}

} // namespace nnh::collision
