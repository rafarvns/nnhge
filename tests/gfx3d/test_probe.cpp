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
