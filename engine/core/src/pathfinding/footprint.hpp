#pragma once
// Agent footprints (spec 025 FR-5): with agent size k > 1 a cell is walkable for the map only if the whole k x k
// square whose top-left it is lies in the map and is walkable in the source bytes; its weight is the square's
// largest. Computed once per change (k^2 reads per affected cell), never per search.

#include "pathfinding/path_block.hpp"

namespace nnh::detail {

/// After the source cells of the inclusive rectangle (x0, y0)..(x1, y1) (inside the map) changed: recomputes every
/// searchable cell whose square overlaps it (the rectangle grown by k - 1 up and left, clipped).
void footprint_refresh(PathMapBlock& b, int x0, int y0, int x1, int y1);

/// Writes `value` (0 blocked, 1..255) to cell (x, y): the searchable byte (agent 1) or the source byte.
inline void write_cell(PathMapBlock& b, int x, int y, std::uint8_t value) {
    if (b.source == nullptr) {
        b.cells[padded_index(b.stride, x, y)] = value;
    } else {
        b.source[y * b.width + x] = value;
    }
}

} // namespace nnh::detail
