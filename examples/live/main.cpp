// live: live values (spec 030). The showcase: a jumping square, bouncing balls, a sky and a ground, every number of
// them a tunable (player/jump, player/gravity, player/run, debug/hitboxes, spawn/count, spawn/shape, colors/sky,
// colors/ground, fx/dust, the player/respawn action) and its state watched (player/pos, player/vel, player/state,
// frame/count; spawn/alive by show()). SELECT + L shows the overlay, SELECT + R its pages; on the Tunables page the
// D-Pad edits, SELECT + Y pauses, SELECT + X steps. Y logs the self-check's sum (`live: sum XXXXXXXX`), X runs the
// bench, START quits.
//
// LIVE_DEMO=check logs the self-check at once; LIVE_SHOT sets the overlay for screenshots:
//   LIVE_SHOT=tunables NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/live.png build/pc-debug/examples/live/live.exe
// Console test (spec 013): nnh run live --script examples/live/tests/check.nnhs

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

#include <nnh/app.hpp>
#include <nnh/gfx.hpp>
#include <nnh/log.hpp>

#include "live_scene.hpp"
#include "self_check.hpp"

NNH_LOG_CHANNEL(example);

int main() {
    if (!nnh::init({.title = "NNHGE Live values"})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(12, 14, 24));
    const char* demo = std::getenv("LIVE_DEMO");
    if (demo != nullptr && std::strcmp(demo, "check") == 0) {
        const std::uint32_t sum = live_self_check(Tuning{});
        NNH_LOG_INFO(example, "live: sum %08X", static_cast<unsigned>(sum));
        if (sum == kLiveDefaultsGolden) NNH_LOG_INFO(example, "live: PASS defaults");
        NNH_LOG_INFO(example, "live: script values sum %08X", static_cast<unsigned>(live_self_check(live_script_tuning())));
    }
    {
        std::unique_ptr<LiveScene> scene(new (std::nothrow) LiveScene());
        if (scene != nullptr) nnh::run(*scene); // until START
    }
    nnh::shutdown();
    return 0;
}
