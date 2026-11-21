#pragma once
// Grid helpers shared by move, contacts and cells_touching (spec 012). Pure: no engine state.

#include "nnh/collision.hpp"

namespace nnh::collision::detail {

/// Cell indices are clamped to +-kMaxCellIndex so huge coordinates never overflow `int`, and loops
/// can step one past them.
inline constexpr int kMaxCellIndex = 1 << 30;

/// An inclusive range of cells; empty when x1 < x0 or y1 < y0.
struct CellRange {
    int x0 = 0, y0 = 0, x1 = -1, y1 = -1;
    constexpr bool empty() const { return x1 < x0 || y1 < y0; }
};

/// Index of the cell holding coordinate `v`: floor((v - origin) / size), saturated.
int first_cell(float origin, float size, float v);
/// Index of the last cell an edge ending at `end` covers (half-open): ceil((end - origin) / size) - 1.
int last_cell(float origin, float size, float end);
/// Cells `box` overlaps (shared area > 0); empty for a box without area.
CellRange cell_range(const Grid& grid, Rect box);
/// The kind of cell (x, y): `grid.outside` beyond the edges (or when the grid has no cells).
Cell cell_at(const Grid& grid, int x, int y);

enum class Axis : unsigned char { X, Y };

struct SweepResult {
    float position = 0;   ///< the box's new x (Axis::X) or y (Axis::Y)
    bool blocked = false; ///< stopped by a cell before going the whole way
};

/// Moves `box` along one axis by `delta` (finite, non-zero), stopping before the first blocking cell the
/// box enters. Cells it already overlaps never block. Solid always blocks; with `one_way`, a one-way cell
/// blocks when its top is at or below the box's starting bottom (only meaningful for a downward Y sweep).
SweepResult sweep(const Grid& grid, Rect box, Axis axis, float delta, bool one_way);

} // namespace nnh::collision::detail
