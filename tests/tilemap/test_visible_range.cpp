// Which cells of a tile layer a screen shows, and where (tilemap/visible_range.hpp; spec 012 FR-3, AC-2).
// Parallax follows Tiled's preview: screen = world + offset + (camera + screen/2 − origin)·(1 − factor) −
// camera.

#include "test.hpp"
#include "tilemap/visible_range.hpp"

#include <cmath>
#include <limits>

using nnh::Vec2;
using nnh::detail::LayerPlacement;
using nnh::detail::LayerView;
using nnh::detail::visible_range;

namespace {

constexpr Vec2 kTop{400, 240};

LayerView top(Vec2 camera, int tile = 16, int map_w = 100, int map_h = 100) {
    return visible_range({.camera = camera, .screen_size = kTop}, tile, tile, map_w, map_h);
}

} // namespace

NNH_TEST(a_layer_at_the_origin_shows_whole_tiles_from_cell_zero) {
    const LayerView v = top({0, 0});
    CHECK(v.origin_x == 0 && v.origin_y == 0);
    CHECK(v.x0 == 0 && v.x1 == 25 && v.y0 == 0 && v.y1 == 15); // 25 × 16 = 400, 15 × 16 = 240
    CHECK(!v.empty());
}

NNH_TEST(a_camera_between_cells_shows_one_more_column_and_row) {
    const LayerView v = top({8, 4});
    CHECK(v.origin_x == -8 && v.origin_y == -4);
    CHECK(v.x0 == 0 && v.x1 == 26 && v.y0 == 0 && v.y1 == 16); // the plan's worst case: 26 × 16
}

NNH_TEST(the_first_visible_cell_follows_the_camera) {
    const LayerView v = top({160, 48});
    CHECK(v.origin_x == -160 && v.origin_y == -48);
    CHECK(v.x0 == 10 && v.x1 == 35 && v.y0 == 3 && v.y1 == 18);
    const LayerView w = top({175, 63}); // one pixel short of the next cell
    CHECK(w.x0 == 10 && w.x1 == 36 && w.y0 == 3 && w.y1 == 19);
}

NNH_TEST(cells_are_clamped_to_the_map) {
    const LayerView v = top({-50, -50}, 16, 10, 5);
    CHECK(v.origin_x == 50 && v.origin_y == 50);
    CHECK(v.x0 == 0 && v.x1 == 10 && v.y0 == 0 && v.y1 == 5);
    const LayerView right = top({100, 0}, 16, 10, 5); // the map ends at x = 160
    CHECK(right.x0 == 6 && right.x1 == 10);
}

NNH_TEST(a_camera_past_the_map_shows_nothing) {
    CHECK(top({1000, 0}, 16, 10, 10).empty());
    CHECK(top({-400, 0}, 16, 10, 10).empty());  // the map starts exactly at the right edge
    CHECK(!top({-399, 0}, 16, 10, 10).empty()); // one pixel of it shows
    CHECK(top({0, 160}, 16, 10, 10).empty());
    CHECK(top({0, -240}, 16, 10, 10).empty());
}

NNH_TEST(the_origin_is_snapped_to_whole_pixels_with_halves_going_up) {
    CHECK(top({0.4f, 0.5f}).origin_x == 0 && top({0.4f, 0.5f}).origin_y == 0);   // -0.4 → 0, -0.5 → 0
    CHECK(top({0.6f, 1.5f}).origin_x == -1 && top({0.6f, 1.5f}).origin_y == -1); // -0.6 → -1, -1.5 → -1
    CHECK(top({-2.5f, 0}).origin_x == 3);                                        // 2.5 → 3
}

NNH_TEST(the_layer_offset_moves_the_origin) {
    const LayerView v = visible_range({.screen_size = kTop, .offset = {5.4f, -3.6f}}, 16, 16, 100, 100);
    CHECK(v.origin_x == 5 && v.origin_y == -4);
    CHECK(v.x0 == 0 && v.x1 == 25 && v.y0 == 0 && v.y1 == 16);
}

NNH_TEST(parallax_one_scrolls_with_the_camera) {
    const LayerView v =
        visible_range({.camera = {300, 20}, .screen_size = kTop, .parallax = {1, 1}}, 16, 16, 100, 100);
    CHECK(v.origin_x == -300 && v.origin_y == -20);
}

NNH_TEST(parallax_zero_stays_fixed_on_screen) {
    const LayerPlacement a{.camera = {0, 0}, .screen_size = kTop, .parallax = {0, 0}};
    LayerPlacement b = a;
    b.camera = {1234, 567};
    CHECK(visible_range(a, 16, 16, 100, 100).origin_x == visible_range(b, 16, 16, 100, 100).origin_x);
    CHECK(visible_range(a, 16, 16, 100, 100).origin_y == visible_range(b, 16, 16, 100, 100).origin_y);
    // Around the view's centre and the map's parallax origin (0, 0): (0 + 200 − 0)·1 − 0.
    CHECK(visible_range(a, 16, 16, 100, 100).origin_x == 200 &&
          visible_range(a, 16, 16, 100, 100).origin_y == 120);
}

NNH_TEST(parallax_half_moves_half_as_fast_around_the_view_centre) {
    const LayerPlacement p{.camera = {100, 0}, .screen_size = kTop, .parallax = {0.5f, 0.5f}};
    const LayerView v = visible_range(p, 16, 16, 100, 100);
    CHECK(v.origin_x == 50 && v.origin_y == 60); // (100 + 200)·0.5 − 100, (0 + 120)·0.5 − 0
    LayerPlacement moved = p;
    moved.camera.x += 64;
    CHECK(visible_range(moved, 16, 16, 100, 100).origin_x == 50 - 32);
}

NNH_TEST(the_parallax_origin_is_where_the_layer_lines_up_with_the_map) {
    // View centre on the parallax origin: every factor gives the same picture as parallax 1.
    const LayerPlacement p{
        .camera = {300, 100}, .screen_size = kTop, .parallax = {0.25f, 0}, .parallax_origin = {500, 220}};
    const LayerView v = visible_range(p, 16, 16, 100, 100);
    CHECK(v.origin_x == -300 && v.origin_y == -100);
    LayerPlacement left = p;
    left.camera.x -= 100; // (200 + 200 − 500)·0.75 − 200 = −275
    CHECK(visible_range(left, 16, 16, 100, 100).origin_x == -275);
}

NNH_TEST(small_and_large_tiles_cover_the_screen) {
    const LayerView small = top({4, 4}, 8, 1024, 1024);
    CHECK(small.x1 - small.x0 == 51 && small.y1 - small.y0 == 31); // the plan's 8×8 worst case
    const LayerView large = top({32, 32}, 64, 100, 100);
    CHECK(large.x0 == 0 && large.x1 == 7 && large.y0 == 0 && large.y1 == 5);
    const LayerView wide = visible_range({.camera = {20, 0}, .screen_size = kTop}, 32, 8, 100, 100);
    CHECK(wide.x1 - wide.x0 == 14 && wide.y1 - wide.y0 == 30); // non-square tiles; 32 px worst case: 14
    const LayerView wide4 = visible_range({.camera = {4, 0}, .screen_size = kTop}, 32, 8, 100, 100);
    CHECK(wide4.x1 - wide4.x0 == 13);
}

NNH_TEST(the_bottom_screen_is_narrower) {
    const LayerView v = visible_range({.camera = {8, 0}, .screen_size = {320, 240}}, 16, 16, 100, 100);
    CHECK(v.x0 == 0 && v.x1 == 21);
}

NNH_TEST(a_huge_or_invalid_camera_shows_nothing_and_does_not_overflow) {
    CHECK(top({1e12f, 0}).empty());
    // Refused before any float → int conversion: the empty default view, not a saturated origin.
    CHECK(top({1e12f, 0}).origin_x == 0 && top({0, 1e12f}).origin_y == 0);
    CHECK(top({-1e12f, 0}).origin_x == 0 && top({0, -1e12f}).origin_y == 0);
    CHECK(top({-1e12f, -1e12f}).empty());
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK(top({nan, 0}).empty());
    const float inf = std::numeric_limits<float>::infinity();
    CHECK(top({0, inf}).empty());
}
