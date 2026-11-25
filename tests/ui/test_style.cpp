// Styles and themes (spec 028 T6.1, FR-26): state precedence, custom styles and overrides, metric changes relaying out
// while colour changes don't, a game theme, and the default theme's text readable (contrast ≥ 4.5) on its faces. A
// theme converted by hand from a game's own data values applies (the data-file pattern; the style structs' data
// declarations are checked by the ctest ui_style_structs_are_data_loadable).

#include <cmath>

#include "nnh/texture.hpp"
#include "ui_draw_fixture.hpp"

using nnh::Color;
using nnh::Screen;
using nnh::Ui;
using nnh::UiState;
using nnh::UiStyle;
using nnh::UiWidget;

namespace {

double channel(int c) {
    const double v = c / 255.0;
    return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}
double luminance(Color c) { return 0.2126 * channel(c.r) + 0.7152 * channel(c.g) + 0.0722 * channel(c.b); }
double contrast(Color a, Color b) {
    const double x = luminance(a), y = luminance(b);
    return (x > y ? x + 0.05 : y + 0.05) / (x > y ? y + 0.05 : x + 0.05);
}

ui_test::Item g_items[64];

} // namespace

NNH_TEST(ui_style_default_theme_text_is_readable_on_its_faces) {
    const nnh::UiTheme& t = nnh::ui::default_theme();
    const UiStyle* styles[] = {&t.button, &t.toggle, &t.slider, &t.choice, &t.panel, &t.progress};
    bool ok = true;
    for (const UiStyle* s : styles) {
        for (const nnh::UiLook& l : s->states) {
            if (l.background.kind != nnh::UiBackgroundKind::Solid) continue;
            ok = ok && contrast(l.text, l.background.color) >= 4.5;
        }
    }
    // a list draws text only in its rows: normal and disabled rows on its face, the focused row on the focus colour
    ok = ok && contrast(t.list.states[0].text, t.list.states[0].background.color) >= 4.5;
    ok = ok && contrast(t.list.states[3].text, t.list.states[0].background.color) >= 4.5;
    ok = ok && contrast(t.list.states[1].text, t.list.list.row[1].color) >= 4.5;
    CHECK(ok);
    for (const nnh::UiLook& l : t.button.states) CHECK(l.background.texture == 0 && l.background.kind != nnh::UiBackgroundKind::NineSlice);
}

NNH_TEST(ui_style_state_precedence) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
    const UiWidget t = ui.toggle(box, {.text = "t", .checked = true});
    const UiWidget other = ui.button(box, {.text = "o"});
    ui_test::settle(ui);
    CHECK(ui.state(t) == UiState::Focused); // focused beats checked
    ui.focus(other);
    CHECK(ui.state(t) == UiState::Checked);
    ui.focus(t);
    ui_test::tap(ui, ui_test::kConfirm);
    CHECK(ui.state(t) == UiState::Pressed); // pressed beats focused
    ui.set_enabled(t, false);
    CHECK(ui.state(t) == UiState::Disabled); // disabled beats all
}

NNH_TEST(ui_style_custom_style_overrides_one_widget) {
    ui_test::restart();
    {
        Ui ui = ui_test::make({.styles = 2});
        UiStyle red = nnh::ui::default_theme().button;
        red.states[0].background.color = nnh::colors::red;
        const nnh::UiStyleId id = ui.add_style(red);
        CHECK(id.value == 1);
        const UiWidget a = ui.button(ui.root(Screen::Bottom), {.text = "", .style = id});
        ui.button(ui.root(Screen::Bottom), {.text = "", .layout = {.place = {.y = 50}}});
        ui_test::capture([&] { ui.draw(); }, g_items, 64);
        CHECK(g_items[0].color.r == 255 && g_items[0].color.g == 0);
        CHECK(g_items[1].color.r == 44);
        ui.set_style(a, {});
        ui_test::capture([&] { ui.draw(); }, g_items, 64);
        CHECK(g_items[0].color.r == 44);
    }
    nnh::shutdown();
}

NNH_TEST(ui_style_metric_changes_relayout_colour_changes_dont) {
    Ui ui = ui_test::make({.styles = 1});
    UiStyle s = nnh::ui::default_theme().button;
    const nnh::UiStyleId id = ui.add_style(s);
    const UiWidget b = ui.button(ui.root(Screen::Bottom), {.text = "ab", .style = id});
    CHECK(ui.rect(b).w == 28);
    const int layouts = ui.stats().layouts;
    s.states[0].background.color = nnh::colors::green;
    ui.set_style_value(id, s);
    (void)ui.rect(b);
    CHECK(ui.stats().layouts == layouts);
    s.padding[0] = 20;
    ui.set_style_value(id, s);
    CHECK(ui.rect(b).w == 20 + 16 + 6);
    s.text.scale = 2;
    ui.set_style_value(id, s);
    CHECK(ui.rect(b).w == 20 + 32 + 6 && ui.rect(b).h == 20 + 6);
}

NNH_TEST(ui_style_a_game_theme_replaces_the_default) {
    nnh::UiTheme theme = nnh::ui::default_theme();
    theme.button.padding[0] = theme.button.padding[2] = 10;
    Ui ui = ui_test::make();
    const UiWidget b = ui.button(ui.root(Screen::Bottom), {.text = "ab"});
    CHECK(ui.rect(b).w == 28);
    ui.set_theme(theme);
    CHECK(ui.rect(b).w == 36);
    ui.set_theme(nnh::ui::default_theme());
    CHECK(ui.rect(b).w == 28);
}

NNH_TEST(ui_style_a_theme_from_game_data_values_applies) {
    // what a game's own data type would hold after spec 014 loaded it (docs/guides/menus.md)
    struct ThemeFile {
        Color face, ink;
        int padding;
    } file{nnh::rgb(10, 20, 30), nnh::rgb(250, 240, 230), 3};
    nnh::UiTheme theme = nnh::ui::default_theme();
    for (nnh::UiLook& l : theme.button.states) {
        l.background.color = file.face;
        l.text = file.ink;
    }
    for (std::uint8_t& p : theme.button.padding) p = static_cast<std::uint8_t>(file.padding);
    Ui ui = ui_test::make();
    ui.set_theme(theme);
    const UiWidget b = ui.button(ui.root(Screen::Bottom), {.text = "ab"});
    CHECK(ui.rect(b).w == 16 + 6 && ui.rect(b).h == 10 + 6);
}

NNH_TEST(ui_style_texture_slots_are_0_to_7) {
    static Ui* g;
    Ui ui = ui_test::make();
    g = &ui;
    CHECK(ui_test::asserts_or_is_ignored([] {
        static nnh::Texture t;
        g->set_texture(8, t);
    }));
    CHECK(ui_test::asserts_or_is_ignored([] {
        const nnh::UiStyle style; // a named value: GCC 13/14 on Linux refuse add_style({}) here
        (void)g->add_style(style);
    })); // UiDesc::styles is 0
}
