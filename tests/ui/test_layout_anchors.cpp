// Anchored layout (spec 028 T2.3, FR-10): a root's children by the nine points and fills on both screens; a wrapping
// label's height from the width its box gave it.

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiAnchor;
using nnh::UiRect;
using nnh::UiWidget;

NNH_TEST(ui_root_children_anchor_on_the_top_screen) {
    Ui ui = ui_test::make();
    const UiWidget r = ui.root(Screen::Top);
    const UiWidget tl = ui.label(r, {.text = "ab", .layout = {.place = {.anchor = UiAnchor::TopLeft, .x = 4, .y = 4}}});
    const UiWidget tr = ui.label(r, {.text = "ab", .layout = {.place = {.anchor = UiAnchor::TopRight, .x = -4, .y = 4}}});
    const UiWidget c = ui.label(r, {.text = "ab", .layout = {.place = {.anchor = UiAnchor::Center}}});
    const UiWidget bm = ui.label(r, {.text = "ab", .layout = {.place = {.anchor = UiAnchor::Bottom}}});
    CHECK((ui.rect(tl) == UiRect{4, 4, 16, 10}));
    CHECK((ui.rect(tr) == UiRect{380, 4, 16, 10}));
    CHECK((ui.rect(c) == UiRect{192, 115, 16, 10}));
    CHECK((ui.rect(bm) == UiRect{192, 230, 16, 10}));
}

NNH_TEST(ui_root_children_anchor_on_the_bottom_screen) {
    Ui ui = ui_test::make();
    const UiWidget r = ui.root(Screen::Bottom);
    const UiWidget c = ui.button(r, {.text = "Go", .layout = {.place = {.anchor = UiAnchor::Center}}});
    const UiWidget br = ui.button(r, {.text = "Go", .layout = {.place = {.anchor = UiAnchor::BottomRight}}});
    CHECK((ui.rect(c) == UiRect{160 - 14, 120 - 8, 28, 16}));
    CHECK((ui.rect(br) == UiRect{320 - 28, 240 - 16, 28, 16}));
}

NNH_TEST(ui_root_child_fills_between_margins) {
    Ui ui = ui_test::make();
    const UiWidget bar = ui.progress(ui.root(Screen::Top),
                                     {.layout = {.height = 12, .place = {.anchor = UiAnchor::Bottom, .x = 8, .y = -2,
                                                                         .fill_x = true, .right = 8}}});
    CHECK((ui.rect(bar) == UiRect{8, 240 - 12 - 2, 384, 12}));
}

NNH_TEST(ui_a_wrapping_label_is_as_tall_as_its_lines_at_its_final_width) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom), {.layout = {.width = 80}});
    const UiWidget l = ui.label(box, {.text = "the quick brown fox", .wrap = true});
    const UiWidget after = ui.label(box, {.text = "x"});
    CHECK((ui.rect(l) == UiRect{0, 0, 80, 20})); // 10 cells: "the quick" / "brown fox"
    CHECK(ui.rect(after).y == 24);
    ui.set_layout(box, {.width = 48});
    CHECK(ui.rect(l).h == 40); // 6 cells: the / quick / brown / fox
}

NNH_TEST(ui_a_hidden_root_still_lays_out) {
    Ui ui = ui_test::make();
    const UiWidget l = ui.label(ui.root(Screen::Top), {.text = "ab", .layout = {.place = {.anchor = UiAnchor::Center}}});
    ui.set_root_visible(Screen::Top, false);
    CHECK(ui.rect(l).x == 192);
    CHECK(!ui.visible(l));
}
