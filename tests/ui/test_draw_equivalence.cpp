// Draw equivalence (spec 028 T6.2, FR-28): an options-like screen's items equal the items of the same public gfx calls
// written out by hand from the UI's queried rectangles and the theme's colours — the UI records exactly what a
// hand-written menu would (AC-6's ratio compares like with like).

#include "ui_draw_fixture.hpp"

using nnh::Color;
using nnh::Screen;
using nnh::Ui;
using nnh::UiRect;
using nnh::UiWidget;
using ui_test::capture;
using ui_test::Item;
namespace gfx = nnh::gfx;

namespace {

Item g_ui[512], g_hand[512];

} // namespace

NNH_TEST(ui_an_options_screen_draws_like_hand_written_calls) {
    ui_test::restart();
    {
        Ui ui = ui_test::make({.texts = 3});
        const UiWidget panel = ui.panel(ui.root(Screen::Bottom),
                                        {.as_box = true, .spacing = 2, .layout = {.width = 240, .place = {.anchor = nnh::UiAnchor::Center}}});
        const UiWidget title = ui.label(panel, {.text = "Options", .align = nnh::TextAlign::Center});
        const UiWidget music = ui.slider(panel, {.text = "Music", .value = 7});
        const UiWidget invert = ui.toggle(panel, {.text = "Invert Y"});
        static const nnh::UiText opts[3] = {"Easy", "Normal", "Hard"};
        const UiWidget diff = ui.choice(panel, {.text = "Level", .options = opts, .count = 3, .index = 1});
        const UiWidget back = ui.button(panel, {.text = "Back"});
        ui.update(nnh::UiInput{}); // focus: the slider
        const int n = capture([&] { ui.draw(); }, g_ui, 512);

        const nnh::UiTheme& t = nnh::ui::default_theme();
        const nnh::ShapeOptions shape{.layer = 14, .ignore_camera = true};
        auto text = [](Color c) { return nnh::TextOptions{.color = c, .layer = 14, .ignore_camera = true}; };
        const UiRect p = ui.rect(panel), ti = ui.rect(title), mu = ui.rect(music), in = ui.rect(invert),
                     di = ui.rect(diff), ba = ui.rect(back);
        const Color focus_ink = t.slider.states[1].text, ink = t.button.states[0].text;
        const int m = capture([&] {
            gfx::fill_rect(Screen::Bottom, p.rect(), t.panel.states[0].background.color, shape);
            gfx::draw_rect(Screen::Bottom, p.rect(), t.panel.states[0].background.border_color, shape);
            gfx::draw_text(Screen::Bottom, {static_cast<float>(ti.x + (ti.w - 56) / 2), static_cast<float>(ti.y)},
                           "Options", text(t.label.states[0].text));
            // the slider, focused: face, label, track, fill, knob, value
            gfx::fill_rect(Screen::Bottom, mu.rect(), t.slider.states[1].background.color, shape);
            const int cx = mu.x + 6, cy = mu.y + 3, cw = mu.w - 12;
            gfx::draw_text(Screen::Bottom, {float(cx), float(cy)}, "Music", text(focus_ink));
            const int tx = cx + 44, tw = cw - 44 - 20;
            const int kx = tx + (tw - 6) * 7 / 10;
            gfx::fill_rect(Screen::Bottom, {float(tx), float(cy + 3), float(tw), 4}, t.slider.slider.track.color, shape);
            gfx::fill_rect(Screen::Bottom, {float(tx), float(cy + 3), float(kx - tx), 4}, t.slider.slider.fill.color, shape);
            gfx::fill_rect(Screen::Bottom, {float(kx), float(cy), 6, 10}, t.slider.slider.knob.color, shape);
            gfx::draw_text(Screen::Bottom, {float(cx + cw - 8), float(cy)}, "7", text(focus_ink));
            // the toggle, off
            gfx::fill_rect(Screen::Bottom, in.rect(), t.toggle.states[0].background.color, shape);
            gfx::draw_rect(Screen::Bottom, {float(in.x + 6), float(in.y + 3), 10, 10}, ink, shape);
            gfx::draw_text(Screen::Bottom, {float(in.x + 20), float(in.y + 3)}, "Invert Y", text(ink));
            // the choice: label, arrows, the option centred between them
            gfx::fill_rect(Screen::Bottom, di.rect(), t.choice.states[0].background.color, shape);
            const int dx = di.x + 6, dw = di.w - 12;
            gfx::draw_text(Screen::Bottom, {float(dx), float(di.y + 3)}, "Level", text(ink));
            gfx::draw_text(Screen::Bottom, {float(dx + 44), float(di.y + 3)}, "<", text(ink));
            const int ox = dx + 44 + 12, ow = (dx + dw - 8 - 4) - ox;
            gfx::draw_text(Screen::Bottom, {float(ox + (ow - 48) / 2), float(di.y + 3)}, "Normal", text(ink));
            gfx::draw_text(Screen::Bottom, {float(dx + dw - 8), float(di.y + 3)}, ">", text(ink));
            // the button
            gfx::fill_rect(Screen::Bottom, ba.rect(), t.button.states[0].background.color, shape);
            gfx::draw_text(Screen::Bottom, {float(ba.x + 6 + (ba.w - 12 - 32) / 2), float(ba.y + 3)}, "Back", text(ink));
        }, g_hand, 512);
        CHECK(n > 40);
        CHECK(ui_test::same_items(g_ui, n, g_hand, m));
        CHECK(ui.stats().draws == n);
    }
    nnh::shutdown();
}
