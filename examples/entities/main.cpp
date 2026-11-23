// entities: entities, components, tags and object pools (spec 020). Three scenes:
//   showcase    an arena of walkers, flyers (bobbing by tweens through handle-capturing setters) and coins, a ship
//               firing bullets from a pool of 32 and a homing missile that keeps its target's handle. Controls in
//               showcase_scene.hpp: tag filters with find() and each_tagged(), touch-to-destroy through a box view
//               of the body storage, remove hooks that spawn explosions, the world's stats on the bottom screen;
//   self-check  100 000 operations on a world and a pool: "entities: <n> sum ..." every 10 000, then
//               "entities: PASS <sum>" when the bits are the simulator's (the console must match);
//   bench       AC-7's five workloads timed: "entities bench: ..." lines and a verdict.
// SELECT: self-check, Y: bench (both come back with SELECT). START quits.
//
// ENTITIES_DEMO picks the first scene: showcase (default), check, bench. Screenshots (Debug/Development):
//   NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/entities.png build/pc-debug/examples/entities/entities.exe
// Console test (spec 013): nnh run entities --script examples/entities/tests/check.nnhs

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
    const char* demo = std::getenv("ENTITIES_DEMO");
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
    if (!nnh::init({.title = "NNHGE Entities"})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(12, 14, 24));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(12, 14, 24));
    {
        std::unique_ptr<Game> game(new (std::nothrow) Game());
        if (game != nullptr) nnh::run(game->first()); // until START
    } // the scenes (and their worlds and pools) go before the engine
    nnh::shutdown();
    return 0;
}
