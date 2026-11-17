// Debug overlay: toggled by SELECT+L or a platform dev request, drawn at the end of each frame.

#include <cstring>

#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/debug.hpp"
#include "nnh/input.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;

namespace {

constexpr std::uint32_t kSelect = static_cast<std::uint32_t>(nnh::Button::Select);
constexpr std::uint32_t kL = static_cast<std::uint32_t>(nnh::Button::L);

void fresh() {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init());
    nnh::debug::set_overlay_visible(false);
}

bool drew_text_containing(const char* needle) {
    for (int i = 0; i < fake::state().text_count; ++i) {
        if (std::strstr(fake::state().texts[i].text, needle) != nullptr) return true;
    }
    return false;
}

} // namespace

NNH_TEST(select_plus_l_toggles_the_overlay) {
    if (!nnh::build::kDevToolsEnabled) return;
    fresh();
    fake::set_held_buttons(kSelect);
    CHECK(nnh::frame());
    CHECK(!nnh::debug::overlay_visible());
    fake::set_held_buttons(kSelect | kL);
    CHECK(nnh::frame());
    CHECK(nnh::debug::overlay_visible());
    CHECK(nnh::frame()); // still held: no second toggle
    CHECK(nnh::debug::overlay_visible());
    fake::set_held_buttons(kSelect);
    CHECK(nnh::frame());
    fake::set_held_buttons(kSelect | kL);
    CHECK(nnh::frame());
    CHECK(!nnh::debug::overlay_visible());
    nnh::shutdown();
}

NNH_TEST(l_alone_does_not_toggle_the_overlay) {
    if (!nnh::build::kDevToolsEnabled) return;
    fresh();
    fake::set_held_buttons(kL);
    CHECK(nnh::frame());
    CHECK(!nnh::debug::overlay_visible());
    nnh::shutdown();
}

NNH_TEST(platform_dev_request_toggles_the_overlay) {
    if (!nnh::build::kDevToolsEnabled) return;
    fresh();
    fake::request_overlay_toggle();
    CHECK(nnh::frame());
    CHECK(nnh::debug::overlay_visible());
    nnh::shutdown();
}

NNH_TEST(visible_overlay_draws_fps_and_memory_before_the_frame_ends) {
    if (!nnh::build::kDevToolsEnabled) return;
    fresh();
    nnh::debug::set_overlay_visible(true);
    CHECK(nnh::frame());
    fake::advance_time(nnh::Duration::from_ms(16));
    CHECK(nnh::frame()); // ends frame 1: overlay drawn into it
    CHECK(fake::state().last_frame_text_count > 0);
    CHECK(fake::state().last_frame_rects > 0); // frame-time graph
    nnh::shutdown();
}

NNH_TEST(hidden_overlay_draws_nothing) {
    fresh();
    CHECK(nnh::frame());
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_text_count == 0);
    CHECK(fake::state().last_frame_rects == 0);
    nnh::shutdown();
}

NNH_TEST(overlay_stays_off_in_release) {
    if (nnh::build::kDevToolsEnabled) return;
    fresh();
    nnh::debug::set_overlay_visible(true);
    CHECK(!nnh::debug::overlay_visible());
    nnh::shutdown();
}
