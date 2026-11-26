// UI font slots (spec 029 FR-14, FR-35): a style naming UiFont::Slot0 measures, wraps, cuts with "…", reveals and draws
// with the slot's font; set_font relays out; a stale font is the built-in one (asserting in Debug/Development); the draw
// hash folds the slot only for a slot. Expected sizes from the fixture's metrics (FontBuilder::latin(): capitals advance
// 8, lowercase 6, ' ' 4, '…' 9, lines 20 high).

#include "../font/font_load_fixture.hpp"
#include "nnh/font.hpp"
#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiWidget;
using namespace font_test;
namespace fake = nnh::platform_fake;

namespace {

nnh::Font g_font;

/// The engine with the fixture's Latin font loaded.
void with_font() {
    start_engine({.max_draws = 4096});
    add_font("fonts/latin", FontBuilder::latin());
    auto f = nnh::load_font("fonts/latin");
    CHECK(f.ok());
    g_font = f ? static_cast<nnh::Font&&>(f.value()) : nnh::Font{};
}

/// A UI whose labels use font slot 0 (no padding: content = text).
struct FontUi {
    Ui ui = ui_test::make({.widgets = 16, .styles = 2});
    nnh::UiStyleId style{};
    FontUi() {
        nnh::UiStyle st;
        st.text.font = nnh::UiFont::Slot0;
        st.padding[0] = st.padding[1] = st.padding[2] = st.padding[3] = 0;
        style = ui.add_style(st);
        ui.set_font(0, g_font);
    }
};

} // namespace

NNH_TEST(ui_fonts_a_label_takes_the_fonts_proportional_size) {
    with_font();
    FontUi u;
    const UiWidget l = u.ui.label(u.ui.root(Screen::Top), {.text = "Aa a", .style = u.style});
    const nnh::UiRect r = u.ui.rect(l);
    CHECK(r.w == 8 + 6 + 4 + 6 && r.h == 20);
    g_font = nnh::Font{};
}

NNH_TEST(ui_fonts_a_wrapping_label_breaks_in_the_fonts_pixels) {
    with_font();
    FontUi u;
    const UiWidget l = u.ui.label(u.ui.root(Screen::Top),
                                  {.text = "aaa bbb ccc", .style = u.style, .layout = {.width = 40}, .wrap = true});
    CHECK(u.ui.rect(l).h == 2 * 20); // "aaa bbb" is 18 + 4 + 18 = 40 (fits), then "ccc"
    g_font = nnh::Font{};
}

NNH_TEST(ui_fonts_max_lines_ends_with_the_fonts_ellipsis) {
    with_font();
    FontUi u;
    const UiWidget l = u.ui.label(u.ui.root(Screen::Top), {.text = "aaaaaaaaaa bbbb", .style = u.style,
                                                           .layout = {.width = 40}, .wrap = true, .max_lines = 1});
    CHECK(u.ui.rect(l).h == 20);
    CHECK(nnh::frame());
    u.ui.draw();
    CHECK(nnh::frame());
    // "aaaaa" (30) + "…" (9) ≤ 40: five glyphs and the ellipsis
    CHECK(fake::state().last_frame_item_count == 6);
    g_font = nnh::Font{};
}

NNH_TEST(ui_fonts_reveal_counts_characters_with_a_font) {
    with_font();
    FontUi u;
    const UiWidget l = u.ui.label(u.ui.root(Screen::Top), {.text = "Abc de", .style = u.style, .reveal_rate = 1.0f});
    CHECK(u.ui.text_cells(l) == 6);
    u.ui.update(nnh::UiInput{});
    u.ui.update(nnh::UiInput{});
    CHECK(u.ui.revealed(l) == 2);
    g_font = nnh::Font{};
}

NNH_TEST(ui_fonts_buttons_choices_and_sliders_measure_with_the_font) {
    with_font();
    FontUi u;
    const UiWidget b = u.ui.button(u.ui.root(Screen::Top), {.text = "Ok", .style = u.style});
    CHECK(u.ui.rect(b).w == 8 + 6 && u.ui.rect(b).h == 20);
    g_font = nnh::Font{};
}

NNH_TEST(ui_fonts_set_font_relays_out) {
    with_font();
    FontUi u;
    const UiWidget l = u.ui.label(u.ui.root(Screen::Top), {.text = "ab", .style = u.style});
    CHECK(u.ui.rect(l).w == 12);
    u.ui.set_font(0, nnh::FontRef{}); // the built-in font again
    CHECK(u.ui.rect(l).w == 16 && u.ui.rect(l).h == 10);
    u.ui.set_font(0, g_font);
    CHECK(u.ui.rect(l).w == 12);
    g_font = nnh::Font{};
}

NNH_TEST(ui_fonts_the_draw_hash_folds_a_slot_only) {
    with_font();
    Ui plain = ui_test::make({.widgets = 4});
    plain.label(plain.root(Screen::Top), {.text = "Hello"});
    Ui slotted = ui_test::make({.widgets = 4});
    slotted.set_font(3, g_font); // a slot set but not used by any style: the hash doesn't change
    slotted.label(slotted.root(Screen::Top), {.text = "Hello"});
    CHECK(nnh::frame());
    plain.draw();
    slotted.draw();
    CHECK(plain.stats().draw_hash == slotted.stats().draw_hash);
    FontUi u;
    u.ui.label(u.ui.root(Screen::Top), {.text = "Hello", .style = u.style});
    u.ui.draw();
    CHECK(u.ui.stats().draw_hash != plain.stats().draw_hash);
    CHECK(nnh::frame());
    g_font = nnh::Font{};
}

NNH_TEST(ui_fonts_a_slot_outside_0_to_7_is_ignored) {
    with_font();
    // the UI lives outside the asserting lambda: an assert jumps out of it and would leak it (LSan, Linux)
    static Ui* ui = nullptr;
    Ui owned = ui_test::make({.widgets = 4});
    ui = &owned;
    CHECK(ui_test::asserts_or_is_ignored([] {
        ui->set_font(8, g_font);
        ui->set_font(-1, g_font);
    }));
    ui = nullptr;
    g_font = nnh::Font{};
}

NNH_TEST(ui_fonts_a_destroyed_font_draws_with_the_builtin) {
    with_font();
    // the UI and its font live outside the asserting lambda: an assert jumps out of it and would leak them (LSan, Linux)
    static FontUi* u = nullptr;
    static UiWidget l{};
    FontUi owned;
    u = &owned;
    l = u->ui.label(u->ui.root(Screen::Top), {.text = "ab", .style = u->style});
    CHECK(ui_test::asserts_or_is_ignored([] {
        g_font = nnh::Font{}; // the slot's font is gone
        CHECK(u->ui.rect(l).w == 16);
    }));
    u = nullptr;
    g_font = nnh::Font{};
}
