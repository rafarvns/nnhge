// The held frame (spec 030 T3.4, FR-16, D52) on the fake platform: a paused tick submits the last game frame's 2D items
// and its debug text again; a released texture is skipped; a scene change at the start of the frame that paused drops
// the replay; Release keeps today's submit.

#include <cstring>

#include "gfx/gfx_replay.hpp"
#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/debug.hpp"
#include "nnh/gfx.hpp"
#include "nnh/platform/system.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/scene.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
using nnh::platform::kDevTogglePause;

namespace {

void restart() {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init({}));
}

void draw_scene() {
    nnh::gfx::fill_rect(nnh::Screen::Top, {10, 20, 30, 40}, nnh::rgba(200, 10, 10, 255));
    nnh::gfx::fill_rect(nnh::Screen::Bottom, {1, 2, 3, 4}, nnh::rgba(10, 200, 10, 255), {.layer = 3});
    nnh::debug::print(nnh::Screen::Top, 4, 100, "held text");
}

struct Items {
    int count = 0;
    nnh::platform::DrawItem2D items[nnh::platform_fake::kMaxRecordedItems];
};

void copy_last(Items& out) {
    out.count = fake::state().last_frame_item_count;
    std::memcpy(out.items, fake::state().last_frame_items, sizeof(nnh::platform::DrawItem2D) * static_cast<std::size_t>(out.count));
}

bool same(const Items& a, const Items& b) {
    return a.count == b.count && std::memcmp(a.items, b.items, sizeof(nnh::platform::DrawItem2D) * static_cast<std::size_t>(a.count)) == 0;
}

Items g_before, g_paused;

} // namespace

NNH_TEST(gfx_replay_a_paused_tick_draws_the_held_frame) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    CHECK(nnh::frame());
    draw_scene();
    CHECK(nnh::frame());
    draw_scene();
    copy_last(g_before); // frame 1 as drawn (2D items and debug text glyphs)
    CHECK(g_before.count > 2);
    fake::queue_dev_requests({kDevTogglePause, 0, kDevTogglePause});
    CHECK(nnh::frame()); // ends frame 2 (drawn like frame 1), then two paused ticks
    copy_last(g_paused);
    CHECK(same(g_before, g_paused));
}

NNH_TEST(gfx_replay_lists_are_cleared_when_the_next_game_frame_records) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    CHECK(nnh::frame());
    draw_scene();
    CHECK(nnh::frame()); // no draw in frame 2
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 0);
}

namespace {
struct Empty : nnh::Scene {};
} // namespace

NNH_TEST(gfx_replay_is_dropped_after_a_scene_change) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    CHECK(nnh::frame());
    draw_scene();
    static Empty scene;
    nnh::scenes::push(scene); // applied at the start of the frame that pauses
    fake::queue_dev_requests({kDevTogglePause, 0, kDevTogglePause});
    CHECK(nnh::frame());
    // the paused ticks drew no 2D item of the held frame, only the note in debug text
    bool red = false;
    for (int i = 0; i < fake::state().last_frame_item_count; ++i) {
        red = red || (fake::state().last_frame_items[i].color.r == 200 && fake::state().last_frame_items[i].color.g == 10);
    }
    CHECK(!red);
    nnh::scenes::pop();
    CHECK(nnh::frame());
}

NNH_TEST(gfx_replay_release_submits_and_clears_as_before) {
    if constexpr (nnh::build::kDevToolsEnabled) return;
    restart();
    CHECK(nnh::frame());
    draw_scene();
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count > 0);
    CHECK(!nnh::detail::g_gfx_held);
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 0);
}
