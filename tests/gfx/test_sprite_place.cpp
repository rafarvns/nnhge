// Shared placement (spec 024 T1.2): the sprite and rectangle arithmetic DrawList::build used, moved into
// gfx/sprite_place.hpp so the particle expansion places items by the same rule (zoom, flips, pivots, rounding).

#include <initializer_list>

#include "gfx/sprite_place.hpp"
#include "test.hpp"

using nnh::platform::DrawItem2D;
using nnh::detail::place_rect;
using nnh::detail::place_sprite;
using nnh::detail::snap;

NNH_TEST(snap_rounds_halves_up) {
    CHECK(snap(1.5f) == 2.0f && snap(-1.5f) == -1.0f && snap(2.49f) == 2.0f && snap(-0.5f) == 0.0f);
}

NNH_TEST(place_sprite_scales_sizes_and_pivots_by_scale_and_zoom) {
    for (float z : {0.5f, 1.0f, 2.0f}) {
        DrawItem2D item;
        place_sprite({8, 0, 8, 4}, false, false, {2, 3}, {4, 2}, 0.25f, {10.25f, 3.5f}, z, item);
        CHECK(item.region.x == 8 && item.region.w == 8 && item.region.h == 4);
        CHECK(item.w == 16 * z && item.h == 12 * z);
        CHECK(item.pivot_x == 8 * z && item.pivot_y == 6 * z);
        CHECK(item.rotation == 0.25f);
        CHECK(item.x == snap(10.25f * z) && item.y == snap(3.5f * z));
    }
}

NNH_TEST(place_sprite_negative_scale_flips) {
    DrawItem2D item;
    place_sprite({0, 0, 4, 4}, false, true, {-1, -2}, {}, 0, {}, 1, item);
    CHECK(item.flip_x && !item.flip_y); // a flip and a negative scale cancel
    CHECK(item.w == 4 && item.h == 8);
}

NNH_TEST(place_rect_rounds_both_corners) {
    DrawItem2D item;
    place_rect({0.5f, 1.25f}, {2.25f, 2.5f}, 1, item);
    CHECK(item.x == 1 && item.y == 1 && item.w == 2 && item.h == 3); // corners 1..3 and 1..4
    place_rect({0.5f, 1.25f}, {2.25f, 2.5f}, 2, item);
    CHECK(item.x == 1 && item.y == 3 && item.w == 5 && item.h == 5);
}
