// queries: collision layers and queries (spec 019). Three scenes (SELECT: the next one):
//   playground  a ray from the emitter (top-left) to the pink target (Circle Pad / D-Pad) against a room of
//               walls, a water pool on its own layer, one-way planks and 12 boxes on three layers. X: first hit /
//               all hits with the cells crossed / box cast; A, B, L, R toggle the mask's water, red, green and
//               blue bits; Y ignores one-way cells. The bottom screen shows the room through a Camera2D at
//               (40, 0): a touch becomes the world point (40 + x, y), and the HUD tells what is under it;
//   self-check  4096 rays and 512 box casts on the real queries: "queries: <n> sum ..." every 1024 rays, then
//               "queries: PASS <sum>" when the bits are the simulator's (the console must match);
//   bench       rays, point queries and moves timed: "queries bench: ..." lines (spec 019 AC-7).
// START quits.
//
// QUERIES_DEMO picks the first scene and the playground's state: first (default), all, cast, water, touch, check,
// bench. Screenshots (Debug/Development):
//   QUERIES_DEMO=all NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/queries.png
//       build/pc-debug/examples/queries/queries.exe
// Console test (spec 013): nnh run queries --script examples/queries/tests/check.nnhs

#include "bench_scene.hpp"
#include "demos.hpp"
#include "queries_scene.hpp"
#include "room.hpp"
#include "self_check_scene.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <nnh/app.hpp>
#include <nnh/gfx.hpp>

NNH_LOG_CHANNEL(example);

bool demo_is(const char* name) {
    const char* demo = std::getenv("QUERIES_DEMO");
    return demo != nullptr && std::strcmp(demo, name) == 0;
}

namespace {

/// The scenes and the room, on the heap (the 3DS main-thread stack is small).
struct Game {
    room::Room room;
    Demos demos;
    QueriesScene play{demos, room};
    SelfCheckScene check{demos};
    BenchScene bench{demos, room};

    Game() { demos = {&play, &check, &bench}; }
    nnh::Scene& first() {
        if (demo_is("check")) return check;
        if (demo_is("bench")) return bench;
        play.preset();
        return play;
    }
};

} // namespace

int main() {
    // Four tile layers on both screens and up to ~200 shapes per screen.
    if (!nnh::init({.title = "NNHGE Queries", .gfx = {.max_draws = 2048, .max_tiles = 4096}})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(12, 14, 24));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(20, 22, 32));
    {
        std::unique_ptr<Game> game(new (std::nothrow) Game());
        if (game != nullptr && game->room.build()) nnh::run(game->first()); // until START
    } // scenes, the map and its texture go before the engine
    nnh::shutdown();
    return 0;
}
