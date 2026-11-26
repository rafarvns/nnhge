// Scroll boxes (spec 028 T5.2, FR-12): a window of whole children; moving the focus past it scrolls with the margin;
// scrolling moves rectangles without a layout; children outside aren't drawn or touchable; set_scroll.

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiRect;
using nnh::UiWidget;
using ui_test::kDown;
using ui_test::kUp;
using ui_test::tap;

namespace {

/// A 56-pixel scroll box of eight 16-pixel buttons, 4 apart: three fit (16 + 4 + 16 + 4 + 16 = 56).
struct Scroller {
    Ui ui = ui_test::make();
    UiWidget box;
    UiWidget b[8];
    explicit Scroller(int margin = 0) {
        box = ui.scroll_box(ui.root(Screen::Bottom), {.margin = margin, .layout = {.width = 100, .height = 56,
                                                                                   .place = {.y = 10}}});
        for (UiWidget& w : b) w = ui.button(box, {.text = "x"});
        ui_test::settle(ui);
    }
};

} // namespace

NNH_TEST(ui_scroll_box_lays_children_out_from_its_top) {
    Scroller s;
    CHECK((s.ui.rect(s.b[0]) == UiRect{0, 10, 100, 16}));
    CHECK((s.ui.rect(s.b[2]) == UiRect{0, 50, 100, 16}));
    CHECK(s.ui.visible(s.b[3])); // visible (not hidden), only outside the window
}

NNH_TEST(ui_scroll_box_scrolls_to_reveal_the_next_child) {
    Scroller s;
    tap(s.ui, kDown);
    tap(s.ui, kDown);
    CHECK(s.ui.focused() == s.b[2] && s.ui.rect(s.b[0]).y == 10);
    const int layouts = s.ui.stats().layouts;
    tap(s.ui, kDown);
    CHECK(s.ui.focused() == s.b[3]);
    CHECK(s.ui.rect(s.b[1]).y == 10 && s.ui.rect(s.b[3]).y == 50);
    CHECK(s.ui.stats().layouts == layouts); // moved, not laid out
    for (int i = 0; i < 4; ++i) tap(s.ui, kDown);
    CHECK(s.ui.focused() == s.b[7] && s.ui.rect(s.b[5]).y == 10);
    tap(s.ui, kUp);
    tap(s.ui, kUp);
    tap(s.ui, kUp);
    CHECK(s.ui.focused() == s.b[4] && s.ui.rect(s.b[4]).y == 10);
}

NNH_TEST(ui_scroll_box_keeps_a_margin) {
    Scroller s(1);
    tap(s.ui, kDown);
    CHECK(s.ui.focused() == s.b[1] && s.ui.rect(s.b[0]).y == 10);
    tap(s.ui, kDown); // b[2] with one below it: scrolls one
    CHECK(s.ui.focused() == s.b[2] && s.ui.rect(s.b[1]).y == 10);
}

NNH_TEST(ui_scroll_box_children_outside_arent_touchable) {
    Scroller s;
    // b[3] sits below the window (y 70): a touch there reaches nothing inside the box
    s.ui.update(ui_test::touch_start(50, 75));
    s.ui.update(ui_test::touch_end(50, 75));
    CHECK(!s.ui.activated(s.b[3]) && s.ui.focused() == s.b[0]);
}

NNH_TEST(ui_scroll_box_set_scroll) {
    Scroller s;
    s.ui.set_scroll(s.box, 4);
    CHECK(s.ui.rect(s.b[4]).y == 10 && s.ui.rect(s.b[0]).y == 10 - 80);
}

NNH_TEST(ui_scroll_box_children_outside_arent_drawn) {
    ui_test::restart();
    {
        Ui ui = ui_test::make();
        const UiWidget box = ui.scroll_box(ui.root(Screen::Bottom), {.layout = {.width = 100, .height = 30}});
        for (int i = 0; i < 5; ++i) ui.label(box, {.text = "ab"}); // 10 tall, 4 apart: two fit
        CHECK(nnh::frame());
        ui.draw();
        CHECK(nnh::frame());
        CHECK(nnh::platform_fake::state().last_frame_item_count == 4); // two labels of two glyphs
    }
    nnh::shutdown();
}
