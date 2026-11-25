// The focus cursor (spec 028 T6.4, FR-27, FR-29): its glide in whole pixels pinned for CubicOut over 6 frames, a
// jump with move_frames 0, the pulse cycle, the pressed look's 6 frames (test_values.cpp), the image placed outside the
// widget at its anchor.

#include "ui/ui_block.hpp"
#include "nnh/texture.hpp"
#include "ui/ui_cursor.hpp"
#include "ui_draw_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiWidget;

namespace {

/// Two buttons 24 px apart (16 tall, spacing 8): the cursor's left-centre point goes from y 8 to y 32.
struct Two {
    Ui ui = ui_test::make();
    UiWidget a, b;
    explicit Two(const nnh::UiTheme* theme = nullptr) {
        if (theme != nullptr) ui.set_theme(*theme);
        const UiWidget box = ui.vbox(ui.root(Screen::Bottom), {.spacing = 8});
        a = ui.button(box, {.text = "a"});
        b = ui.button(box, {.text = "b"});
        ui_test::settle(ui);
    }
    int y() const { return nnh::UiAccess::block(ui)->cursor.y; }
};

} // namespace

NNH_TEST(ui_cursor_glides_with_cubic_out_in_whole_pixels) {
    Two t;
    CHECK(t.y() == 8);
    t.ui.update(ui_test::press(ui_test::kDown));
    const int expected[6] = {18, 25, 29, 31, 32, 32}; // 8 + 24 × (1 − (1 − k/6)³), rounded once
    bool ok = t.y() == expected[0];
    for (int k = 1; k < 6; ++k) {
        t.ui.update(nnh::UiInput{});
        ok = ok && t.y() == expected[k];
    }
    CHECK(ok);
}

NNH_TEST(ui_cursor_jumps_with_move_frames_0) {
    nnh::UiTheme theme = nnh::ui::default_theme();
    theme.cursor.move_frames = 0;
    Two t(&theme);
    t.ui.update(ui_test::press(ui_test::kDown));
    CHECK(t.y() == 32);
}

NNH_TEST(ui_cursor_lerp_rounds_once) {
    CHECK(nnh::detail::cursor_lerp(0, 10, 0.25f) == 3);  // 2.5 → 3
    CHECK(nnh::detail::cursor_lerp(10, 0, 0.25f) == 8);  // 7.5 → 8
    CHECK(nnh::detail::cursor_lerp(0, -10, 0.25f) == -2); // -2.5 → -2 (floor(x + 0.5))
}

NNH_TEST(ui_cursor_pulses_in_whole_frames) {
    nnh::UiTheme theme = nnh::ui::default_theme();
    theme.cursor.pulse_frames = 4;
    theme.cursor.pulse_opacity = 100;
    theme.cursor.pulse_x = 2;
    Two t(&theme);
    const nnh::detail::UiBlock& b = *nnh::UiAccess::block(t.ui);
    int opacity[4], dx[4];
    for (int k = 0; k < 4; ++k) {
        const nnh::detail::CursorPulse p = nnh::detail::cursor_pulse(b);
        opacity[k] = p.opacity;
        dx[k] = p.dx;
        t.ui.update(nnh::UiInput{});
    }
    // phases 1, 2, 3, 0 (one update ran already): amounts 1, 2, 1, 0 of 2
    CHECK(opacity[0] == 178 && opacity[1] == 100 && opacity[2] == 178 && opacity[3] == 255);
    CHECK(dx[0] == 1 && dx[1] == 2 && dx[2] == 1 && dx[3] == 0);
}

NNH_TEST(ui_cursor_image_sits_outside_the_widget) {
    ui_test::restart();
    {
        auto tex = nnh::load_texture("tile");
        CHECK(tex.ok());
        nnh::UiTheme theme = nnh::ui::default_theme();
        theme.cursor.image = {.texture = 0, .region = {0, 0, 8, 6}};
        Two t(&theme);
        t.ui.set_texture(0, tex.value());
        ui_test::Item items[32];
        const int n = ui_test::capture([&] { t.ui.draw(); }, items, 32);
        // anchor Left: the image's right-centre meets (0, 8), then x - 2: x = 0 - 8 - 2, y = 8 - 3
        CHECK(items[n - 1].kind == ui_test::Item::Kind::Sprite && items[n - 1].x == -10 && items[n - 1].y == 5);
    }
    nnh::shutdown();
}
