// Debug overlay: toggled by SELECT+L or a platform dev request, drawn at the end of each frame.

#include <cstring>

#include "debug/debug_list.hpp"
#include "debug/dev_tools.hpp"
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

using nnh::detail::DebugText;

/// Texts / rectangles the last finished frame drew in its debug pass.
int last_frame(DebugText::Kind kind) {
    const nnh::detail::DebugList& list = nnh::detail::debug_list_last_frame();
    int n = 0;
    for (int i = 0; i < list.count(); ++i) n += list.at(i).kind == kind ? 1 : 0;
    return n;
}

bool drew_text_containing(const char* needle) {
    const nnh::detail::DebugList& list = nnh::detail::debug_list_last_frame();
    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i).kind == DebugText::Kind::Text && std::strstr(list.text(i), needle) != nullptr) return true;
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
    CHECK(last_frame(DebugText::Kind::Text) > 0);
    CHECK(last_frame(DebugText::Kind::Rect) > 0); // frame-time graph
    nnh::shutdown();
}

NNH_TEST(hidden_overlay_draws_nothing) {
    fresh();
    CHECK(nnh::frame());
    CHECK(nnh::frame());
    CHECK(last_frame(DebugText::Kind::Text) == 0);
    CHECK(last_frame(DebugText::Kind::Rect) == 0);
    nnh::shutdown();
}

NNH_TEST(overlay_stays_off_in_release) {
    if (nnh::build::kDevToolsEnabled) return;
    fresh();
    nnh::debug::set_overlay_visible(true);
    CHECK(!nnh::debug::overlay_visible());
    nnh::shutdown();
}
