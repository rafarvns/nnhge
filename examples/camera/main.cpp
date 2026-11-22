// camera: camera helpers (spec 018). Three scenes (SELECT: the next one):
//   playground  a CameraRig follows the square you move through four rooms (yellow: dead zone, red: limits);
//               A shakes, X zooms (1, 1.5, 2, 3, 0.5), Y toggles smoothing, R switches single view (HUD
//               below), tall world (the bottom screen continues the top one) and two views (the bottom screen
//               follows a second, automatic target), L toggles a 24-pixel gap between the screens in tall
//               world; a touch on the bottom screen puts a marker on the world point under the finger;
//   self-check  a fixed 600-update script on a real rig: "camera: <frame> sum ..." every 60 frames, then
//               "camera: PASS <sum>" when the bits are the simulator's (the console must match);
//   bench       two rigs (tall world + bottom screen) updated and applied 16 times per frame:
//               "camera: bench avg X ms worst Y ms per pair (target < 0.05 ms) PASS|FAIL".
// START quits.
//
// CAMERA_DEMO picks the first scene and the playground's state: zoom1 (default), zoom2, zoom05, tall, two,
// check, bench. Screenshots (Debug/Development):
//   CAMERA_DEMO=tall NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/camera.png
//       build/pc-debug/examples/camera/camera.exe
// Console test (spec 013): nnh run camera --script examples/camera/tests/check.nnhs

#include "bench_scene.hpp"
#include "camera_scene.hpp"
#include "demos.hpp"
#include "self_check_scene.hpp"
#include "world.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <nnh/app.hpp>
#include <nnh/gfx.hpp>

NNH_LOG_CHANNEL(example);

bool demo_is(const char* name) {
    const char* demo = std::getenv("CAMERA_DEMO");
    return demo != nullptr && std::strcmp(demo, name) == 0;
}

namespace {

/// The scenes, on the heap (the 3DS main-thread stack is small).
struct Game {
    world::World world;
    Demos demos;
    CameraScene play{demos, world};
    SelfCheckScene check{demos};
    BenchScene bench{demos};

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
    // Zoom 0.5 shows four times the tiles of zoom 1: two full 16 × 16 layers on both screens fit in 8192.
    if (!nnh::init({.title = "NNHGE Camera", .gfx = {.max_draws = 4096, .max_tiles = 8192}})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(12, 14, 24));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(20, 22, 32));
    {
        std::unique_ptr<Game> game(new (std::nothrow) Game());
        if (game != nullptr && game->world.build()) nnh::run(game->first()); // until START
    } // scenes, the map and its texture go before the engine
    nnh::shutdown();
    return 0;
}
