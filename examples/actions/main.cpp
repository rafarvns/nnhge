// actions: input actions (spec 017). One ActionSet for the whole game (controls.cpp), loaded from the SD card at
// start; a playfield driven by actions (play_scene), a controls screen to rebind them (controls_scene, START) and a
// bench (bench_scene). Every action edge and remap result is logged in a fixed format for the console script.
//
// ACTIONS_DEMO picks the first screen: play (default), controls (the controls screen over the playfield), bench.
// Screenshots (Debug/Development):
//   NNH_SIM_INPUT="circle=0.7,0;buttons=B" NNH_EXIT_AFTER_FRAMES=40 NNH_SCREENSHOT=build/actions.png \
//     build/pc-debug/examples/actions/actions.exe
//   ACTIONS_DEMO=controls NNH_EXIT_AFTER_FRAMES=20 NNH_SCREENSHOT=build/controls.png build/pc-debug/examples/actions/actions.exe
// Test (spec 013): nnh run actions --script examples/actions/tests/check.nnhs   (--pc for the simulator)

#include "bench_scene.hpp"
#include "action_log.hpp"
#include "controls_scene.hpp"
#include "play_scene.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <nnh/app.hpp>
#include <nnh/gfx.hpp>
#include <nnh/system.hpp>

NNH_LOG_CHANNEL(example);

void log_line(const char* line) { NNH_LOG_INFO(example, "%s", line); }

namespace {

bool env_is(const char* value) {
    const char* demo = std::getenv("ACTIONS_DEMO");
    return demo != nullptr && std::strcmp(demo, value) == 0;
}

/// The set and the scenes, on the heap (the 3DS main-thread stack is small).
struct Game {
    Shared shared;
    PlayScene play{shared};
    ControlsScene controls{shared};
    BenchScene bench{shared};

    Game() {
        shared.controls_scene = &controls;
        shared.bench_scene = &bench;
    }
};

void load_bindings(nnh::ActionSet& set) {
    const nnh::Result<nnh::LoadReport> r = set.load(kBindingsPath, kBindingsVersion);
    char line[96];
    if (!r) {
        std::snprintf(line, sizeof line, "actions: bindings not loaded (%s): defaults", r.error().message);
    } else if (!r.value().found) {
        std::snprintf(line, sizeof line, "actions: no saved bindings: defaults");
    } else {
        std::snprintf(line, sizeof line, "actions: loaded %d applied %d skipped%s", r.value().applied,
                      r.value().skipped, r.value().version_ignored ? " (old version ignored)" : "");
    }
    log_line(line);
}

} // namespace

int main() {
    if (!nnh::init({.title = "NNHGE Actions"})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(18, 22, 36));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(26, 28, 38));
    {
        std::unique_ptr<Game> game(new (std::nothrow) Game());
        if (game != nullptr && declare_controls(game->shared.controls)) {
            load_bindings(game->shared.controls);
            log_line(aim_line(nnh::system::has(nnh::Capability::CStick)));
            if (env_is("controls") || env_is("bench")) {
                nnh::scenes::push(game->play);
                nnh::run(env_is("bench") ? static_cast<nnh::Scene&>(game->bench) : game->controls);
            } else {
                nnh::run(game->play); // until the system quits; every scene leaves the stack at the end
            }
        }
    } // scenes, then the set, before the engine
    nnh::shutdown();
    return 0;
}
