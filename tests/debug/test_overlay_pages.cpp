// The overlay's pages (spec 030 T4.1, FR-7, FR-10, FR-16): SELECT + R cycles, the panel takes the controls on a live
// page (the game's queries read idle), a button held when the overlay closes stays blocked until released, the game's
// own block is independent, the chords reach the pause, a game without live values shows its note.

#include <cstring>

#include "app/dev_pause.hpp"
#include "debug/debug_list.hpp"
#include "debug/dev_tools.hpp"
#include "debug/overlay_pages.hpp"
#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/debug.hpp"
#include "nnh/input.hpp"
#include "nnh/platform/system.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
using nnh::Button;

namespace {

constexpr std::uint32_t bit(Button b) { return static_cast<std::uint32_t>(b); }

void restart() {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init({}));
}

/// Holds SELECT, presses `b` for a frame, lets go of both.
void chord(Button b) {
    fake::set_held_buttons(bit(Button::Select));
    CHECK(nnh::frame());
    fake::set_held_buttons(bit(Button::Select) | bit(b));
    CHECK(nnh::frame());
    fake::set_held_buttons(0);
    CHECK(nnh::frame());
}

} // namespace

NNH_TEST(overlay_pages_select_l_shows_and_select_r_cycles) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    chord(Button::L);
    CHECK(nnh::debug::overlay_visible());
    CHECK(nnh::detail::overlay_page() == nnh::detail::kPageStats);
    chord(Button::R);
    CHECK(nnh::detail::overlay_page() == 1);
    chord(Button::R);
    CHECK(nnh::detail::overlay_page() == 2);
    chord(Button::R);
    CHECK(nnh::detail::overlay_page() == 0);
    chord(Button::L);
    CHECK(!nnh::debug::overlay_visible());
}

NNH_TEST(overlay_pages_a_live_page_takes_the_controls_until_hidden) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    chord(Button::L);
    chord(Button::R); // Tunables
    fake::set_held_buttons(bit(Button::A));
    CHECK(nnh::frame());
    CHECK(!nnh::input::held(Button::A) && !nnh::input::pressed(Button::A)); // the game reads idle
    // SELECT + L with A still held: the overlay closes, A stays blocked until released
    fake::set_held_buttons(bit(Button::A) | bit(Button::Select));
    CHECK(nnh::frame());
    fake::set_held_buttons(bit(Button::A) | bit(Button::Select) | bit(Button::L));
    CHECK(nnh::frame());
    CHECK(!nnh::debug::overlay_visible());
    fake::set_held_buttons(bit(Button::A));
    CHECK(nnh::frame());
    CHECK(!nnh::input::held(Button::A));
    fake::set_held_buttons(0);
    CHECK(nnh::frame());
    fake::set_held_buttons(bit(Button::A));
    CHECK(nnh::frame());
    CHECK(nnh::input::pressed(Button::A)); // the game has its controls back
    fake::set_held_buttons(0);
}

NNH_TEST(overlay_pages_the_stats_page_leaves_the_game_its_controls) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    chord(Button::L);
    fake::set_held_buttons(bit(Button::A));
    CHECK(nnh::frame());
    CHECK(nnh::input::pressed(Button::A));
    fake::set_held_buttons(0);
}

NNH_TEST(overlay_pages_the_games_own_block_is_independent) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    nnh::input::set_blocked(true);
    chord(Button::L);
    chord(Button::R);
    chord(Button::L); // the panel lets go: the game's block stays
    CHECK(nnh::input::blocked());
    nnh::input::set_blocked(false);
}

NNH_TEST(overlay_pages_chords_pause_and_step) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    chord(Button::L);
    chord(Button::R);
    fake::set_held_buttons(bit(Button::Select));
    CHECK(nnh::frame());
    // SELECT + Y pauses at this frame's start; the paused ticks then see SELECT + Y released and pressed again
    fake::set_held_buttons(bit(Button::Select) | bit(Button::Y));
    fake::queue_dev_requests({0, nnh::platform::kDevTogglePause});
    const std::uint64_t frame = nnh::frame_count();
    CHECK(nnh::frame());
    CHECK(nnh::frame_count() == frame + 1);
    fake::set_held_buttons(0);
}

NNH_TEST(overlay_pages_without_live_values_show_a_note) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    nnh::detail::overlay_show_page(2);
    CHECK(nnh::frame());
    CHECK(nnh::frame());
    const nnh::detail::DebugList& l = nnh::detail::debug_list_last_frame();
    bool note = false;
    for (int i = 0; i < l.count(); ++i) note = note || std::strcmp(l.text(i), "no live values in this game") == 0;
    CHECK(note);
}
