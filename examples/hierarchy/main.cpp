// hierarchy: parents, children and world transforms (spec 022). Three scenes:
//   showcase    a yard: a ship (Circle Pad moves, L/R turn) with a gun (D-Pad left/right aims, A fires from its muzzle)
//               and a hand; a walker mirrored with scale.x = -1 when it turns, its hat bobbing (a tween on the hat's
//               local position); a cart whose rider the top camera follows; a squashed block with a turned child
//               (shear); crates. Bottom screen: the whole yard at zoom 0.5; a touch picks a node, X attaches it to the
//               ship's hand or drops it (keep world: it doesn't jump), B destroys the ship (its children go with it),
//               D-Pad up detaches its children first, D-Pad down toggles the debug draw, X with nothing picked restarts;
//   self-check  50 000 operations, prefab hierarchies against hand-written ones: "hierarchy: <n> sum ..." every 10 000,
//               then "hierarchy: PASS <sum>" when the bits are the simulator's (the console must match);
//   bench       AC-7's workloads timed: "hierarchy bench: ..." lines and a verdict.
// SELECT: self-check, Y: bench (both come back with SELECT). START quits.
//
// HIERARCHY_DEMO picks the first scene: showcase (default), check, bench. Screenshots (Debug/Development):
//   NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/hierarchy.png build/pc-debug/examples/hierarchy/hierarchy.exe
// Console test (spec 013): nnh run hierarchy --script examples/hierarchy/tests/check.nnhs

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
    const char* demo = std::getenv("HIERARCHY_DEMO");
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
    if (!nnh::init({.title = "NNHGE Hierarchy", .gfx = {.max_draws = 1024}})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(150, 196, 230));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(14, 16, 26));
    {
        std::unique_ptr<Game> game(new (std::nothrow) Game());
        if (game != nullptr) nnh::run(game->first()); // until START
    } // the scenes (and their worlds and libraries) go before the engine
    nnh::shutdown();
    return 0;
}
