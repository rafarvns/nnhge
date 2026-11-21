// Grid queries without moving (spec 012 FR-8, FR-9): contacts and cells_touching.

#include "collision/grid_internal.hpp"
#include "nnh/assert.hpp"

namespace nnh::collision {

namespace {

// A side "touches" when a blocking cell edge is this close: a tiny probe move, so contacts and move agree.
// 1/64 px stays above float spacing for worlds up to 2^17 px wide.
constexpr float kContactDistance = 1.0f / 64;

bool valid(const Grid& grid) { return grid.cell_w > 0 && grid.cell_h > 0; }

int max_of(int a, int b) { return a > b ? a : b; }
int min_of(int a, int b) { return a < b ? a : b; }

} // namespace

Sides contacts(const Grid& grid, Rect box) {
    NNH_ASSERT(valid(grid), "collision::contacts: cell size must be > 0");
    Sides s;
    if (!valid(grid)) return s;
    using detail::Axis;
    s.left = detail::sweep(grid, box, Axis::X, -kContactDistance, false).blocked;
    s.right = detail::sweep(grid, box, Axis::X, kContactDistance, false).blocked;
    s.ceiling = detail::sweep(grid, box, Axis::Y, -kContactDistance, false).blocked;
    s.floor = detail::sweep(grid, box, Axis::Y, kContactDistance, true).blocked;
    return s;
}

int cells_touching(const Grid& grid, Rect box, CellHit* out, int capacity) {
    NNH_ASSERT(valid(grid), "collision::cells_touching: cell size must be > 0");
    NNH_ASSERT(capacity >= 0 && (out != nullptr || capacity == 0), "cells_touching: bad output %d", capacity);
    if (!valid(grid) || out == nullptr) capacity = 0;
    detail::CellRange r = detail::cell_range(grid, box);
    if (grid.outside == Cell::Empty) { // nothing to find beyond the edges: visit only the grid
        r = {max_of(r.x0, 0), max_of(r.y0, 0), min_of(r.x1, grid.width - 1), min_of(r.y1, grid.height - 1)};
    }
    if (!valid(grid) || r.empty()) return 0;
    int total = 0;
    for (int y = r.y0; y <= r.y1; ++y) {
        for (int x = r.x0; x <= r.x1; ++x) {
            const Cell kind = detail::cell_at(grid, x, y);
            if (kind == Cell::Empty) continue;
            if (total < capacity) out[total] = {x, y, kind};
            ++total;
        }
    }
    return total;
}

} // namespace nnh::collision
