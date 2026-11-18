// nnh::core frame loop, driven through the fake platform.

#include "debug/debug_list.hpp"
#include "debug/dev_tools.hpp"
#include "nnh/app.hpp"
#include "nnh/debug.hpp"
#include "nnh/gfx.hpp"
#include "nnh/input.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;

namespace {

constexpr std::uint32_t kA = static_cast<std::uint32_t>(nnh::Button::A);

bool same(nnh::Color a, nnh::Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

void fresh_engine() {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init());
}

} // namespace

NNH_TEST(frame_returns_false_before_init) {
    nnh::shutdown();
    fake::reset();
    CHECK(!nnh::frame());
    CHECK(fake::state().frames_begun == 0);
}

NNH_TEST(init_fails_when_platform_init_fails) {
    nnh::shutdown();
    fake::reset();
    fake::set_init_fails(true);
    CHECK(!nnh::init());
    CHECK(!nnh::frame());
}

NNH_TEST(each_frame_ends_the_previous_one_and_begins_a_new_one) {
    fresh_engine();
    CHECK(nnh::frame());
    CHECK(fake::state().frames_begun == 1 && fake::state().frames_ended == 0);
    CHECK(nnh::frame());
    CHECK(fake::state().frames_begun == 2 && fake::state().frames_ended == 1);
    CHECK(nnh::frame_count() == 2);
    nnh::shutdown();
}

NNH_TEST(shutdown_ends_the_open_frame_and_the_platform) {
    fresh_engine();
    CHECK(nnh::frame());
    nnh::shutdown();
    CHECK(fake::state().frames_ended == 1);
    CHECK(fake::state().shutdown_calls == 1);
    nnh::shutdown(); // idempotent
    CHECK(fake::state().shutdown_calls == 1);
}

NNH_TEST(frame_returns_false_when_platform_asks_to_quit) {
    fresh_engine();
    fake::set_quit_requested(true);
    CHECK(!nnh::frame());
    nnh::shutdown();
}

NNH_TEST(buttons_from_platform_are_reported_as_pressed_once) {
    fresh_engine();
    fake::set_held_buttons(kA);
    CHECK(nnh::frame());
    CHECK(nnh::input::pressed(nnh::Button::A));
    CHECK(nnh::frame());
    CHECK(!nnh::input::pressed(nnh::Button::A));
    CHECK(nnh::input::held(nnh::Button::A));
    nnh::shutdown();
}

NNH_TEST(clear_colors_reach_the_platform_on_the_next_frame) {
    fresh_engine();
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::colors::red);
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::colors::blue);
    CHECK(nnh::frame());
    CHECK(same(fake::state().top_clear, nnh::colors::red));
    CHECK(same(fake::state().bottom_clear, nnh::colors::blue));
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::colors::black);
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::colors::black);
    nnh::shutdown();
}

NNH_TEST(debug_print_is_drawn_inside_a_frame_and_ignored_outside) {
    fresh_engine();
    nnh::debug::print(nnh::Screen::Top, 1, 2, "outside");
    CHECK(nnh::frame());
    nnh::debug::print(nnh::Screen::Bottom, 3, 4, "inside");
    CHECK(fake::state().item_count == 0); // recorded, drawn on top at the end of the frame
    CHECK(nnh::frame());
    const nnh::detail::DebugList& last = nnh::detail::debug_list_last_frame();
    CHECK(last.count() == 1); // "outside" was ignored
    CHECK(last.at(0).screen == nnh::Screen::Bottom && last.at(0).x == 3 && last.at(0).y == 4);
    CHECK(fake::state().last_frame_item_count == 6); // one glyph per letter of "inside"
    nnh::shutdown();
}
