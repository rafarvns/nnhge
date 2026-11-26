// Typewriter reveal (spec 028 T2.5, FR-23): rates 1, 0.5, 0.25 and 3 exactly over 600 frames, show_all, fully_shown,
// set_reveal, and line breaks that never change while revealing.

#include <limits>

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiInput;
using nnh::UiWidget;

namespace {

const char* kText = "Welcome, traveller. The gate opens at dawn; bring a lantern and twelve coins."; // 77 cells, wrapped

bool reveals_exactly(float rate, int num, int den) {
    Ui ui = ui_test::make();
    const UiWidget l = ui.label(ui.root(Screen::Top), {.text = kText, .layout = {.width = 160}, .wrap = true,
                                                       .reveal_rate = rate});
    const int total = ui.text_cells(l);
    bool ok = total > 0 && ui.revealed(l) == 0;
    for (int f = 1; f <= 600; ++f) {
        ui.update(UiInput{});
        const int expected = f * num / den < total ? f * num / den : total;
        ok = ok && ui.revealed(l) == expected && ui.fully_shown(l) == (expected == total);
    }
    return ok;
}

} // namespace

NNH_TEST(ui_reveal_one_per_frame) { CHECK(reveals_exactly(1.0f, 1, 1)); }
NNH_TEST(ui_reveal_half_per_frame) { CHECK(reveals_exactly(0.5f, 1, 2)); }
NNH_TEST(ui_reveal_quarter_per_frame) { CHECK(reveals_exactly(0.25f, 1, 4)); }
NNH_TEST(ui_reveal_three_per_frame) { CHECK(reveals_exactly(3.0f, 3, 1)); }

NNH_TEST(ui_reveal_show_all_and_set_reveal) {
    Ui ui = ui_test::make();
    const UiWidget l = ui.label(ui.root(Screen::Top), {.text = "Hello there", .reveal_rate = 0.5f});
    ui.update(UiInput{});
    CHECK(!ui.fully_shown(l));
    ui.show_all(l);
    CHECK(ui.fully_shown(l) && ui.revealed(l) == 11);
    ui.set_reveal(l, 3);
    CHECK(ui.revealed(l) == 3);
    ui.update(UiInput{});
    ui.update(UiInput{});
    CHECK(ui.revealed(l) == 4);
}

NNH_TEST(ui_reveal_without_a_rate_shows_everything) {
    Ui ui = ui_test::make();
    const UiWidget l = ui.label(ui.root(Screen::Top), {.text = "Hello"});
    CHECK(ui.fully_shown(l) && ui.revealed(l) == 5);
}

NNH_TEST(ui_reveal_never_moves_a_word_to_another_line) {
    ui_test::restart();
    Ui ui = ui_test::make();
    const UiWidget l = ui.label(ui.root(Screen::Top), {.text = kText, .layout = {.width = 160}, .wrap = true,
                                                       .reveal_rate = 1.0f});
    // the glyphs of the whole text, where they sit when fully shown
    static float full_x[128], full_y[128];
    ui.show_all(l);
    CHECK(nnh::frame());
    ui.draw();
    CHECK(nnh::frame());
    const auto& s = nnh::platform_fake::state();
    const int glyphs = s.last_frame_item_count;
    for (int i = 0; i < glyphs && i < 128; ++i) {
        full_x[i] = s.last_frame_items[i].x;
        full_y[i] = s.last_frame_items[i].y;
    }
    CHECK(glyphs > 40 && ui.rect(l).h == 50); // 20 cells per line: 5 lines
    ui.set_reveal(l, 0);
    bool ok = true;
    for (int f = 0; f < 90; ++f) {
        ui.update(UiInput{});
        ui.draw();
        CHECK(nnh::frame());
        const int n = s.last_frame_item_count;
        ok = ok && n <= glyphs;
        for (int i = 0; i < n && i < 128; ++i) {
            ok = ok && s.last_frame_items[i].x == full_x[i] && s.last_frame_items[i].y == full_y[i];
        }
    }
    CHECK(ok);
    nnh::shutdown();
}

NNH_TEST(ui_reveal_rate_must_be_finite) {
    static Ui* ui;
    Ui u = ui_test::make();
    ui = &u;
    static UiWidget l;
    l = u.label(u.root(Screen::Top), {.text = "x"});
    CHECK(ui_test::asserts_or_is_ignored([] { ui->set_reveal_rate(l, std::numeric_limits<float>::quiet_NaN()); }));
}
