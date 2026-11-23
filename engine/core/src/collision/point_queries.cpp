// What is under a point, what overlaps a box (spec 019 FR-14, FR-15): one scan of the view / one cell. The scans
// decide contains()/overlaps() on order keys first (box_view.hpp: integers, the cheapest and most selective test),
// then the filter, and make a float sum x + w only for the boxes still open, with the same float operations as
// aabb.cpp.

#include "collision/box_view.hpp"
#include "collision/grid_internal.hpp"
#include "collision/hit_list.hpp"
#include "collision/query_check.hpp"

namespace nnh::collision {

namespace {

float sum_x(const detail::BoxBits& b) { return detail::from_bits(b.x) + detail::from_bits(b.w); }
float sum_y(const detail::BoxBits& b) { return detail::from_bits(b.y) + detail::from_bits(b.h); }

/// The query box of boxes_overlapping (with area) for overlaps(a, b) against each candidate b.
struct Query {
    Rect a;
    std::int32_t kx0, ky0, kx1, ky1; ///< keys of a.x, a.y, a.x + a.w, a.y + a.h
};

/// overlaps(a, b) for a candidate b whose x, y keys are below a's far sides: a.x < b.x + b.w and a.y < b.y + b.h.
/// When b.x > a.x, b.x + b.w ≥ b.x > a.x (b.w > 0, round to nearest), so a sum is only made when b.x ≤ a.x.
bool reaches(const Query& q, const detail::BoxBits& b) {
    if (detail::order_key(b.x) <= q.kx0 && !(q.a.x < sum_x(b))) return false;
    return detail::order_key(b.y) > q.ky0 || q.a.y < sum_y(b);
}

} // namespace

int boxes_at(const BoxView& boxes, Vec2 point, int* indices, int capacity, const QueryFilter& filter) {
    if (!detail::view_ok(boxes, "boxes_at") || !detail::buffer_ok(indices, capacity, "boxes_at") ||
        !detail::point_ok(point, "boxes_at")) {
        return 0;
    }
    const std::int32_t kx = detail::order_key(detail::to_bits(point.x));
    const std::int32_t ky = detail::order_key(detail::to_bits(point.y));
    const QueryFilter f = filter; // locals: the stores into `indices` can't alias them
    const int count = boxes.count;
    int total = 0;
    detail::BoxCursor cursor(boxes);
    detail::BoxBits box;
    for (int i = 0; i < count; ++i, cursor.next()) {
        if (cursor.x_key() > kx || cursor.y_key() > ky) continue; // contains(): x ≤ p.x, y ≤ p.y
        if (detail::candidate(cursor, f, i, box) && point.x < sum_x(box) && point.y < sum_y(box)) {
            total = detail::append_index(indices, capacity, total, i);
        }
    }
    return total;
}

int boxes_overlapping(const BoxView& boxes, Rect box, int* indices, int capacity, const QueryFilter& filter) {
    if (!detail::view_ok(boxes, "boxes_overlapping") ||
        !detail::buffer_ok(indices, capacity, "boxes_overlapping") ||
        !detail::rect_ok(box, "boxes_overlapping")) {
        return 0;
    }
    if (!(box.w > 0 && box.h > 0)) return 0; // overlaps() is false for every box
    const Query q{box, detail::order_key(detail::to_bits(box.x)), detail::order_key(detail::to_bits(box.y)),
                  detail::order_key(detail::to_bits(box.x + box.w)),
                  detail::order_key(detail::to_bits(box.y + box.h))};
    const QueryFilter f = filter;
    const int count = boxes.count;
    int total = 0;
    detail::BoxCursor cursor(boxes);
    detail::BoxBits other;
    for (int i = 0; i < count; ++i, cursor.next()) {
        if (cursor.x_key() >= q.kx1 || cursor.y_key() >= q.ky1) continue; // overlaps(): b.x < a.x + a.w, ...
        if (detail::candidate(cursor, f, i, other) && reaches(q, other)) {
            total = detail::append_index(indices, capacity, total, i);
        }
    }
    return total;
}

CellHit cell_at(const Grid& grid, Vec2 point) {
    if (!detail::grid_ok(grid, "cell_at") || !detail::point_ok(point, "cell_at")) return {};
    const int x = detail::first_cell(grid.origin.x, grid.cell_w, point.x);
    const int y = detail::first_cell(grid.origin.y, grid.cell_h, point.y);
    return {x, y, detail::cell_at(grid, x, y)};
}

} // namespace nnh::collision
