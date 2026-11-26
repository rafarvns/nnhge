// Per-kind drawing (spec 028 T6.3, FR-5, FR-6, FR-25, FR-26): each kind's items against the gfx calls written by hand
// from the default theme's colours and the rectangles worked out on paper.

#include "nnh/texture.hpp"
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

Item g_ui[256], g_hand[256];
constexpr nnh::ShapeOptions kShape{.layer = 14, .ignore_camera = true};
constexpr Color kFace = nnh::rgb(44, 52, 84), kInk = nnh::rgb(236, 238, 246);

nnh::TextOptions ink(Color c = kInk) { return {.color = c, .layer = 14, .ignore_camera = true}; }

template <typename Hand>
bool draws_like(Ui& ui, Hand&& hand) {
    const int n = capture([&] { ui.draw(); }, g_ui, 256);
    const int m = capture(hand, g_hand, 256);
    return ui_test::same_items(g_ui, n, g_hand, m);
}

} // namespace

NNH_TEST(ui_drawn_label_aligned_and_wrapped) {
    ui_test::restart();
    {
        Ui ui = ui_test::make();
        ui.label(ui.root(Screen::Top), {.text = "one two three", .layout = {.width = 80, .height = 40},
                                        .wrap = true, .align = nnh::TextAlign::Center, .valign = nnh::UiAlign::End});
        // 10 cells: "one two" (7 → x 12) / "three" (5 → x 20); two lines 20 px at the bottom of 40: y 20, 30
        CHECK(draws_like(ui, [] {
            gfx::draw_text(Screen::Top, {12, 20}, "one two", ink());
            gfx::draw_text(Screen::Top, {20, 30}, "three", ink());
        }));
    }
    nnh::shutdown();
}

NNH_TEST(ui_drawn_label_max_lines_with_ellipsis) {
    ui_test::restart();
    {
        Ui ui = ui_test::make();
        ui.label(ui.root(Screen::Top), {.text = "alpha beta gamma delta", .layout = {.width = 64}, .wrap = true,
                                        .max_lines = 2});
        CHECK(draws_like(ui, [] {
            gfx::draw_text(Screen::Top, {0, 0}, "alpha", ink());
            gfx::draw_text(Screen::Top, {0, 10}, "beta...", ink()); // "beta gam" cut to 5 cells, its space dropped
        }));
    }
    nnh::shutdown();
}

NNH_TEST(ui_drawn_button_face_and_centred_text) {
    ui_test::restart();
    {
        Ui ui = ui_test::make();
        ui.button(ui.root(Screen::Bottom), {.text = "Go", .layout = {.width = 60, .height = 20}});
        CHECK(draws_like(ui, [] {
            gfx::fill_rect(Screen::Bottom, {0, 0, 60, 20}, kFace, kShape);
            gfx::draw_text(Screen::Bottom, {22, 5}, "Go", ink()); // content 6..54 (48): (48 - 16) / 2 = 16 → 22
        }));
    }
    nnh::shutdown();
}

NNH_TEST(ui_drawn_toggle_box_and_check) {
    ui_test::restart();
    {
        Ui ui = ui_test::make();
        const UiWidget t = ui.toggle(ui.root(Screen::Bottom), {.text = "On", .checked = true});
        (void)t;
        // content (6, 3) 10 + 4 + 16 wide, 10 tall
        CHECK(draws_like(ui, [] {
            gfx::fill_rect(Screen::Bottom, {0, 0, 42, 16}, kFace, kShape);
            gfx::draw_rect(Screen::Bottom, {6, 3, 10, 10}, kInk, kShape);
            gfx::fill_rect(Screen::Bottom, {8, 5, 6, 6}, kInk, kShape);
            gfx::draw_text(Screen::Bottom, {20, 3}, "On", ink());
        }));
    }
    nnh::shutdown();
}

NNH_TEST(ui_drawn_slider_track_fill_knob_value) {
    ui_test::restart();
    {
        Ui ui = ui_test::make();
        ui.slider(ui.root(Screen::Bottom), {.text = "V", .min = 0, .max = 10, .value = 5, .layout = {.width = 112}});
        // content x 6..106 (100): label 8 + 4 → track x 18, value "10" 16 wide at 90, track 18..86 (68)
        // knob 6 wide: (68 - 6) × 5 / 10 = 31 → x 49; track 4 tall centred in 10: y 3 + 3
        CHECK(draws_like(ui, [] {
            gfx::fill_rect(Screen::Bottom, {0, 0, 112, 16}, kFace, kShape);
            gfx::draw_text(Screen::Bottom, {6, 3}, "V", ink());
            gfx::fill_rect(Screen::Bottom, {18, 6, 68, 4}, nnh::rgb(90, 100, 140), kShape);
            gfx::fill_rect(Screen::Bottom, {18, 6, 31, 4}, nnh::rgb(120, 180, 255), kShape);
            gfx::fill_rect(Screen::Bottom, {49, 3, 6, 10}, nnh::colors::white, kShape);
            gfx::draw_text(Screen::Bottom, {98, 3}, "5", ink()); // right-aligned at 106
        }));
    }
    nnh::shutdown();
}

NNH_TEST(ui_drawn_choice_text_arrows_and_option) {
    ui_test::restart();
    {
        Ui ui = ui_test::make({.texts = 2});
        static const nnh::UiText opts[2] = {"Hard", "No"};
        ui.choice(ui.root(Screen::Bottom), {.options = opts, .count = 2});
        // no label: "<" at 6, option area 18..(6 + 48 + 8 = 62 - 4 = 58)... see choice_parts: widest 4 cells
        // content 6..78 (8 + 4 + 32 + 4 + 8 = 56 + 12 = 68 wide → content 56: 6..62): left 6, right 54,
        // option 18..50 (32): "Hard" at 18
        CHECK(draws_like(ui, [] {
            gfx::fill_rect(Screen::Bottom, {0, 0, 68, 16}, kFace, kShape);
            gfx::draw_text(Screen::Bottom, {6, 3}, "<", ink());
            gfx::draw_text(Screen::Bottom, {18, 3}, "Hard", ink());
            gfx::draw_text(Screen::Bottom, {54, 3}, ">", ink());
        }));
    }
    nnh::shutdown();
}

NNH_TEST(ui_drawn_progress_fills_both_ways) {
    ui_test::restart();
    {
        Ui ui = ui_test::make();
        ui.progress(ui.root(Screen::Top), {.value = 3, .max = 10, .layout = {.width = 102, .height = 12}});
        ui.progress(ui.root(Screen::Top),
                    {.value = 1, .max = 4, .vertical = true, .layout = {.width = 10, .height = 42, .place = {.y = 50}}});
        const Color bg = nnh::rgb(30, 34, 50), edge = nnh::rgb(96, 108, 150), fill = nnh::rgb(84, 196, 120);
        CHECK(draws_like(ui, [&] {
            gfx::fill_rect(Screen::Top, {0, 0, 102, 12}, bg, kShape);
            gfx::draw_rect(Screen::Top, {0, 0, 102, 12}, edge, kShape);
            gfx::fill_rect(Screen::Top, {1, 1, 30, 10}, fill, kShape); // 100 × 3 / 10
            gfx::fill_rect(Screen::Top, {0, 50, 10, 42}, bg, kShape);
            gfx::draw_rect(Screen::Top, {0, 50, 10, 42}, edge, kShape);
            gfx::fill_rect(Screen::Top, {1, 51 + 30, 8, 10}, fill, kShape); // 40 × 1 / 4 from the bottom
        }));
    }
    nnh::shutdown();
}

NNH_TEST(ui_drawn_list_rows_numbers_and_scroll_bar) {
    ui_test::restart();
    {
        Ui ui = ui_test::make({.rows = 4});
        ui.list(ui.root(Screen::Bottom), {.rows = 2, .count = 4, .fill = [](int i, nnh::UiRow& r) {
                                              r.text = i == 0 ? "Potion" : "Ether";
                                              r.number = i + 1;
                                              r.number_prefix = "x";
                                          },
                                          .cell_width = 100});
        ui.update(nnh::UiInput{}); // the list takes the focus
        const Color list_bg = nnh::rgb(28, 32, 50), focus = nnh::rgb(244, 204, 72), dark = nnh::rgb(22, 22, 32);
        CHECK(draws_like(ui, [&] {
            gfx::fill_rect(Screen::Bottom, {0, 0, 104, 24}, list_bg, kShape);
            gfx::fill_rect(Screen::Bottom, {0, 0, 100, 12}, focus, kShape); // row 0, focused
            gfx::draw_text(Screen::Bottom, {82, 1}, "x1", ink(dark));
            gfx::draw_text(Screen::Bottom, {2, 1}, "Potion", ink(dark));
            gfx::draw_text(Screen::Bottom, {82, 13}, "x2", ink());
            gfx::draw_text(Screen::Bottom, {2, 13}, "Ether", ink());
            gfx::fill_rect(Screen::Bottom, {100, 0, 4, 24}, nnh::rgb(40, 44, 64), kShape); // track
            gfx::fill_rect(Screen::Bottom, {100, 0, 4, 12}, nnh::rgb(150, 160, 200), kShape); // thumb: 24 × 2 / 4
        }));
    }
    nnh::shutdown();
}

NNH_TEST(ui_drawn_image_and_nine_slice) {
    ui_test::restart();
    {
        auto tex = nnh::load_texture("tile");
        CHECK(tex.ok());
        const nnh::Texture& t = tex.value();
        Ui ui = ui_test::make({.styles = 1});
        ui.set_texture(2, t);
        nnh::UiStyle frame = nnh::ui::default_theme().panel;
        for (nnh::UiLook& l : frame.states)
            l.background = {.kind = nnh::UiBackgroundKind::NineSlice, .texture = 2, .region = {0, 0, 16, 16},
                            .left = 4, .top = 4, .right = 4, .bottom = 4};
        const nnh::UiStyleId id = ui.add_style(frame);
        const UiWidget p = ui.panel(ui.root(Screen::Top), {.style = id, .layout = {.width = 40, .height = 20}});
        (void)p;
        ui.image(ui.root(Screen::Top), {.texture = 2, .region = {4, 4, 8, 8}, .scale = 2, .layout = {.place = {.y = 30}}});
        const int n = capture([&] { ui.draw(); }, g_ui, 256);
        CHECK(n == 9 + 1);
        CHECK(g_ui[4].x == 4 && g_ui[4].y == 4 && g_ui[4].w == 32 && g_ui[4].h == 12); // the centre: 8 → 32, 8 → 12
        CHECK(g_ui[4].region.x == 4 && g_ui[4].region.w == 8);
        CHECK(g_ui[9].x == 0 && g_ui[9].y == 30 && g_ui[9].w == 16 && g_ui[9].h == 16 && g_ui[9].region.w == 8);
    }
    nnh::shutdown();
}

NNH_TEST(ui_drawn_label_with_a_carriage_return_keeps_every_character) {
    ui_test::restart();
    {
        Ui ui = ui_test::make();
        ui.label(ui.root(Screen::Top), {.text = "ab\rc"}); // '\r' takes no cell (as gfx and the wrapper count)
        CHECK(draws_like(ui, [] { gfx::draw_text(Screen::Top, {0, 0}, "ab\rc", ink()); }));
        CHECK(ui.stats().draws == 3); // nor a glyph
        Ui revealed = ui_test::make();
        const UiWidget l = revealed.label(revealed.root(Screen::Top), {.text = "ab\rcd", .reveal_rate = 0});
        revealed.set_reveal(l, 3);
        CHECK(draws_like(revealed, [] { gfx::draw_text(Screen::Top, {0, 0}, "ab\rc", ink()); }));
    }
    nnh::shutdown();
}

NNH_TEST(ui_drawn_label_of_several_lines_aligns_each_line) {
    ui_test::restart();
    {
        Ui ui = ui_test::make();
        ui.label(ui.root(Screen::Top), {.text = "a\nbcd", .layout = {.width = 40}, .align = nnh::TextAlign::Center});
        CHECK(draws_like(ui, [] {
            gfx::draw_text(Screen::Top, {16, 0}, "a", ink());    // 1 cell in 40: (40 - 8) / 2
            gfx::draw_text(Screen::Top, {8, 10}, "bcd", ink());  // 3 cells: (40 - 24) / 2
        }));
    }
    nnh::shutdown();
}

NNH_TEST(ui_draw_count_skips_both_kinds_of_space) {
    ui_test::restart();
    {
        Ui ui = ui_test::make();
        ui.label(ui.root(Screen::Top), {.text = "a\xC2\xA0" "b \xC3\xA9"}); // a, no-break space, b, space, e acute
        CHECK(draws_like(ui, [] { gfx::draw_text(Screen::Top, {0, 0}, "a\xC2\xA0" "b \xC3\xA9", ink()); }));
        CHECK(ui.stats().draws == 3);
    }
    nnh::shutdown();
}

NNH_TEST(ui_drawn_nearly_opaque_parent_still_fades) {
    ui_test::restart();
    {
        Ui ui = ui_test::make();
        const UiWidget box = ui.vbox(ui.root(Screen::Top));
        ui.label(box, {.text = "a"});
        ui.set_opacity(box, 254.0f / 255.0f);
        Color faded = kInk;
        faded.a = 254; // (255 × 254 + 127) / 255
        CHECK(draws_like(ui, [&] { gfx::draw_text(Screen::Top, {0, 0}, "a", ink(faded)); }));
    }
    nnh::shutdown();
}
