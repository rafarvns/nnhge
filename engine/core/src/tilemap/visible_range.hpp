#pragma once
// Where a tile layer lands on a screen and which of its cells the screen shows (spec 012 FR-3, decision
// D26). Pure; unit tested in tests/tilemap/test_visible_range.cpp.
//
// Parallax follows Tiled's preview, which scrolls around the view's centre and the map's parallax origin:
// screen = world + offset + (camera + screen_size/2 − parallax_origin)·(1 − parallax) − camera.
// The screen position of world (0, 0) is snapped to whole pixels once (halves up, like sprites), so every
// tile is an exact multiple of the tile size from it: no seams, and PC and 3DS agree.

#include "nnh/math.hpp"

namespace nnh::detail {

struct LayerPlacement {
    Vec2 camera{};          ///< world point at the screen's top-left (zero when ignoring the camera)
    Vec2 screen_size{};     ///< pixels
    Vec2 offset{};          ///< the layer's offset plus the draw's
    Vec2 parallax{1, 1};    ///< 1 = scrolls with the camera, 0 = fixed on screen
    Vec2 parallax_origin{}; ///< the map's
};

struct LayerView {
    int origin_x = 0, origin_y = 0; ///< screen pixel of cell (0, 0)'s top-left corner
    int x0 = 0, y0 = 0;             ///< first visible cell
    int x1 = 0, y1 = 0;             ///< one past the last visible cell (clamped to the map)
    bool empty() const { return x0 >= x1 || y0 >= y1; }
};

/// The view of a `map_w` × `map_h` layer of `tile_w` × `tile_h` tiles. Non-finite or huge positions give an
/// empty view.
LayerView visible_range(const LayerPlacement& placement, int tile_w, int tile_h, int map_w, int map_h);

} // namespace nnh::detail
