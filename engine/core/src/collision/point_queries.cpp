// What is under a point, what overlaps a box (spec 019 FR-14, FR-15): one scan of the view / one cell.

#include "collision/box_view.hpp"
#include "collision/grid_internal.hpp"
#include "collision/hit_list.hpp"
#include "collision/query_check.hpp"

namespace nnh::collision {

int boxes_at(const BoxView& boxes, Vec2 point, int* indices, int capacity, const QueryFilter& filter) {
    if (!detail::view_ok(boxes, "boxes_at") || !detail::buffer_ok(indices, capacity, "boxes_at") ||
        !detail::point_ok(point, "boxes_at")) {
        return 0;
    }
    int total = 0;
    Rect box;
    for (int i = 0; i < boxes.count; ++i) {
        if (detail::passes(boxes, filter, i, box) && contains(box, point)) {
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
    int total = 0;
    Rect other;
    for (int i = 0; i < boxes.count; ++i) {
        if (detail::passes(boxes, filter, i, other) && overlaps(box, other)) {
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
