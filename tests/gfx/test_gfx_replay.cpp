// The held frame (spec 030 T3.4, FR-16, D52) on the fake platform: a paused tick submits the last game frame's 2D items
// and its debug text again; a released texture is skipped; a scene change at the start of the frame that paused drops
// the replay; Release keeps today's submit.

#include <cstring>

#include "gfx/gfx_replay.hpp"
#include "gfx_fixture.hpp"
#include "app/dev_pause.hpp"
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

namespace {
struct Textured : nnh::Scene {
    nnh::Texture hero;
    void on_enter() override { hero = gfx_test::load("hero"); }
    void on_exit() override { hero = nnh::Texture{}; } // released: the held lists still name it
    void draw() override { nnh::gfx::draw(nnh::Screen::Top, hero, {10, 10}); }
};
struct Other : nnh::Scene {};
} // namespace

// Review: a scene replace applied at the start of the frame that pauses, its texture released in on_exit.
NNH_TEST(gfx_replay_skips_a_frame_whose_scene_was_replaced_with_its_texture) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    gfx_test::restart();
    static Textured textured;
    static Other other;
    nnh::scenes::push(textured);
    CHECK(nnh::frame());
    nnh::scenes::update_and_draw(); // records the hero sprite
    nnh::scenes::replace(other);    // applied at the start of the next frame, which pauses
    fake::queue_dev_requests({kDevTogglePause, 0, kDevTogglePause});
    CHECK(nnh::frame()); // no crash, no stale-texture assert
    int sprites = 0;
    for (int i = 0; i < fake::state().last_frame_item_count; ++i) {
        const nnh::platform::DrawItem2D& it = fake::state().last_frame_items[i];
        sprites += it.w == 64 && it.h == 32 ? 1 : 0; // the hero (64 x 32), never a glyph
    }
    CHECK(sprites == 0); // the released texture's sprite was not submitted again
    CHECK(nnh::detail::g_dev_scenes_changed);
    nnh::scenes::pop();
    CHECK(nnh::frame());
}
