// Anchors (spec 028 T1.3, FR-10): nine points + offsets and fills with margins on 400x240 and 320x240 parents, odd
// sizes and negative offsets. Expected values by hand.

#include "test.hpp"
#include "ui/ui_anchor.hpp"

using nnh::UiAnchor;
using nnh::UiPlace;
using nnh::UiRect;
using nnh::detail::anchor_place;

namespace {
const UiRect kTop{0, 0, 400, 240};
const UiRect kBottom{0, 0, 320, 240};
UiRect at(const UiRect& parent, UiAnchor a, int w = 40, int h = 20, int x = 0, int y = 0) {
    return anchor_place(parent, w, h, UiPlace{.anchor = a, .x = x, .y = y});
}
} // namespace

NNH_TEST(ui_anchor_nine_points_on_the_top_screen) {
    CHECK((at(kTop, UiAnchor::TopLeft) == UiRect{0, 0, 40, 20}));
    CHECK((at(kTop, UiAnchor::Top) == UiRect{180, 0, 40, 20}));
    CHECK((at(kTop, UiAnchor::TopRight) == UiRect{360, 0, 40, 20}));
    CHECK((at(kTop, UiAnchor::Left) == UiRect{0, 110, 40, 20}));
    CHECK((at(kTop, UiAnchor::Center) == UiRect{180, 110, 40, 20}));
    CHECK((at(kTop, UiAnchor::Right) == UiRect{360, 110, 40, 20}));
    CHECK((at(kTop, UiAnchor::BottomLeft) == UiRect{0, 220, 40, 20}));
    CHECK((at(kTop, UiAnchor::Bottom) == UiRect{180, 220, 40, 20}));
    CHECK((at(kTop, UiAnchor::BottomRight) == UiRect{360, 220, 40, 20}));
}

NNH_TEST(ui_anchor_nine_points_on_the_bottom_screen) {
    CHECK((at(kBottom, UiAnchor::Top) == UiRect{140, 0, 40, 20}));
    CHECK((at(kBottom, UiAnchor::Center) == UiRect{140, 110, 40, 20}));
    CHECK((at(kBottom, UiAnchor::BottomRight) == UiRect{280, 220, 40, 20}));
}

NNH_TEST(ui_anchor_odd_sizes_take_the_centre_rounded_down) {
    // parent centre 200 (400 >> 1), child centre 20 (41 >> 1): x = 180; y: 120 - 10 (21 >> 1) = 110
    CHECK((at(kTop, UiAnchor::Center, 41, 21) == UiRect{180, 110, 41, 21}));
    const UiRect odd{10, 20, 101, 51};
    CHECK((at(odd, UiAnchor::Center, 40, 20) == UiRect{10 + 50 - 20, 20 + 25 - 10, 40, 20}));
}

NNH_TEST(ui_anchor_offsets_add_and_may_be_negative) {
    CHECK((at(kTop, UiAnchor::TopRight, 40, 20, -4, 4) == UiRect{356, 4, 40, 20}));
    CHECK((at(kBottom, UiAnchor::BottomLeft, 40, 20, 6, -6) == UiRect{6, 214, 40, 20}));
}

NNH_TEST(ui_anchor_parent_position_counts) {
    const UiRect p{100, 50, 200, 100};
    CHECK((at(p, UiAnchor::BottomRight) == UiRect{260, 130, 40, 20}));
}

NNH_TEST(ui_anchor_fill_x_stretches_between_margins) {
    const UiRect r = anchor_place(kTop, 40, 20, UiPlace{.anchor = UiAnchor::Bottom, .x = 8, .y = -4, .fill_x = true,
                                                         .right = 12});
    CHECK((r == UiRect{8, 216, 380, 20}));
}

NNH_TEST(ui_anchor_fill_both_axes) {
    const UiRect r = anchor_place(kBottom, 0, 0,
                                  UiPlace{.x = 10, .y = 20, .fill_x = true, .fill_y = true, .right = 30, .bottom = 40});
    CHECK((r == UiRect{10, 20, 280, 180}));
}
