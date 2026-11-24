#pragma once
// A copyable debug-draw helper for pathfinding (spec 025 FR-20): the module draws nothing, a game draws what it asks.
// Search mode: expanded cells shaded, the open frontier (reached, not expanded) outlined, the raw path as dots and the
// waypoints joined by lines. Field mode: the distance field as a heat map (near: yellow, far: dark red). The same
// calls draw over the world (top screen, 16-px cells through the camera) and over the minimap (6-px cells).

#include <nnh/gfx.hpp>
#include <nnh/pathfinding.hpp>

namespace debug_view {

enum class Mode { Off, Search, Field };

/// Where cells are drawn: cell (x, y) covers {origin + (x, y) x size, size, size} on `screen`.
struct Surface {
    nnh::Screen screen = nnh::Screen::Top;
    nnh::Vec2 origin{};
    float size = 16;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0; ///< the cells to draw (inclusive), e.g. those on screen
};

/// Expanded cells, the frontier and the path of the searcher's last search (agent size `agent` widens the dots).
void draw_search(const Surface& s, const nnh::PathSearcher& searcher, const nnh::CellPos* path, int count,
                 const nnh::Vec2* waypoints, int waypoint_count, float world_to_surface);
/// The field's distances as colours (unreachable cells left alone).
void draw_field(const Surface& s, const nnh::DistanceField& field);

const char* mode_name(Mode mode);

} // namespace debug_view
