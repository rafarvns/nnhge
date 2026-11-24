// particles: emitters, their deterministic simulation and particle batches (spec 024). Three scenes:
//   showcase    the top screen's gallery: a torch (local space, flame frames over life), chimney smoke, rain behind or
//               in front of a tile band, a ship leaving an exhaust trail (Circle Pad, or its own path), an explosion
//               (A), a fountain of texture-less squares, embers from a data preset (assets/fx/embers.particles). The
//               bottom screen: a touch bursts the selected effect (L/R) at the touched point; the HUD shows the
//               emitters' stats and gfx::stats()'s particle line. X world/local space, B rain layer, D-Pad Down smoke
//               stop/start, SELECT self-check, Y bench, START quit.
//   self-check  8 emitters, 3600 frames of a fixed script: "particles: <frame> sum ..." every 600 frames, then
//               "particles: PASS <sum>" when the bits are the simulator's (the console must match);
//   bench       AC-7's workloads timed: "particles bench: ..." lines and a verdict.
//
// PARTICLES_DEMO picks the first scene: showcase (default), check, bench. PARTICLES_SHOT sets fixed inputs at frame 10
// (gallery, trail, explosion, touch, local, world, full). Screenshots:
//   PARTICLES_SHOT=explosion NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/fx.png build/pc-debug/examples/particles/particles.exe
// Console test (spec 013): nnh run particles --script examples/particles/tests/check.nnhs

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <nnh/app.hpp>
#include <nnh/gfx.hpp>

#include "bench_scene.hpp"
#include "check_scene.hpp"
#include "demos.hpp"
#include "showcase_scene.hpp"

NNH_LOG_CHANNEL(example);

bool demo_is(const char* name) {
    const char* demo = std::getenv("PARTICLES_DEMO");
    return demo != nullptr && std::strcmp(demo, name) == 0;
}

const char* shot_name() {
    const char* shot = std::getenv("PARTICLES_SHOT");
    return shot != nullptr ? shot : "";
}

namespace {

/// The scenes, on the heap (the 3DS main-thread stack is small).
struct Game {
    Demos demos;
    ShowcaseScene showcase{demos};
    CheckScene check{demos};
    BenchScene bench{demos, showcase.sheet()};

    Game() { demos = {&showcase, &check, &bench}; }
    nnh::Scene& first() {
        if (demo_is("check")) return check;
        if (demo_is("bench")) return bench;
        return showcase;
    }
};

} // namespace

int main() {
    // The gallery's tile band, its sprites and HUD text, and up to 4096 particle quads a frame (both screens).
    if (!nnh::init({.title = "NNHGE Particles", .gfx = {.max_draws = 4096, .max_tiles = 512, .max_particles = 4096}})) {
        return 1;
    }
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(16, 20, 40));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(12, 14, 24));
    {
        std::unique_ptr<Game> game(new (std::nothrow) Game());
        if (game != nullptr && game->showcase.create()) nnh::run(game->first()); // until START
    } // the scenes and their emitters go before the engine
    nnh::shutdown();
    return 0;
}
