// Box layout (spec 028 T2.3, FR-9): VBox, HBox, grid box, stack and panel with padding, spacing, alignment, expand,
// stretch ratios and the remainder order; hidden children take no space; overflow warned once. Rectangles by hand
// (default theme: labels 8 px per character, 10 px per line, no padding).

#include "ui/ui_block.hpp"
#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiAlign;
using nnh::UiRect;
using nnh::UiWidget;

namespace {
UiWidget bottom(Ui& ui) { return ui.root(Screen::Bottom); }
} // namespace

NNH_TEST(ui_vbox_stacks_children_with_spacing_and_fills_across) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.vbox(bottom(ui));
    const UiWidget a = ui.label(box, {.text = "ab"});
    const UiWidget b = ui.label(box, {.text = "abcd"});
    CHECK((ui.rect(box) == UiRect{0, 0, 32, 24}));
    CHECK((ui.rect(a) == UiRect{0, 0, 32, 10}));
    CHECK((ui.rect(b) == UiRect{0, 14, 32, 10}));
}

NNH_TEST(ui_vbox_padding_surrounds_its_children) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.vbox(bottom(ui), {.spacing = 2, .padding = 3});
    const UiWidget a = ui.label(box, {.text = "ab"});
    const UiWidget b = ui.label(box, {.text = "a"});
    CHECK((ui.rect(box) == UiRect{0, 0, 16 + 6, 22 + 6}));
    CHECK((ui.rect(a) == UiRect{3, 3, 16, 10}));
    CHECK((ui.rect(b) == UiRect{3, 15, 16, 10}));
}

NNH_TEST(ui_vbox_aligns_children_across) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.vbox(bottom(ui), {.layout = {.width = 41}});
    const UiWidget s = ui.label(box, {.text = "ab", .layout = {.align = UiAlign::Start}});
    const UiWidget c = ui.label(box, {.text = "ab", .layout = {.align = UiAlign::Center}});
    const UiWidget e = ui.label(box, {.text = "ab", .layout = {.align = UiAlign::End}});
    CHECK(ui.rect(s).x == 0 && ui.rect(s).w == 16);
    CHECK(ui.rect(c).x == 12); // (41 - 16) / 2 = 12.5 → 12
    CHECK(ui.rect(e).x == 25);
}

NNH_TEST(ui_hbox_expanding_child_takes_the_extra) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.hbox(bottom(ui), {.spacing = 0, .layout = {.width = 100}});
    const UiWidget a = ui.label(box, {.text = "ab"});
    const UiWidget gap = ui.spacer(box);
    const UiWidget b = ui.label(box, {.text = "cd"});
    CHECK((ui.rect(a) == UiRect{0, 0, 16, 10}));
    CHECK((ui.rect(gap) == UiRect{16, 0, 68, 10}));
    CHECK((ui.rect(b) == UiRect{84, 0, 16, 10}));
}

NNH_TEST(ui_hbox_stretch_ratios_share_the_space) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.hbox(bottom(ui), {.spacing = 0, .layout = {.width = 90}});
    const UiWidget a = ui.spacer(box, {.expand = true, .stretch = 1});
    const UiWidget b = ui.spacer(box, {.expand = true, .stretch = 2});
    CHECK(ui.rect(a).w == 30 && ui.rect(b).w == 60 && ui.rect(b).x == 30);
}

NNH_TEST(ui_hbox_remainder_pixels_go_first_child_first) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.hbox(bottom(ui), {.spacing = 1, .layout = {.width = 106}}); // 103 after 3 spacings
    UiWidget s[4];
    for (UiWidget& w : s) w = ui.spacer(box);
    CHECK(ui.rect(s[0]).w == 26 && ui.rect(s[1]).w == 26 && ui.rect(s[2]).w == 26 && ui.rect(s[3]).w == 25);
    CHECK(ui.rect(s[1]).x == 27 && ui.rect(s[3]).x == 81);
}

NNH_TEST(ui_box_hidden_children_take_no_space) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.vbox(bottom(ui));
    ui.label(box, {.text = "a"});
    const UiWidget mid = ui.label(box, {.text = "bbbbbbbb"});
    const UiWidget c = ui.label(box, {.text = "c"});
    ui.set_visible(mid, false);
    CHECK(ui.rect(c).y == 14);
    CHECK((ui.rect(box) == UiRect{0, 0, 8, 24}));
    ui.set_visible(mid, true);
    CHECK(ui.rect(c).y == 28 && ui.rect(box).w == 64);
}

NNH_TEST(ui_grid_box_columns_and_rows_take_their_largest_child) {
    Ui ui = ui_test::make();
    const UiWidget g = ui.grid_box(bottom(ui), {.spacing = 2, .columns = 2});
    const UiWidget a = ui.label(g, {.text = "a"});
    const UiWidget b = ui.label(g, {.text = "bbb"});
    const UiWidget c = ui.label(g, {.text = "cc"});
    const UiWidget d = ui.label(g, {.text = "d", .layout = {.align = UiAlign::End}});
    CHECK((ui.rect(g) == UiRect{0, 0, 16 + 2 + 24, 22}));
    CHECK((ui.rect(a) == UiRect{0, 0, 16, 10}));
    CHECK((ui.rect(b) == UiRect{18, 0, 24, 10}));
    CHECK((ui.rect(c) == UiRect{0, 12, 16, 10}));
    CHECK((ui.rect(d) == UiRect{18 + 16, 12, 8, 10}));
}

NNH_TEST(ui_stack_places_children_by_anchors) {
    Ui ui = ui_test::make();
    const UiWidget st = ui.stack(bottom(ui), {.layout = {.width = 100, .height = 50, .place = {.x = 10, .y = 20}}});
    const UiWidget c = ui.label(st, {.text = "ab", .layout = {.place = {.anchor = nnh::UiAnchor::Center}}});
    const UiWidget br = ui.label(st, {.text = "x", .layout = {.place = {.anchor = nnh::UiAnchor::BottomRight, .x = -1}}});
    CHECK((ui.rect(st) == UiRect{10, 20, 100, 50}));
    CHECK((ui.rect(c) == UiRect{10 + 50 - 8, 20 + 25 - 5, 16, 10}));
    CHECK((ui.rect(br) == UiRect{10 + 100 - 8 - 1, 20 + 50 - 10, 8, 10}));
}

NNH_TEST(ui_panel_as_box_stacks_with_its_style_padding) {
    Ui ui = ui_test::make();
    const UiWidget p = ui.panel(bottom(ui), {.as_box = true, .box = nnh::UiKind::HBox, .spacing = 2});
    const UiWidget a = ui.label(p, {.text = "ab"});
    const UiWidget b = ui.label(p, {.text = "c"});
    CHECK((ui.rect(p) == UiRect{0, 0, 6 + 16 + 2 + 8 + 6, 6 + 10 + 6}));
    CHECK((ui.rect(a) == UiRect{6, 6, 16, 10}));
    CHECK((ui.rect(b) == UiRect{24, 6, 8, 10}));
}

NNH_TEST(ui_nested_boxes_compose) {
    Ui ui = ui_test::make();
    const UiWidget col = ui.vbox(bottom(ui), {.spacing = 0, .layout = {.place = {.x = 5, .y = 7}}});
    const UiWidget row = ui.hbox(col, {.spacing = 4});
    const UiWidget a = ui.label(row, {.text = "ab"});
    const UiWidget b = ui.label(row, {.text = "cd"});
    const UiWidget under = ui.label(col, {.text = "wide line"});
    CHECK((ui.rect(col) == UiRect{5, 7, 72, 20}));
    CHECK((ui.rect(row) == UiRect{5, 7, 72, 10}));
    CHECK(ui.rect(a).x == 5 && ui.rect(b).x == 25);
    CHECK((ui.rect(under) == UiRect{5, 17, 72, 10}));
}

NNH_TEST(ui_overflow_keeps_minimums_and_is_warned_once) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.vbox(bottom(ui), {.layout = {.width = 10}});
    const UiWidget l = ui.label(box, {.text = "abcdef"});
    CHECK(ui.rect(l).w == 10); // filled across to the box's width: the label itself overflows its content? no:
    // a fixed box narrower than its content: the box is warned, children keep the box's width (Fill)
    const nnh::detail::UiBlock& b = *nnh::UiAccess::block(ui);
    CHECK(((b.hot[box.slot()].flags & nnh::detail::kUiWarned) != 0) == nnh::build::kAssertsEnabled);
    const int lines = nnh::platform_fake::state().console_lines;
    ui.set_text(l, "abcdefg");
    (void)ui.rect(l);
    CHECK(nnh::platform_fake::state().console_lines == lines); // once per widget
}

NNH_TEST(ui_hbox_children_over_the_width_overflow_without_shrinking) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.hbox(bottom(ui), {.spacing = 0, .layout = {.width = 20}});
    const UiWidget a = ui.label(box, {.text = "abc"});
    const UiWidget b = ui.label(box, {.text = "def"});
    CHECK(ui.rect(a).w == 24 && ui.rect(b).x == 24 && ui.rect(b).w == 24);
    // given less than its children's minimums by its parent (not by a fixed size), the box itself is warned
    Ui nested = ui_test::make();
    const UiWidget outer = nested.vbox(bottom(nested), {.layout = {.width = 20}});
    const UiWidget row = nested.hbox(outer, {.spacing = 0});
    nested.label(row, {.text = "abc"});
    nested.label(row, {.text = "def"});
    (void)nested.rect(row);
    const nnh::detail::UiBlock& block = *nnh::UiAccess::block(nested);
    CHECK(((block.hot[row.slot()].flags & nnh::detail::kUiWarned) != 0) == nnh::build::kAssertsEnabled);
}

NNH_TEST(ui_grid_box_with_fewer_children_than_columns_is_as_wide_as_they_are) {
    Ui ui = ui_test::make();
    const UiWidget g = ui.grid_box(bottom(ui), {.spacing = 2, .columns = 4});
    ui.label(g, {.text = "ab"});
    ui.label(g, {.text = "abc"});
    CHECK((ui.rect(g) == UiRect{0, 0, 16 + 2 + 24, 10})); // two columns, not four
}

NNH_TEST(ui_grid_box_rows_past_the_track_table_are_placed_in_order) {
    Ui ui = ui_test::make({.widgets = 80});
    const UiWidget g = ui.grid_box(bottom(ui), {.spacing = 1, .columns = 1});
    UiWidget rows[70];
    for (int i = 0; i < 70; ++i) rows[i] = ui.label(g, {.text = i == 66 ? "a\nb" : "a"}); // row 66 two lines high
    CHECK(ui.rect(rows[63]).y == 63 * 11);
    CHECK(ui.rect(rows[64]).y == 64 * 11 && ui.rect(rows[65]).y == 65 * 11);
    CHECK(ui.rect(rows[66]).y == 66 * 11 && ui.rect(rows[66]).h == 20);
    CHECK(ui.rect(rows[67]).y == 66 * 11 + 21 && ui.rect(rows[69]).y == 66 * 11 + 21 + 2 * 11);
    CHECK(ui.rect(g).h == 69 * 11 + 21 - 1);
}
