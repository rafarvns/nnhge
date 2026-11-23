// prefabs: prefab libraries spawned into worlds (spec 021). Three scenes:
//   showcase    a room made in Tiled whose objects name prefabs (walkers, a flying_walker variant, coins, a gold coin by
//               a property; a door the game reads itself), a roster of enemies registered from data assets, touch
//               spawns of the selected prefab, a gold coin by component override, the library's stats;
//   self-check  50 000 operations, prefab spawns against hand-written ones: "prefabs: <n> sum ..." every 10 000, then
//               "prefabs: PASS <sum>" when the bits are the simulator's (the console must match);
//   bench       AC-7's workloads timed: "prefabs bench: ..." lines and a verdict.
// SELECT: self-check, Y: bench (both come back with SELECT). START quits.
//
// PREFABS_DEMO picks the first scene: showcase (default), check, bench. Screenshots (Debug/Development):
//   NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/prefabs.png build/pc-debug/examples/prefabs/prefabs.exe
// Console test (spec 013): nnh run prefabs --script examples/prefabs/tests/check.nnhs

#include "bench_scene.hpp"
#include "demos.hpp"
#include "self_check_scene.hpp"
#include "showcase_scene.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <nnh/app.hpp>
#include <nnh/gfx.hpp>

NNH_LOG_CHANNEL(example);

bool demo_is(const char* name) {
    const char* demo = std::getenv("PREFABS_DEMO");
    return demo != nullptr && std::strcmp(demo, name) == 0;
}

namespace {

/// The scenes, on the heap (the 3DS main-thread stack is small).
struct Game {
    Demos demos;
    ShowcaseScene showcase{demos};
    SelfCheckScene check{demos};
    BenchScene bench{demos};

    Game() { demos = {&showcase, &check, &bench}; }
    nnh::Scene& first() {
        if (demo_is("check")) return check;
        if (demo_is("bench")) return bench;
        return showcase;
    }
};

} // namespace

int main() {
    if (!nnh::init({.title = "NNHGE Prefabs", .gfx = {.max_draws = 1024, .max_tiles = 1024}})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(12, 14, 24));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(12, 14, 24));
    {
        std::unique_ptr<Game> game(new (std::nothrow) Game());
        if (game != nullptr) nnh::run(game->first()); // until START
    } // the scenes (and their worlds and libraries) go before the engine
    nnh::shutdown();
    return 0;
}
