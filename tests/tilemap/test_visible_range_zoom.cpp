// A layer's place and visible cells at a camera zoom (tilemap/visible_range.hpp; spec 018 FR-9, decision D33): the
// origin is snap((world origin − camera) × zoom), cells follow the zoomed view, parallax uses the zoomed view's
// centre.

#include "test.hpp"
#include "tilemap/visible_range.hpp"

using nnh::Vec2;
using nnh::detail::LayerPlacement;
using nnh::detail::LayerView;
using nnh::detail::visible_range;

namespace {

constexpr Vec2 kTop{400, 240};

LayerView zoomed(Vec2 camera, float zoom, int tile = 16, int map = 1000) {
    return visible_range({.camera = camera, .screen_size = kTop, .zoom = zoom}, tile, tile, map, map);
}

} // namespace

NNH_TEST(zoom_2_shows_a_quarter_of_the_cells) {
    const LayerView v = zoomed({160, 48}, 2);
    CHECK(v.origin_x == -320 && v.origin_y == -96 && v.zoom_q == 131072);
    CHECK(v.x0 == 10 && v.x1 == 23 && v.y0 == 3 && v.y1 == 11); // 32-pixel cells: 12.5 × 7.5 of them
}

NNH_TEST(zoom_half_shows_four_times_the_cells) {
    const LayerView v = zoomed({0, 0}, 0.5f);
    CHECK(v.origin_x == 0 && v.origin_y == 0 && v.zoom_q == 32768);
    CHECK(v.x0 == 0 && v.x1 == 50 && v.y0 == 0 && v.y1 == 30); // 8-pixel cells
    const LayerView w = zoomed({4, 4}, 0.5f, 8); // 4-pixel cells, 2 px off: one more of each
    CHECK(w.x1 - w.x0 == 101 && w.y1 - w.y0 == 61);
}

NNH_TEST(the_zoomed_origin_is_snapped_once) {
    const LayerView v = zoomed({10.3f, -7.2f}, 1.5f);
    CHECK(v.origin_x == -15 && v.origin_y == 11); // −15.45 → −15, 10.8 → 11
}

NNH_TEST(a_fixed_layer_is_zoomed_about_the_view_centre) {
    // Parallax 0 with the parallax origin at the view's centre: at zoom 1 the layer's (0, 0) is the screen's.
    LayerPlacement p{.camera = {1000, 37}, .screen_size = kTop, .parallax = {0, 0}, .parallax_origin = {200, 120}};
    CHECK(visible_range(p, 16, 16, 100, 100).origin_x == 0);
    // Zoom 2 about the view's centre: the screen's centre (200, 120) stays on the layer's point (200, 120).
    p.zoom = 2;
    const LayerView v = visible_range(p, 16, 16, 100, 100);
    CHECK(v.origin_x == -200 && v.origin_y == -120);
}

NNH_TEST(a_half_parallax_layer_scrolls_half_as_fast_at_any_zoom) {
    LayerPlacement a{.camera = {0, 0}, .screen_size = kTop, .parallax = {0.5f, 1}, .zoom = 2};
    LayerPlacement b = a;
    b.camera = {100, 0};
    const int moved = visible_range(a, 16, 16, 100, 100).origin_x - visible_range(b, 16, 16, 100, 100).origin_x;
    CHECK(moved == 100); // 50 world pixels × zoom 2
}

NNH_TEST(huge_or_nan_cameras_still_give_an_empty_view_when_zoomed) {
    CHECK(zoomed({3e9f, 0}, 0.5f).empty());
    CHECK(zoomed({__builtin_nanf(""), 0}, 2).empty());
    CHECK(zoomed({0, -3e9f}, 3).empty());
}
