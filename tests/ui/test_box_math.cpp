// Box arithmetic (spec 028 T1.2, FR-9): sizes along a box's main axis by stretch ratios, the minimum-exceeds-share
// iteration, remainder pixels first child first, negative extra; cross-axis alignment. Expected values by hand.

#include "test.hpp"
#include "ui/ui_box_math.hpp"

using nnh::UiAlign;
using nnh::detail::box_align;
using nnh::detail::box_distribute;
using nnh::detail::BoxItem;

namespace {
BoxItem fixed_item(int min) { return BoxItem{.min = min}; }
BoxItem grow(int min, int stretch = 1) { return BoxItem{.min = min, .stretch = stretch, .expand = true}; }
} // namespace

NNH_TEST(ui_box_children_that_dont_expand_keep_their_minimum) {
    BoxItem items[3] = {fixed_item(10), fixed_item(20), fixed_item(5)};
    box_distribute(items, 3, 100);
    CHECK(items[0].size == 10 && items[1].size == 20 && items[2].size == 5);
}

NNH_TEST(ui_box_two_expanding_children_share_the_space_equally) {
    BoxItem items[3] = {fixed_item(20), grow(0), grow(0)};
    box_distribute(items, 3, 100); // 80 for the two
    CHECK(items[1].size == 40 && items[2].size == 40);
}

NNH_TEST(ui_box_ratios_one_two_one_share_by_quarter_and_half) {
    BoxItem items[3] = {grow(0, 1), grow(0, 2), grow(0, 1)};
    box_distribute(items, 3, 100);
    CHECK(items[0].size == 25 && items[1].size == 50 && items[2].size == 25);
}

NNH_TEST(ui_box_ratio_255_to_1_gives_the_small_one_its_floor_and_a_remainder_pixel) {
    BoxItem items[2] = {grow(0, 255), grow(0, 1)};
    box_distribute(items, 2, 100); // 100*255/256 = 99 (.6), 100/256 = 0 (.39): remainder 1 to the first
    CHECK(items[0].size == 100 && items[1].size == 0);
}

NNH_TEST(ui_box_remainder_pixels_go_one_each_from_the_first_child) {
    BoxItem items[4] = {grow(0), grow(0), grow(0), grow(0)};
    box_distribute(items, 4, 103); // 25 each, 3 left
    CHECK(items[0].size == 26 && items[1].size == 26 && items[2].size == 26 && items[3].size == 25);
    BoxItem two[3] = {grow(0), grow(0), grow(0)};
    box_distribute(two, 3, 11); // 3 each, 2 left
    CHECK(two[0].size == 4 && two[1].size == 4 && two[2].size == 3);
}

NNH_TEST(ui_box_a_minimum_over_its_share_is_fixed_and_the_rest_recomputed) {
    BoxItem items[3] = {grow(70), grow(0), grow(0)};
    box_distribute(items, 3, 100); // share 33 < 70: fixed; the other two share 30
    CHECK(items[0].size == 70 && items[1].size == 15 && items[2].size == 15);
    CHECK(items[0].fixed && !items[1].fixed);
}

NNH_TEST(ui_box_fixing_one_minimum_can_fix_another) {
    // shares 30 each: 40 fixed → 50 for two: 25 < 28 → 28 fixed → 22 for the last
    BoxItem items[3] = {grow(40), grow(28), grow(0)};
    box_distribute(items, 3, 90);
    CHECK(items[0].size == 40 && items[1].size == 28 && items[2].size == 22);
}

NNH_TEST(ui_box_zero_extra_keeps_every_minimum) {
    BoxItem items[2] = {grow(30), grow(20)};
    box_distribute(items, 2, 50);
    CHECK(items[0].size == 30 && items[1].size == 20);
}

NNH_TEST(ui_box_negative_extra_keeps_minimums_and_overflows) {
    BoxItem items[3] = {grow(30), fixed_item(40), grow(20)};
    const int used = box_distribute(items, 3, 50);
    CHECK(items[0].size == 30 && items[1].size == 40 && items[2].size == 20);
    CHECK(used == 90);
}

NNH_TEST(ui_box_stretch_is_clamped_to_1_255) {
    BoxItem items[2] = {grow(0, 0), grow(0, 1000)};
    box_distribute(items, 2, 256); // 1 : 255
    CHECK(items[0].size == 1 && items[1].size == 255);
}

NNH_TEST(ui_box_align_start_centre_end_fill) {
    CHECK(box_align(UiAlign::Start, 100, 30).pos == 0 && box_align(UiAlign::Start, 100, 30).size == 30);
    CHECK(box_align(UiAlign::Center, 100, 30).pos == 35);
    CHECK(box_align(UiAlign::End, 100, 30).pos == 70);
    CHECK(box_align(UiAlign::Fill, 100, 30).pos == 0 && box_align(UiAlign::Fill, 100, 30).size == 100);
}

NNH_TEST(ui_box_centre_rounds_down) {
    CHECK(box_align(UiAlign::Center, 101, 30).pos == 35); // 71 / 2 = 35.5 → 35
    CHECK(box_align(UiAlign::Center, 30, 41).pos == -6);  // -11 / 2 = -5.5 → -6 (floor)
}
