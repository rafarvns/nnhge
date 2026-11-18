// Platform contract conformance (docs/architecture/platforms.md).
// Uses only nnh/platform/*.hpp, so the same file is linked against every platform library:
// nnh_conformance_fake and nnh_conformance_pc (headless) in CI; on 3DS through an example later.

#include "nnh/platform/app.hpp"
#include "nnh/platform/input.hpp"
#include "nnh/platform/log.hpp"
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

NNH_TEST(texture_create_rejects_data_it_cannot_decode) {
    CHECK(platform::init(config()));
    const unsigned char garbage[] = {1, 2, 3, 4, 5, 6, 7, 8};
    platform::TextureInfo info;
    CHECK(!platform::texture_create(garbage, sizeof garbage, info));
    CHECK(info.handle == 0);
    platform::shutdown();
}

NNH_TEST(draw_2d_accepts_empty_and_shape_passes) {
    CHECK(platform::init(config()));
    CHECK(platform::poll_events());
    platform::begin_frame(nnh::colors::black, nnh::colors::black);
    platform::draw_2d(nnh::Screen::Top, nullptr, 0);
    platform::DrawItem2D shapes[3];
    shapes[0].kind = platform::DrawItem2D::Kind::FillRect;
    shapes[1].kind = platform::DrawItem2D::Kind::RectOutline;
    shapes[2].kind = platform::DrawItem2D::Kind::Line;
    for (auto& s : shapes) {
        s.x = 10;
        s.y = 10;
        s.w = 20;
        s.h = 20;
        s.color = nnh::colors::red;
    }
    platform::draw_2d(nnh::Screen::Bottom, shapes, 3);
    platform::end_frame();
    platform::shutdown();
}

NNH_TEST(idle_input_is_zero) {
    CHECK(platform::init(config()));
    CHECK(platform::poll_events());
    platform::InputSample sample;
    sample.buttons = ~0u; // read_input must overwrite everything
    sample.circle_x = sample.cstick_y = 5.0f;
    sample.touching = true;
    platform::read_input(sample);
    CHECK(sample.buttons == 0);
    CHECK(sample.circle_x == 0.0f && sample.circle_y == 0.0f && sample.cstick_x == 0.0f && sample.cstick_y == 0.0f);
    CHECK(!sample.touching);
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

NNH_TEST(console_accepts_lines_of_every_level) {
    for (int l = 0; l <= static_cast<int>(nnh::LogLevel::Fatal); ++l) {
        platform::log_console(static_cast<nnh::LogLevel>(l), "[f0        0.000s] I test   conformance line");
    }
}

NNH_TEST(last_error_is_never_null) {
    CHECK(platform::last_error() != nullptr);
}

NNH_TEST(debug_line_height_is_positive) {
    CHECK(platform::init(config()));
    CHECK(platform::debug_line_height() > 0);
    platform::shutdown();
}
