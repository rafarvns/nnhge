// The drawing walk (spec 028 T6.2, FR-28, FR-7): items on the root's layer, unaffected by the camera's position and
// zoom (ignore_camera), parents before children, modals and the cursor last, hidden roots drawing nothing, nothing
// recorded outside a frame but the hash still computed, the draw count equal to gfx's, render offsets and opacity
// moving and fading items without a layout, missing glyphs as the hollow box.

#include "nnh/gfx.hpp"
#include "ui_draw_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiWidget;
using ui_test::capture;
using ui_test::Item;

namespace {

Item g_a[256], g_b[256];

struct Screenful {
    Ui ui = ui_test::make();
    UiWidget panel, label, button;
    Screenful() {
        panel = ui.panel(ui.root(Screen::Bottom), {.as_box = true, .layout = {.place = {.x = 20, .y = 30}}});
        label = ui.label(panel, {.text = "Hi"});
        button = ui.button(panel, {.text = "Go"});
        ui_test::settle(ui);
    }
};

} // namespace

NNH_TEST(ui_draw_on_the_default_layer_14) {
    ui_test::restart();
    {
        Screenful s;
        const int n = capture([&] { s.ui.draw(); }, g_a, 256);
        CHECK(n == 2 + 2 + 1 + 2); // panel fill + border, "Hi", button face, "Go"
        bool ok = true;
        for (int i = 0; i < n; ++i) ok = ok && g_a[i].layer == nnh::kUiDefaultLayer;
        CHECK(ok);
        s.ui.set_layer(Screen::Bottom, 9);
        capture([&] { s.ui.draw(); }, g_a, 256);
        CHECK(g_a[0].layer == 9);
    }
    nnh::shutdown();
}

NNH_TEST(ui_draw_parents_before_children) {
    ui_test::restart();
    {
        Screenful s;
        capture([&] { s.ui.draw(); }, g_a, 256);
        CHECK(g_a[0].kind == Item::Kind::FillRect && g_a[0].x == 20 && g_a[0].y == 30); // the panel
        CHECK(g_a[1].kind == Item::Kind::RectOutline);
        CHECK(g_a[2].kind == Item::Kind::Sprite && g_a[2].x == 26); // "H" at the panel's padding
    }
    nnh::shutdown();
}

NNH_TEST(ui_draw_ignores_the_camera) {
    ui_test::restart();
    {
        Screenful s;
        const int n = capture([&] { s.ui.draw(); }, g_a, 256);
        nnh::gfx::set_camera(Screen::Bottom, {.position = {100, 50}, .zoom = 2});
        const int m = capture([&] { s.ui.draw(); }, g_b, 256);
        CHECK(ui_test::same_items(g_a, n, g_b, m));
        nnh::gfx::set_camera(Screen::Bottom, {.position = {-7, 3}, .zoom = 0.5f});
        const int k = capture([&] { s.ui.draw(); }, g_b, 256);
        CHECK(ui_test::same_items(g_a, n, g_b, k));
    }
    nnh::shutdown();
}

NNH_TEST(ui_draw_a_hidden_root_draws_nothing) {
    ui_test::restart();
    {
        Screenful s;
        s.ui.set_root_visible(Screen::Bottom, false);
        CHECK(capture([&] { s.ui.draw(); }, g_a, 256) == 0);
        CHECK(s.ui.stats().draws == 0);
    }
    nnh::shutdown();
}

NNH_TEST(ui_draw_outside_a_frame_records_nothing_but_hashes) {
    ui_test::restart();
    {
        Screenful s;
        s.ui.draw(); // before the first frame: gfx ignores it
        const nnh::UiStats outside = s.ui.stats();
        const int n = capture([&] { s.ui.draw(); }, g_a, 256);
        CHECK(outside.draws == n && outside.draw_hash == s.ui.stats().draw_hash && outside.draw_hash != 0);
    }
    nnh::shutdown();
}

NNH_TEST(ui_draw_count_equals_gfx_draws) {
    ui_test::restart();
    {
        Ui ui = ui_test::make();
        ui.label(ui.root(Screen::Top), {.text = "a b  c\n\xC3\xA9"});
        ui.button(ui.root(Screen::Bottom), {.text = "Shadowed"});
        CHECK(nnh::frame());
        ui.draw();
        CHECK(nnh::frame());
        CHECK(ui.stats().draws == nnh::gfx::stats().draws_last_frame);
        CHECK(ui.stats().draws == 4 + 1 + 8);
    }
    nnh::shutdown();
}

NNH_TEST(ui_draw_render_offset_and_opacity_move_and_fade) {
    ui_test::restart();
    {
        Screenful s;
        const int n = capture([&] { s.ui.draw(); }, g_a, 256);
        s.ui.set_render_offset(s.panel, {10.4f, -5.6f}); // rounded: (10, -6)
        s.ui.set_opacity(s.panel, 0.5f);                 // 128 / 255
        const int m = capture([&] { s.ui.draw(); }, g_b, 256);
        CHECK(n == m);
        bool ok = true;
        for (int i = 0; i < n; ++i) {
            ok = ok && g_b[i].x == g_a[i].x + 10 && g_b[i].y == g_a[i].y - 6;
            ok = ok && g_b[i].color.a == (g_a[i].color.a * 128 + 127) / 255;
        }
        CHECK(ok);
        CHECK(s.ui.rect(s.panel).x == 20 && s.ui.rect(s.panel).y == 30); // rects unchanged
    }
    nnh::shutdown();
}

NNH_TEST(ui_draw_an_open_modal_draws_last) {
    ui_test::restart();
    {
        Ui ui = ui_test::make();
        const UiWidget m = ui.vbox(ui.root(Screen::Bottom));
        ui.button(m, {.text = "M"});
        ui.button(ui.root(Screen::Bottom), {.text = "after", .layout = {.place = {.y = 100}}});
        ui_test::settle(ui);
        ui.open_modal(m);
        const int n = capture([&] { ui.draw(); }, g_a, 256);
        CHECK(n == 1 + 5 + 1 + 1);
        CHECK(g_a[n - 1].kind == Item::Kind::Sprite && g_a[n - 2].kind == Item::Kind::FillRect); // "M" on its face
        CHECK(g_a[0].y == 100); // the other button first
    }
    nnh::shutdown();
}

NNH_TEST(ui_draw_the_cursor_outline_draws_last) {
    ui_test::restart();
    {
        nnh::UiTheme theme = nnh::ui::default_theme();
        theme.cursor.outline = true;
        theme.cursor.outline_color = nnh::colors::red;
        Screenful s;
        s.ui.set_theme(theme);
        s.ui.update(nnh::UiInput{});
        const int n = capture([&] { s.ui.draw(); }, g_a, 256);
        const nnh::UiRect b = s.ui.rect(s.button);
        CHECK(g_a[n - 1].kind == Item::Kind::RectOutline && g_a[n - 1].x == b.x && g_a[n - 1].w == b.w);
        CHECK(g_a[n - 1].color.r == 255 && g_a[n - 1].color.g == 0);
    }
    nnh::shutdown();
}

NNH_TEST(ui_draw_missing_glyphs_draw_as_the_hollow_box) {
    ui_test::restart();
    {
        Ui ui = ui_test::make();
        ui.label(ui.root(Screen::Top), {.text = "\xE2\x82\xAC"}); // the euro sign: not in the built-in font
        const int n = capture([&] { ui.draw(); }, g_a, 256);
        CHECK(n == 1);
        const int hollow = capture([] { nnh::gfx::draw_text(Screen::Top, {0, 0}, "\xEF\xBF\xBD"); }, g_b, 256);
        CHECK(hollow == 1 && g_a[0].region.x == g_b[0].region.x && g_a[0].region.y == g_b[0].region.y);
    }
    nnh::shutdown();
}
