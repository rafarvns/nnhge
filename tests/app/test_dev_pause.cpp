// Pause and step inside nnh::frame() (spec 030 T3.5, T3.6, FR-16, D52) on the fake platform: paused ticks run no game
// frame (no scene update, no input event, plain loops wait), frame_count() and the frame clock stop, step N runs exactly
// N frames, time::delta() after a pause is the one measured before it, quit while paused, the held start.

#include "app/dev_pause.hpp"
#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/events.hpp"
#include "nnh/input.hpp"
#include "nnh/live.hpp"
#include "nnh/platform/system.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/scene.hpp"
#include "nnh/time.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
using nnh::platform::kDevHoldAtStart;
using nnh::platform::kDevStepFrame;
using nnh::platform::kDevTogglePause;

namespace {

void restart() {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init({}));
}

int g_updates = 0;
struct Counting : nnh::Scene {
    void update() override { ++g_updates; }
};

} // namespace

NNH_TEST(dev_pause_ticks_until_resumed_without_a_game_frame) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    CHECK(nnh::frame());
    const std::uint64_t frame = nnh::frame_count();
    const int begun = fake::state().frames_begun;
    fake::queue_dev_requests({kDevTogglePause, 0, 0, kDevTogglePause});
    CHECK(nnh::frame());
    CHECK(nnh::frame_count() == frame + 1);              // one game frame, after the pause
    CHECK(fake::state().frames_begun == begun + 3 + 1); // three paused ticks, then the game frame
}

NNH_TEST(dev_pause_step_runs_exactly_n_game_frames) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    static Counting scene;
    nnh::scenes::push(scene);
    CHECK(nnh::frame());
    g_updates = 0;
    // pause, a tick, step (one frame), then at the next start a tick and resume
    fake::queue_dev_requests({kDevTogglePause, 0, kDevStepFrame, 0, kDevTogglePause});
    const std::uint64_t frame = nnh::frame_count();
    CHECK(nnh::frame());
    nnh::scenes::update_and_draw();
    CHECK(nnh::frame_count() == frame + 1);
    CHECK(nnh::frame());
    nnh::scenes::update_and_draw();
    CHECK(nnh::frame_count() == frame + 2);
    CHECK(g_updates == 2);
    nnh::scenes::pop();
    CHECK(nnh::frame());
}

NNH_TEST(dev_pause_delta_after_a_pause_is_the_one_before_it) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    CHECK(nnh::frame());
    fake::advance_time(nnh::Duration{16'000'000});
    CHECK(nnh::frame());
    CHECK(nnh::time::delta().ns == 16'000'000);
    fake::advance_time(nnh::Duration{17'000'000});
    fake::queue_dev_requests({kDevTogglePause, 0, 0, kDevTogglePause});
    // The fake clock doesn't move during the ticks here; a long pause is simulated by advancing it before the frame
    // after: the frame that resumed keeps 17 ms, the next one measures from the resume.
    CHECK(nnh::frame());
    CHECK(nnh::time::delta().ns == 17'000'000);
    fake::advance_time(nnh::Duration{15'000'000});
    CHECK(nnh::frame());
    CHECK(nnh::time::delta().ns == 15'000'000);
}

NNH_TEST(dev_pause_quit_while_paused_quits) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    CHECK(nnh::frame());
    fake::queue_dev_requests({kDevTogglePause, 0, 0});
    fake::quit_after_polls(3);
    CHECK(!nnh::frame());
    fake::set_quit_requested(false);
    restart();
}

NNH_TEST(dev_pause_held_start_waits_then_runs_after_its_timeout) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    fake::queue_dev_requests({kDevHoldAtStart});
    const int begun = fake::state().frames_begun;
    CHECK(nnh::frame());
    CHECK(nnh::frame_count() == 1);
    CHECK(fake::state().frames_begun == begun + nnh::detail::PauseState::kHoldTicks + 1);
}

NNH_TEST(dev_pause_live_pause_pauses_at_the_next_frame) {
    if constexpr (!nnh::live::kEnabled) return;
    restart();
    CHECK(nnh::frame());
    nnh::live::pause();
    fake::queue_dev_requests({0, 0, kDevTogglePause});
    const int begun = fake::state().frames_begun;
    CHECK(nnh::frame());
    CHECK(fake::state().frames_begun == begun + 2 + 1);
}

NNH_TEST(dev_pause_no_input_event_from_paused_ticks) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    int presses = 0;
    auto sub = nnh::events::subscribe<nnh::ButtonPressed>([&](const nnh::ButtonPressed&) { ++presses; });
    CHECK(nnh::frame());
    fake::queue_dev_requests({kDevTogglePause, 0, 0, kDevTogglePause});
    fake::set_held_buttons(static_cast<std::uint32_t>(nnh::Button::A)); // pressed during the pause, still held
    CHECK(nnh::frame());
    CHECK(nnh::input::pressed(nnh::Button::A)); // pressed on the next game frame, once
    CHECK(presses == 1);
    fake::set_held_buttons(0);
}
