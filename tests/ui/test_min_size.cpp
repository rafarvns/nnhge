// Minimum sizes (spec 028 T2.2, FR-5 to FR-7): every kind's size from its content, its style's padding and its parts
// with the default theme (cells 8 px, lines 10 px); fixed sizes replace them. Placed at a root's top-left, a widget's
// rectangle is its minimum.

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiRect;
using nnh::UiWidget;

namespace {
nnh::UiWidget top(Ui& ui) { return ui.root(Screen::Top); }
} // namespace

NNH_TEST(ui_min_label_is_its_text_without_padding) {
    Ui ui = ui_test::make();
    CHECK((ui.rect(ui.label(top(ui), {.text = "Hello"})) == UiRect{0, 0, 40, 10}));
    CHECK((ui.rect(ui.label(top(ui), {.text = "two\nlines!"})) == UiRect{0, 0, 48, 20}));
    CHECK((ui.rect(ui.label(top(ui), {.text = ""})) == UiRect{0, 0, 0, 0}));
}

NNH_TEST(ui_min_wrapping_label_is_its_longest_word_wide) {
    Ui ui = ui_test::make();
    const UiWidget w = ui.label(top(ui), {.text = "the quick brown fox", .wrap = true});
    CHECK(ui.rect(w).w == 40);  // "quick" / "brown"
    CHECK(ui.rect(w).h == 40);  // the, quick, brown, fox
}

NNH_TEST(ui_min_button_text_and_icon_with_padding) {
    Ui ui = ui_test::make();
    CHECK((ui.rect(ui.button(top(ui), {.text = "Options"})) == UiRect{0, 0, 56 + 12, 10 + 6}));
    const UiWidget icon = ui.button(top(ui), {.text = "Go", .icon_texture = 0, .icon = {0, 0, 16, 14}});
    CHECK((ui.rect(icon) == UiRect{0, 0, 16 + 4 + 16 + 12, 14 + 6}));
}

NNH_TEST(ui_min_toggle_box_gap_and_text) {
    Ui ui = ui_test::make();
    CHECK((ui.rect(ui.toggle(top(ui), {.text = "Invert"})) == UiRect{0, 0, 10 + 4 + 48 + 12, 10 + 6}));
}

NNH_TEST(ui_min_slider_label_track_and_value) {
    Ui ui = ui_test::make();
    const UiWidget s = ui.slider(top(ui), {.text = "Music", .min = 0, .max = 10});
    CHECK((ui.rect(s) == UiRect{0, 0, (40 + 4) + 48 + (4 + 16) + 12, 16}));
    const UiWidget bare = ui.slider(top(ui), {.min = -100, .max = 5, .show_value = false});
    CHECK(ui.rect(bare).w == 48 + 12);
    const UiWidget neg = ui.slider(top(ui), {.min = -100, .max = 5});
    CHECK(ui.rect(neg).w == 48 + 4 + 32 + 12); // "-100"
}

NNH_TEST(ui_min_choice_arrows_and_widest_option) {
    Ui ui = ui_test::make({.texts = 3});
    static const nnh::UiText opts[3] = {"Easy", "Normal", "Hard"};
    const UiWidget c = ui.choice(top(ui), {.text = "Diff", .options = opts, .count = 3});
    CHECK(ui.rect(c).w == (32 + 4) + 8 + 4 + 48 + 4 + 8 + 12);
    CHECK(ui.rect(c).h == 16);
}

NNH_TEST(ui_min_list_rows_and_cell_width) {
    Ui ui = ui_test::make({.rows = 32});
    const UiWidget l = ui.list(top(ui), {.rows = 8, .count = 100, .fill = [](int, nnh::UiRow&) {}});
    CHECK((ui.rect(l) == UiRect{0, 0, 64 + 4, 8 * 12}));
    const UiWidget g = ui.list(top(ui), {.rows = 3, .columns = 4, .fill = [](int, nnh::UiRow&) {}, .row_height = 20,
                                         .cell_width = 24});
    CHECK((ui.rect(g) == UiRect{0, 0, 24 * 4 + 4, 60}));
}

NNH_TEST(ui_min_progress_image_spacer) {
    Ui ui = ui_test::make();
    CHECK((ui.rect(ui.progress(top(ui), {})) == UiRect{0, 0, 2, 12}));
    CHECK(ui.rect(ui.progress(top(ui), {.value = 3, .max = 10, .show_text = true})).w == 40 + 2); // "10/10"
    CHECK((ui.rect(ui.image(top(ui), {.texture = 0, .region = {0, 0, 16, 8}, .scale = 2})) == UiRect{0, 0, 32, 16}));
    CHECK((ui.rect(ui.spacer(top(ui))) == UiRect{0, 0, 0, 0}));
}

NNH_TEST(ui_min_fixed_size_replaces_the_minimum) {
    Ui ui = ui_test::make();
    const UiWidget b = ui.button(top(ui), {.text = "Go", .layout = {.width = 100, .height = 30}});
    CHECK((ui.rect(b) == UiRect{0, 0, 100, 30}));
    const UiWidget only_w = ui.button(top(ui), {.text = "Go", .layout = {.width = 50}});
    CHECK((ui.rect(only_w) == UiRect{0, 0, 50, 16}));
}

NNH_TEST(ui_min_panel_pads_its_child) {
    Ui ui = ui_test::make();
    const UiWidget p = ui.panel(top(ui));
    const UiWidget l = ui.label(p, {.text = "ab"});
    CHECK((ui.rect(p) == UiRect{0, 0, 16 + 12, 10 + 12}));
    CHECK((ui.rect(l) == UiRect{6, 6, 16, 10}));
}
