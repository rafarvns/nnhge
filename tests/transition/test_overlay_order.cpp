// Where the overlay draws (spec 023 FR-7): after every scene's draws (above their layer 15), below the debug pass,
// in screen pixels whatever the camera, and below what a manual loop draws after scenes::update_and_draw().

#include "nnh/debug.hpp"
#include "nnh/platform/video.hpp"
#include "transition_fixture.hpp"

using namespace tr_test;
using nnh::Frames;
using nnh::Screen;
using Item = nnh::platform::DrawItem2D;

namespace {

/// The top screen's items of the last frame when the bottom screen has no overlay: scenes' items first.
bool is_overlay(const Item& it) {
    return it.kind == Item::Kind::FillRect && it.x == 0 && it.y == 0 && it.w == 400 && it.h == 240 &&
           it.color.r == 0 && it.color.g == 0 && it.color.b == 0 && it.layer == 15;
}

} // namespace

NNH_TEST(the_overlay_comes_after_every_scenes_draws_even_their_layer_15) {
    restart();
    Probe a(nnh::colors::red), b(nnh::colors::blue);
    nnh::scenes::push(a);
    nnh::scenes::push(b);
    step();
    CHECK(tr::cover({.bottom = tr::none(), .out = Frames{2}}));
    step();
    CHECK(nnh::frame());
    const fake::State& s = fake::state();
    // Top: A and B layer 0, A and B layer 15, then the overlay; bottom: the scenes' four items.
    CHECK(s.last_frame_item_count == 9);
    CHECK(s.last_frame_items[2].layer == 15 && s.last_frame_items[3].layer == 15);
    CHECK(is_overlay(s.last_frame_items[4]) && s.last_frame_items[4].color.a == 128);
    nnh::shutdown();
}

NNH_TEST(the_overlay_ignores_the_camera_and_its_zoom) {
    restart();
    Probe a;
    nnh::scenes::push(a);
    step();
    nnh::gfx::set_camera(Screen::Top, {{57, -31}, 2.0f});
    CHECK(tr::cover({.bottom = tr::none(), .out = Frames{1}}));
    step();
    CHECK(nnh::frame());
    const fake::State& s = fake::state();
    bool found = false;
    for (int i = 0; i < s.last_frame_item_count; ++i) found = found || is_overlay(s.last_frame_items[i]);
    CHECK(found);
    nnh::shutdown();
}

NNH_TEST(a_manual_loops_draw_after_update_and_draw_is_above_the_overlay) {
    restart();
    Probe a;
    nnh::scenes::push(a);
    step();
    CHECK(tr::cover({.bottom = tr::none(), .out = Frames{1}}));
    CHECK(nnh::frame());
    nnh::scenes::update_and_draw();
    nnh::gfx::fill_rect(Screen::Top, {1, 2, 3, 4}, nnh::colors::yellow, {.layer = 15}); // a HUD over the transition
    CHECK(nnh::frame());
    const fake::State& s = fake::state();
    CHECK(s.last_frame_item_count == 4 + 1 + 1);
    CHECK(is_overlay(s.last_frame_items[2]) && s.last_frame_items[3].color.b == 0 &&
          s.last_frame_items[3].w == 3);
    nnh::shutdown();
}

NNH_TEST(debug_text_stays_above_the_overlay) {
    if (!nnh::build::kDevToolsEnabled) return;
    restart();
    struct Printing : Probe {
        void draw() override {
            Probe::draw();
            nnh::debug::print(Screen::Top, 0, 0, "HI");
        }
    } a;
    nnh::scenes::push(a);
    step();
    CHECK(tr::cover({.bottom = tr::none(), .out = Frames{1}}));
    step();
    CHECK(nnh::frame());
    const fake::State& s = fake::state();
    int overlay = -1, last_sprite = -1;
    for (int i = 0; i < s.last_frame_item_count && i < fake::kMaxRecordedItems; ++i) {
        if (is_overlay(s.last_frame_items[i])) overlay = i;
        if (s.last_frame_items[i].kind == Item::Kind::Sprite) last_sprite = i;
    }
    CHECK(overlay >= 0 && last_sprite > overlay); // the debug pass's glyphs come later
    nnh::shutdown();
}
