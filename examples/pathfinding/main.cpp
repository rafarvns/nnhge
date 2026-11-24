// pathfinding: path maps, A* searches, smoothing, distance fields and a path follower (spec 025). Three scenes:
//   showcase    a top-down keep made in Tiled (walls, doors, a lake with deep water around an island, swamp and roads
//               whose weights come from the ground tiles' `path_cost` property). The hero walks a smoothed A* path
//               toward the touched cell; 32 chasers share one distance field toward the hero, rebuilt in slices.
//               Touch: goal; A: next door; X: hero 1x1/2x2; L: diagonal mode; R: smoothing; Up: debug view (off,
//               search, field); Down: pause the hero; SELECT: self-check; Y: bench; START: quit.
//   self-check  512 searches, 64 smoothings and 16 fields on seeded maps: "pathfinding: <n> sum ..." every 64 searches,
//               then "pathfinding: PASS <sum>" when the bits are the simulator's (the console must match);
//   bench       AC-7's workloads timed: "pathfinding bench: ..." lines and a verdict.
//
// PATHFINDING_DEMO picks the first scene: showcase (default), check, bench. PATHFINDING_SHOT sets a fixed goal and
// settings at frame 5 (explored, smoothed, field, partial, door-closed, door-open, big). Screenshots:
//   PATHFINDING_SHOT=explored NNH_EXIT_AFTER_FRAMES=40 NNH_SCREENSHOT=build/path.png
//       build/pc-debug/examples/pathfinding/pathfinding.exe
// Console test (spec 013): nnh run pathfinding --script examples/pathfinding/tests/check.nnhs

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <nnh/app.hpp>
#include <nnh/gfx.hpp>

#include "bench_scene.hpp"
#include "check_scene.hpp"
#include "demos.hpp"
#include "level.hpp"
#include "showcase_scene.hpp"

NNH_LOG_CHANNEL(example);

bool demo_is(const char* name) {
    const char* demo = std::getenv("PATHFINDING_DEMO");
    return demo != nullptr && std::strcmp(demo, name) == 0;
}

const char* shot_name() {
    const char* shot = std::getenv("PATHFINDING_SHOT");
    return shot != nullptr ? shot : "";
}

namespace {

/// The level and the scenes, on the heap (the 3DS main-thread stack is small).
struct Game {
    Level level;
    Demos demos;
    ShowcaseScene showcase{demos, level};
    CheckScene check{demos};
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
    // Two tile layers on the top screen; heat maps and the minimap need a few thousand shapes.
    if (!nnh::init({.title = "NNHGE Pathfinding", .gfx = {.max_draws = 4096, .max_tiles = 1024}})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(12, 14, 24));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(12, 14, 24));
    {
        std::unique_ptr<Game> game(new (std::nothrow) Game());
        if (game != nullptr && game->level.load()) nnh::run(game->first()); // until START
    } // the scenes, the maps and the level go before the engine
    nnh::shutdown();
    return 0;
}
