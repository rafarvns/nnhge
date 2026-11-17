// Platform contract conformance (docs/architecture/platforms.md).
// Uses only nnh/platform/*.hpp, so the same file is linked against every platform library:
// nnh_conformance_fake and nnh_conformance_pc (headless) in CI; on 3DS through an example later.

#include "nnh/platform/app.hpp"
#include "nnh/platform/input.hpp"
#include "nnh/platform/time.hpp"
#include "nnh/platform/video.hpp"
#include "test.hpp"

namespace platform = nnh::platform;

namespace {

nnh::AppConfig config() {
    nnh::AppConfig c;
    c.title = "conformance";
    c.pc_window_scale = 1;
    return c;
}

} // namespace

NNH_TEST(init_succeeds_and_shutdown_allows_init_again) {
    CHECK(platform::init(config()));
    platform::shutdown();
    CHECK(platform::init(config()));
    platform::shutdown();
}

NNH_TEST(poll_events_keeps_running_after_init) {
    CHECK(platform::init(config()));
    CHECK(platform::poll_events());
    platform::shutdown();
}

NNH_TEST(frames_can_begin_draw_and_end_repeatedly) {
    CHECK(platform::init(config()));
    for (int i = 0; i < 3; ++i) {
        CHECK(platform::poll_events());
        platform::begin_frame(nnh::colors::black, nnh::colors::gray);
        platform::debug_text(nnh::Screen::Top, 0, 0, "conformance\nsecond line", nnh::colors::white);
        platform::debug_text(nnh::Screen::Bottom, 500, 500, "clipped", nnh::colors::white);
        platform::end_frame();
    }
    platform::shutdown();
}

NNH_TEST(no_buttons_are_held_without_input) {
    CHECK(platform::init(config()));
    CHECK(platform::poll_events());
    CHECK(platform::read_buttons() == 0);
    platform::shutdown();
}

NNH_TEST(clock_never_goes_backwards) {
    CHECK(platform::init(config()));
    std::int64_t previous = platform::ticks_ns();
    for (int i = 0; i < 1000; ++i) {
        const std::int64_t now = platform::ticks_ns();
        CHECK(now >= previous);
        previous = now;
    }
    platform::shutdown();
}

NNH_TEST(debug_line_height_is_positive) {
    CHECK(platform::init(config()));
    CHECK(platform::debug_line_height() > 0);
    platform::shutdown();
}
