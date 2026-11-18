// Simulator window layouts (engine/platforms/pc/src/sim_layout.hpp, spec 006 FR-1, FR-2, FR-4).
// Pure math, compiled into the tests from the PC platform library.

#include "sim_layout.hpp"
#include "test.hpp"

using nnh::platform::pc::compute_layout;
using nnh::platform::pc::fit_scale;
using nnh::platform::pc::LayoutResult;
using nnh::platform::pc::natural_size;
using nnh::platform::pc::next_layout;
using nnh::platform::pc::parse_layout;
using nnh::platform::pc::SimLayout;
using nnh::platform::pc::window_to_bottom;

NNH_TEST(natural_sizes_of_every_layout) {
    CHECK(natural_size(SimLayout::Stacked, 2).w == 800 && natural_size(SimLayout::Stacked, 2).h == 960);
    CHECK(natural_size(SimLayout::SideBySide, 1).w == 720 && natural_size(SimLayout::SideBySide, 1).h == 240);
    CHECK(natural_size(SimLayout::LargeTop, 4).w == 400 * 4 + 320 * 2); // bottom at half scale
    CHECK(natural_size(SimLayout::LargeTop, 1).w == 400 + 320);         // ...but at least 1x
    CHECK(natural_size(SimLayout::TopOnly, 3).w == 1200 && natural_size(SimLayout::TopOnly, 3).h == 720);
    CHECK(natural_size(SimLayout::BottomOnly, 2).w == 640 && natural_size(SimLayout::BottomOnly, 2).h == 480);
}

NNH_TEST(stacked_layout_matches_the_console) {
    const LayoutResult r = compute_layout(SimLayout::Stacked, 2, 800, 960);
    CHECK(r.top.visible && r.top.x == 0 && r.top.y == 0 && r.top.scale == 2);
    CHECK(r.bottom.visible && r.bottom.x == 80 && r.bottom.y == 480 && r.bottom.scale == 2);
}

NNH_TEST(side_by_side_puts_the_bottom_screen_right_of_the_top) {
    const LayoutResult r = compute_layout(SimLayout::SideBySide, 2, 1440, 480);
    CHECK(r.top.x == 0 && r.top.y == 0);
    CHECK(r.bottom.x == 800 && r.bottom.y == 0 && r.bottom.scale == 2);
}

NNH_TEST(large_top_shows_a_small_bottom_screen_aligned_to_the_bottom) {
    const LayoutResult r = compute_layout(SimLayout::LargeTop, 4, 1600 + 640, 960);
    CHECK(r.top.scale == 4 && r.bottom.scale == 2);
    CHECK(r.bottom.x == 1600 && r.bottom.y == 960 - 480);
}

NNH_TEST(single_screen_layouts_hide_the_other_screen) {
    const LayoutResult top = compute_layout(SimLayout::TopOnly, 1, 400, 240);
    CHECK(top.top.visible && !top.bottom.visible);
    const LayoutResult bottom = compute_layout(SimLayout::BottomOnly, 1, 320, 240);
    CHECK(!bottom.top.visible && bottom.bottom.visible && bottom.bottom.x == 0);
}

NNH_TEST(content_is_centered_in_a_bigger_window) {
    const LayoutResult r = compute_layout(SimLayout::TopOnly, 1, 600, 440);
    CHECK(r.top.x == 100 && r.top.y == 100);
}

NNH_TEST(fit_picks_the_largest_integer_scale_that_fits) {
    CHECK(fit_scale(SimLayout::Stacked, 1920, 1080) == 2);  // 3x would be 1440 tall
    CHECK(fit_scale(SimLayout::SideBySide, 1920, 1080) == 2); // 3x would be 2160 wide
    CHECK(fit_scale(SimLayout::TopOnly, 1920, 1080) == 4);
    CHECK(fit_scale(SimLayout::TopOnly, 3840, 2160) == 9);
    CHECK(fit_scale(SimLayout::Stacked, 300, 200) == 1); // nothing fits: still 1x
}

NNH_TEST(the_mouse_maps_to_bottom_screen_pixels_in_every_layout) {
    float bx = -1;
    float by = -1;
    const LayoutResult stacked = compute_layout(SimLayout::Stacked, 2, 800, 960);
    CHECK(window_to_bottom(stacked, 80 + 2 * 10, 480 + 2 * 20, false, bx, by));
    CHECK(bx == 10 && by == 20);
    const LayoutResult side = compute_layout(SimLayout::SideBySide, 3, 2160, 720);
    CHECK(window_to_bottom(side, 1200 + 3 * 319 + 1, 3 * 239, false, bx, by));
    CHECK(static_cast<int>(bx) == 319 && static_cast<int>(by) == 239);
}

NNH_TEST(the_mouse_outside_the_bottom_screen_touches_nothing_unless_clamped) {
    float bx = 0;
    float by = 0;
    const LayoutResult r = compute_layout(SimLayout::Stacked, 1, 400, 480);
    CHECK(!window_to_bottom(r, 10, 10, false, bx, by)); // over the top screen
    CHECK(window_to_bottom(r, 10, 10, true, bx, by));   // while a touch is held: clamped to the edge
    CHECK(bx == 0 && by == 0);
    CHECK(window_to_bottom(r, 1000, 1000, true, bx, by));
    CHECK(bx == 319 && by == 239);
}

NNH_TEST(top_only_has_no_touch_screen) {
    float bx = 0;
    float by = 0;
    const LayoutResult r = compute_layout(SimLayout::TopOnly, 1, 400, 240);
    CHECK(!window_to_bottom(r, 10, 10, true, bx, by));
}

NNH_TEST(layouts_cycle_and_have_names) {
    SimLayout l = SimLayout::Stacked;
    for (int i = 0; i < 5; ++i) l = next_layout(l);
    CHECK(l == SimLayout::Stacked);
    CHECK(next_layout(SimLayout::Stacked) == SimLayout::SideBySide);
    SimLayout parsed = SimLayout::Stacked;
    CHECK(parse_layout("side", parsed) && parsed == SimLayout::SideBySide);
    CHECK(parse_layout("large", parsed) && parsed == SimLayout::LargeTop);
    CHECK(!parse_layout("sideways", parsed) && parsed == SimLayout::LargeTop);
    CHECK(!parse_layout(nullptr, parsed));
}

NNH_TEST(the_bottom_screens_right_and_lower_edges_are_outside_it) {
    float bx = 0;
    float by = 0;
    const LayoutResult r = compute_layout(SimLayout::Stacked, 2, 800, 960);
    CHECK(window_to_bottom(r, 80 + 639, 480 + 479, false, bx, by)); // last pixel: inside
    CHECK(!window_to_bottom(r, 80 + 640, 480 + 10, false, bx, by));  // one past the right edge
    CHECK(!window_to_bottom(r, 80 + 10, 480 + 480, false, bx, by));  // one past the bottom edge
}
