// When layout runs (spec 028 T2.4, FR-13, FR-7): only after a change that affects it — at the end of update(), on a
// rectangle query, or at the start of draw() for changes made after update() (the maintainer's answer to plan question
// 1) — never for render offsets or opacity; a layout visits each visible node four times (three without a wrapping
// label in the root).

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiInput;
using nnh::UiWidget;

namespace {

struct Menu {
    Ui ui = ui_test::make();
    UiWidget box, a, b;
    Menu() {
        box = ui.vbox(ui.root(Screen::Bottom));
        a = ui.button(box, {.text = "one"});
        b = ui.button(box, {.text = "two"});
        ui.update(UiInput{});
    }
};

} // namespace

NNH_TEST(ui_dirty_no_change_no_layout) {
    Menu m;
    const int layouts = m.ui.stats().layouts;
    for (int i = 0; i < 10; ++i) m.ui.update(UiInput{});
    (void)m.ui.rect(m.a);
    m.ui.draw();
    CHECK(m.ui.stats().layouts == layouts);
}

NNH_TEST(ui_dirty_each_change_lays_out_once) {
    Menu m;
    const int start = m.ui.stats().layouts;
    m.ui.set_text(m.a, "longer text");
    (void)m.ui.rect(m.a);
    (void)m.ui.rect(m.b);
    CHECK(m.ui.stats().layouts == start + 1);
    m.ui.set_visible(m.b, false);
    m.ui.update(UiInput{});
    CHECK(m.ui.stats().layouts == start + 2);
    m.ui.set_layout(m.a, {.width = 99});
    m.ui.draw();
    CHECK(m.ui.stats().layouts == start + 3);
}

NNH_TEST(ui_dirty_layout_visits_three_times_the_visible_nodes_without_wrapping) {
    // no wrapping label: heights measured with widths (T10.3), three walks
    Menu m;
    m.ui.set_text(m.a, "x");
    (void)m.ui.rect(m.a);
    CHECK(m.ui.stats().layout_visits == 3 * 4); // root, box, two buttons
    m.ui.set_visible(m.b, false);
    (void)m.ui.rect(m.a);
    CHECK(m.ui.stats().layout_visits == 3 * 3);
}

NNH_TEST(ui_dirty_a_wrapping_label_makes_four_walks) {
    Menu m;
    const UiWidget wrap = m.ui.label(m.box, {.text = "a long wrapped line", .wrap = true});
    (void)m.ui.rect(m.a);
    CHECK(m.ui.stats().layout_visits == 4 * 5);
    m.ui.destroy(wrap); // the root has no wrapping label again
    (void)m.ui.rect(m.a);
    CHECK(m.ui.stats().layout_visits == 3 * 4);
}

NNH_TEST(ui_dirty_render_offset_and_opacity_never_lay_out) {
    Menu m;
    const int layouts = m.ui.stats().layouts;
    m.ui.set_render_offset(m.box, {12.4f, -3.6f});
    m.ui.set_opacity(m.box, 0.5f);
    m.ui.update(UiInput{});
    m.ui.draw();
    CHECK(m.ui.stats().layouts == layouts);
    CHECK((m.ui.rect(m.a) == nnh::UiRect{0, 0, 36, 16}));
}

NNH_TEST(ui_dirty_a_change_after_update_is_drawn_the_same_frame) {
    ui_test::restart();
    Menu m;
    CHECK(nnh::frame());
    m.ui.update(UiInput{});
    m.ui.set_text(m.a, "a much longer one");
    m.ui.draw(); // laid out first
    CHECK(m.ui.rect(m.a).w == 17 * 8 + 12);
    nnh::shutdown();
}

NNH_TEST(ui_dirty_the_other_root_isnt_laid_out) {
    Menu m;
    const UiWidget top = m.ui.label(m.ui.root(Screen::Top), {.text = "top"});
    (void)m.ui.rect(top);
    m.ui.set_text(top, "other");
    (void)m.ui.rect(top);
    CHECK(m.ui.stats().layout_visits == 3 * 2); // the top root and its label only
}
