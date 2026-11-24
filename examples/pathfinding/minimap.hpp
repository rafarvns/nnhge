#pragma once
// The bottom screen (spec 025 AC-4): the whole keep at 6 px per cell (288 x 180 at (16, 30)), runs of equal cells
// drawn as one rectangle. A touch at (tx, ty) is the cell ((tx - 16) / 6, (ty - 30) / 6): integers, no camera.

#include <nnh/gfx.hpp>
#include <nnh/pathfinding.hpp>

#include "level.hpp"

namespace minimap {

inline constexpr int kLeft = 16;
inline constexpr int kTop = 30;
inline constexpr int kCell = 6;

/// The cell under a touch, or false outside the minimap.
bool touched_cell(const Level& level, int tx, int ty, nnh::CellPos& cell);
/// The rectangle of a cell on the bottom screen.
inline nnh::Rect cell_rect(int x, int y) {
    return {static_cast<float>(kLeft + x * kCell), static_cast<float>(kTop + y * kCell), kCell, kCell};
}
/// The level's cells (ground colours, walls, closed doors).
void draw_level(const Level& level);

} // namespace minimap
