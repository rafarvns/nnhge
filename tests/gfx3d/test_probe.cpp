// Probe pixels (spec 026 FR-24, plan P19, T7.5): a request made during a frame is answered from the frame that ended
// (read back through the remote capture contract: the fake's synthetic picture), available from the next frame;
// invalid requests are refused; Release answers nothing.

#include "gfx3d_fixture.hpp"

using namespace gfx3d_test;
using nnh::Screen;

NNH_TEST(gfx3d_probes_are_read_back_after_the_frame) {
    restart();
    next_frame();
    const nnh::Probe at[2] = {{10, 20}, {300, 200}};
    const bool asked = nnh::gfx3d::request_probes(Screen::Top, nnh::Eye::Left, at, 2);
    CHECK(asked == nnh::build::kDevToolsEnabled);
    nnh::Color got[2];
    CHECK(nnh::gfx3d::probe_results(got, 2) == 0); // not before the frame ends
    next_frame();
    next_frame();
    CHECK(nnh::gfx3d::probe_results(got, 2) == 0); // read three frame ends after the request
    next_frame();
    if (nnh::build::kDevToolsEnabled) {
        CHECK(nnh::gfx3d::probe_results(got, 2) == 2);
        std::uint8_t want[4];
        fake::remote_capture_pixel(Screen::Top, 300, 200, want);
        CHECK(got[1].r == want[0] && got[1].g == want[1] && got[1].b == want[2]);
    }
    CHECK(!nnh::gfx3d::request_probes(Screen::Bottom, nnh::Eye::Left, at, 2));
    CHECK(!nnh::gfx3d::request_probes(Screen::Top, nnh::Eye::Left, at, 0));
    CHECK(!nnh::gfx3d::request_probes(Screen::Top, nnh::Eye::Left, at, 33));
    nnh::shutdown();
}

NNH_TEST(gfx3d_probes_of_the_right_eye_read_its_picture) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    nnh::shutdown();
    fake::reset();
    fake::set_capabilities({nnh::Capability::SecondScreen, nnh::Capability::Stereo3D});
    fake::set_stereo_slider(1.0f);
    nnh::AppConfig config;
    config.gfx.max_draws_3d = 8;
    CHECK(nnh::init(config));
    nnh::Mesh c = cube();
    next_frame();
    const nnh::Probe points[1] = {{300, 200}};
    CHECK(nnh::gfx3d::request_probes(Screen::Top, nnh::Eye::Right, points, 1));
    for (int i = 0; i < 4; ++i) {
        nnh::gfx3d::draw(Screen::Top, c, at({}));
        next_frame();
    }
    nnh::Color got[1];
    CHECK(nnh::gfx3d::probe_results(got, 1) == 1);
    std::uint8_t want[4];
    fake::remote_capture_right_pixel(300, 200, want);
    CHECK(got[0].r == want[0] && got[0].g == want[1] && got[0].b == want[2] && want[2] == 0xC0);
    c.reset();
    nnh::shutdown();
}
