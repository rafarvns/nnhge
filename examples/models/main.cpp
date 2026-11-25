// models: 3D foundation (spec 026). Scenes:
//   showcase   a spinning textured ship converted from glTF, a checker floor, a code-built cube and sphere, a vertex-
//              coloured pylon, a cut-out fence, a blended glass pane, a directional and a circling point light; a 2D sky
//              under the world, the HUD and a name tag over it; touch buttons on the bottom screen. Circle Pad: orbit;
//              L/R: zoom; X: perspective/orthographic; SELECT: check; Y: bench; START: quit. With the 3D slider up
//              (simulator: F6/F7, F8 shows the right eye or an anaglyph) the top screen is drawn per eye; the
//              showcase logs "slider: <v>" and "eyes: <n>" when they change (spec 026b).
//   check      the 50 000-operation math script ("3d: <n> sum <x>" every 10 000, "3d: PASS <x>"), then fixed probe
//              scenes whose pixels are read back and compared with FR-10's formula ("3d pixels: PASS"), then the
//              stereo script ("3d-stereo: PASS <x>") and, with the slider up, a stereo probe scene in both eyes.
//   bench      AC-8's workloads timed: "3d bench: <case> avg <ms> worst <ms> missed <n>/600 cpu <ms>" and a verdict;
//              two stereo cases follow once the slider is up (tests/bench.nnhs sets it).
//   smoke      (DOWN in the showcase) a flat cube, then (A) a lit textured one; the backend logs "3d tilt: ..." at init.
// MODELS_DEMO picks the first scene: showcase (default), check, bench, smoke. Screenshots:
//   NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/models.png build/pc-debug/examples/models/models.exe
// Console test (spec 013): nnh run models --script examples/models/tests/check.nnhs

#include <memory>
#include <new>
#include <nnh/app.hpp>
#include <nnh/gfx.hpp>

#include "bench_scene.hpp"
#include "check_scene.hpp"
#include "showcase_scene.hpp"
#include "smoke_scene.hpp"
#include "world.hpp"

NNH_LOG_CHANNEL(example);

namespace {

/// The world and the scenes, on the heap (the 3DS main-thread stack is small).
struct Game {
    World world;
    Demos demos;
    ShowcaseScene showcase{world, demos};
    CheckScene check{world, demos};
    BenchScene bench{world, demos};
    SmokeScene smoke{world};

    Game() { demos = {&showcase, &check, &bench, &smoke}; }
    nnh::Scene& first() {
        if (demo_is("check")) return check;
        if (demo_is("bench")) return bench;
        if (demo_is("smoke")) return smoke;
        return showcase;
    }
};

} // namespace

int main() {
    // 3D on: up to 600 draws a frame (the bench draws 512).
    if (!nnh::init({.title = "NNHGE Models", .gfx = {.max_draws = 2048, .max_draws_3d = 600}})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(20, 24, 40));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(16, 18, 28));
    {
        std::unique_ptr<Game> game(new (std::nothrow) Game());
        if (game != nullptr && game->world.load()) nnh::run(game->first()); // until START
    } // the scenes and the world go before the engine
    nnh::shutdown();
    return 0;
}
