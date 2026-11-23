// Grids the game describes (spec 012 FR-8): grid_view, cell lookup, cell index of a coordinate.

#include <cmath>

#include "collision/grid_internal.hpp"
#include "nnh/assert.hpp"

namespace nnh::collision {

Grid grid_view(const Cell* cells, int width, int height, float cell_w, float cell_h, Vec2 origin) {
    NNH_ASSERT(width >= 0 && height >= 0, "grid_view: negative size %dx%d", width, height);
    NNH_ASSERT(cells != nullptr || width == 0 || height == 0, "grid_view: null cells for %dx%d", width,
               height);
    NNH_ASSERT(cell_w > 0 && cell_h > 0, "grid_view: cell size must be > 0");
    Grid grid;
    grid.origin = origin;
    grid.cell_w = cell_w;
    grid.cell_h = cell_h;
    grid.width = width;
    grid.height = height;
    grid.cells = cells;
    return grid;
}

namespace detail {

namespace {

constexpr float kMax = static_cast<float>(kMaxCellIndex);

// Saturating float -> int; NaN (only reachable in Release, after an assert) maps to -kMax.
int to_index(float i) {
    if (!(i > -kMax)) return -kMaxCellIndex;
    if (i > kMax) return kMaxCellIndex;
    return static_cast<int>(i);
}

} // namespace

int first_cell(float origin, float size, float v) { return to_index(std::floor((v - origin) / size)); }

int last_cell(float origin, float size, float end) { return to_index(std::ceil((end - origin) / size) - 1); }

CellRange cell_range(const Grid& grid, Rect box) {
    if (!(box.w > 0 && box.h > 0)) return {};
    return {first_cell(grid.origin.x, grid.cell_w, box.x), first_cell(grid.origin.y, grid.cell_h, box.y),
            last_cell(grid.origin.x, grid.cell_w, box.x + box.w),
            last_cell(grid.origin.y, grid.cell_h, box.y + box.h)};
}

} // namespace detail

} // namespace nnh::collision
