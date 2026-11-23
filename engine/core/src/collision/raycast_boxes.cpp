// Rays against a box view (spec 019 FR-8, FR-10, FR-11): a slab test per box passing the filter, after a reject by
// the segment's bounding box (exact: it only drops boxes the segment can't reach). The reject is integer first
// (order keys, box_view.hpp), before the filter: a box starting at or past the segment's far side never needs a
// float; one starting at or before its near side needs its float sum x + w, as the slab test computes it.
//
// Past the reject the slab test (ray_box.hpp's rules) is entered inline (enters()): the reject already proved what
// slab_cast() would check first (each moving axis enters before t = 1 and leaves after t = 0, a still axis is
// strictly inside its slab), so only the three ratio compares that choose the entry and the exit remain. A first-hit
// ray keeps the best entry and builds its RayHit once, at the end (plan "Console bench refinement (round 2)").

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

/// Where a ray enters a box.
struct Entry {
    detail::Ratio t{};   ///< kRatioZero when inside
    bool face_x = false; ///< entered through an x face (also on a corner tie)
    bool inside = false; ///< the start is strictly inside
};

/// The numerators of the ray's entry and exit parameters in a box's slab on one moving axis (ray_box.cpp's
/// axis_interval; the denominator is the axis' |d|).
void slab_of(double p, int s, double lo, double hi, double& in, double& out) {
    in = s > 0 ? lo - p : p - hi;
    out = s > 0 ? hi - p : p - lo;
}

/// slab_cast(s, slab_x(r), slab_y(r)) for a box past the reject (from 0 < t < 1 and the still axis' check
/// already hold): the later entry (x on a tie) before the earlier exit; inside when it enters before t = 0. In
/// scalars (a Ratio chosen by a condition went through the stack on the 3DS); each test is compare()'s, with its
/// products: compare(p, q) < 0 ⇔ p.n · q.a < q.n · p.a.
bool enters(const detail::Segment& s, const Rect& r, Entry& e) {
    double in_x = 0, out_x = 0, in_y = 0, out_y = 0;
    if (s.sx != 0) slab_of(s.px, s.sx, r.x, static_cast<double>(r.x + r.w), in_x, out_x);
    if (s.sy != 0) slab_of(s.py, s.sy, r.y, static_cast<double>(r.y + r.h), in_y, out_y);
    const double ax = s.ax, ay = s.ay;
    const bool x_enters = s.sx != 0 && (s.sy == 0 || !(in_x * ay < in_y * ax));   // compare(in_x, in_y) >= 0
    const bool x_exits = s.sx != 0 && (s.sy == 0 || !(out_x * ay > out_y * ax));  // compare(out_x, out_y) <= 0
    const double enter_n = x_enters ? in_x : in_y, enter_a = x_enters ? ax : ay;
    const double exit_n = x_exits ? out_x : out_y, exit_a = x_exits ? ax : ay;
    if (!(enter_n * exit_a < exit_n * enter_a)) return false; // compare(enter, exit) < 0
    e.inside = enter_n < 0;
    e.t = e.inside ? detail::kRatioZero : detail::Ratio{enter_n, enter_a};
    e.face_x = x_enters;
    return true;
}

/// The RayHit of entry `e` into box `box` (all but the distance and the index), as the slab test's callers build it.
RayHit hit_of(const detail::Segment& s, const Rect& box, const Entry& e, float fraction) {
    if (e.inside) return detail::inside_hit(s.from);
    RayHit hit;
    hit.fraction = fraction;
    hit.normal = detail::face_normal(s, e.face_x);
    const float x1 = box.x + box.w;
    const float y1 = box.y + box.h;
    if (e.face_x) {
        hit.point = {s.sx > 0 ? box.x : x1, detail::clamp_to(detail::along(s.py, s.dy, e.t), box.y, y1)};
    } else {
        hit.point = {detail::clamp_to(detail::along(s.px, s.dx, e.t), box.x, x1), s.sy > 0 ? box.y : y1};
    }
    return hit;
}

float fraction_of(const Entry& e) { return e.inside ? 0.0f : detail::to_fraction(e.t); }

/// One scan of the view in index order: calls `found(i, box, entry)` for each box the ray enters (starts inside
/// only with `from_inside`); stops when it returns true.
template <typename Found>
void scan(const BoxView& boxes, const detail::Segment& s, const QueryFilter& filter, bool from_inside, Found found) {
    const Bounds b = bounds_of(s.from, s.to);
    const QueryFilter f = filter; // locals: what `found` writes can't alias them
    const int count = boxes.count;
    detail::BoxCursor cursor(boxes);
    detail::BoxBits box;
    Entry e;
    for (int i = 0; i < count; ++i, cursor.next()) {
        if (cursor.x_key() >= b.kx1 || cursor.y_key() >= b.ky1) continue; // x ≥ max x or y ≥ max y
        if (!detail::candidate(cursor, f, i, box) || !reaches(b, box)) continue;
        const Rect r = detail::to_rect(box);
        if (!enters(s, r, e) || (e.inside && !from_inside)) continue;
        if (found(i, r, e)) return;
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
    scan(boxes, s, filter, false, [&any](int, const Rect&, const Entry&) { return any = true; });
    return any;
}

bool first_box_hit(const BoxView& boxes, const Segment& s, RayHit& hit, const QueryFilter& filter) {
    if (zero_length(s)) return false;
    int best = -1;
    Rect best_box{};
    Entry best_entry;
    float best_fraction = 0;
    scan(boxes, s, filter, filter.hit_from_inside, [&](int i, const Rect& r, const Entry& e) {
        // Ties at one fraction go to the lowest index. A later entry that isn't exactly earlier can't round to a
        // smaller fraction (rounding keeps the order), so the division is made only for an exactly earlier one.
        if (best >= 0 && compare(e.t, best_entry.t) >= 0) return false;
        const float fraction = fraction_of(e);
        if (best >= 0 && !(fraction < best_fraction)) return false;
        best = i;
        best_box = r;
        best_entry = e;
        best_fraction = fraction;
        return fraction == 0; // nothing rounds below 0: the rest of the view can't win
    });
    if (best < 0) return false;
    hit = hit_of(s, best_box, best_entry, best_fraction);
    hit.box = best;
    hit.distance = hit.fraction * ray_length(s.from, s.to);
    return true;
}

} // namespace detail

bool raycast(const BoxView& boxes, Vec2 from, Vec2 to, RayHit& hit, const QueryFilter& filter) {
    if (!inputs_ok(boxes, from, to, "raycast")) return false;
    return detail::first_box_hit(boxes, detail::make_segment(from, to), hit, filter);
}

int raycast_all(const BoxView& boxes, Vec2 from, Vec2 to, RayHit* hits, int capacity, const QueryFilter& filter) {
    if (!inputs_ok(boxes, from, to, "raycast_all") || !detail::buffer_ok(hits, capacity, "raycast_all")) return 0;
    const detail::Segment s = detail::make_segment(from, to);
    if (detail::zero_length(s)) return 0;
    const float length = detail::ray_length(from, to);
    int total = 0;
    scan(boxes, s, filter, filter.hit_from_inside, [&](int i, const Rect& r, const Entry& e) {
        RayHit h = hit_of(s, r, e, fraction_of(e));
        h.box = i;
        h.distance = h.fraction * length;
        total = detail::insert_hit(hits, capacity, total, h);
        return false;
    });
    return total;
}

} // namespace nnh::collision
