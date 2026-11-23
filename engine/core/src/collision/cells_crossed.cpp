// cells_crossed (spec 019 FR-11): the grid walk itself, with each cell's kind and entry fraction. Clipped to the
// grid when `outside` is Empty (as cells_touching); outside cells are listed otherwise.

#include "collision/grid_internal.hpp"
#include "collision/grid_walk.hpp"
#include "collision/query_check.hpp"

namespace nnh::collision {

int cells_crossed(const Grid& grid, Vec2 from, Vec2 to, CellCrossing* cells, int capacity) {
    if (!detail::grid_ok(grid, "cells_crossed") || !detail::buffer_ok(cells, capacity, "cells_crossed") ||
        !detail::point_ok(from, "cells_crossed") || !detail::point_ok(to, "cells_crossed")) {
        return 0;
    }
    detail::GridWalk walk(grid, detail::make_segment(from, to), grid.outside == Cell::Empty);
    detail::WalkCell c;
    int total = 0;
    while (walk.next(c)) {
        if (total < capacity) cells[total] = {c.x, c.y, detail::cell_at(grid, c.x, c.y), detail::to_fraction(c.t)};
        ++total;
    }
    return total;
}

} // namespace nnh::collision
