// animation: sprite animation (spec 016). Three demos, each a scene (SELECT: the next one):
//   showcase    top: one library built in code (a numbered strip) played as loop, once, repeat 3, ping-pong,
//               ping-pong reverse x2 and reverse, L/R change the speed, Y restarts; bottom: a knight loaded from an
//               Aseprite export (art/hero.ase.json, converted by the build) with a state machine added in code: D-Pad
//               run, A jump, B attack (can't be cut short), X hurt (can); `@step` plays a footstep, `@hit` shows the
//               hit box;
//   self-check  64 animators follow a fixed script for 600 frames: checksum lines every 60 frames, then
//               "animation: PASS <sum>" when the bits are the simulator's (the console must match);
//   bench       512 animators: "animation bench: 512 animators avg X.XXX ms worst Y.YYY ms".
// START quits.
//
// ANIMATION_DEMO picks the first demo: showcase (default), check, bench. HERO_DEMO_RUN=1 makes the knight run on its
// own. Screenshots (Debug/Development):
//   HERO_DEMO_RUN=1 NNH_EXIT_AFTER_FRAMES=120 NNH_SCREENSHOT=build/animation.png
//       build/pc-debug/examples/animation/animation.exe
// Console test (spec 013): nnh run animation --script examples/animation/tests/check.nnhs

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

namespace {

bool env_is(const char* value) {
    const char* demo = std::getenv("ANIMATION_DEMO");
    return demo != nullptr && std::strcmp(demo, value) == 0;
}

/// The scenes, on the heap (the 3DS main-thread stack is small).
struct Game {
    Demos demos;
    ShowcaseScene showcase{demos};
    SelfCheckScene check{demos};
    BenchScene bench{demos};

    Game() { demos = {&showcase, &check, &bench}; }
    nnh::Scene& first() {
        if (env_is("check")) return check;
        if (env_is("bench")) return bench;
        return showcase;
    }
};

} // namespace

int main() {
    if (!nnh::init({.title = "NNHGE Animation", .gfx = {.max_draws = 4096}})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(18, 20, 30));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(30, 34, 48));
    {
        std::unique_ptr<Game> game(new (std::nothrow) Game());
        if (game != nullptr) nnh::run(game->first()); // until START; every scene leaves the stack at the end
    } // scenes, libraries and textures go before the engine
    nnh::shutdown();
    return 0;
}
