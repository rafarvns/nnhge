// The bench's options screen (spec 028 T7.5, AC-6): the UI's items equal the gfx calls written out by hand in
// hand_options.cpp, so the bench's draw ratio compares like with like.

#include "../ui/ui_draw_fixture.hpp"
#include "nnh/texture.hpp"
#include "ui/hand_options.hpp"

namespace {
ui_test::Item g_ui[600], g_hand[600];
}

NNH_TEST(ui_example_options_screen_draws_like_its_hand_written_calls) {
    ui_test::restart();
    {
        auto tex = nnh::load_texture("tile"); // the fake's 16x16 image stands in for the art
        CHECK(tex.ok());
        OptionsScreen screen;
        CHECK(screen.build(&tex.value()));
        const int n = ui_test::capture([&] { screen.ui().draw(); }, g_ui, 600);
        const int m = ui_test::capture([&] { screen.draw_by_hand(); }, g_hand, 600);
        CHECK(n > 340 && n == screen.ui().stats().draws); // 18 frame sprites and about 350 glyphs
        CHECK(ui_test::same_items(g_ui, n, g_hand, m));
    }
    nnh::shutdown();
}
