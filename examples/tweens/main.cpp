// tweens: tweens and timers (spec 015). Three demos, each a scene (SELECT: the next one):
//   showcase    top: the 19 easing curves, each with a marker moving along it; bottom: an intro built as one
//               sequence (a panel slides in with BackOut while its colour fades, the title springs in, then
//               "READY" after a timer) and a blink driven by nnh::Timer. A pause/resume, B complete, Y
//               restart, X pushes an overlay scene with its own tweens (the showcase's freeze under it),
//               START pops it;
//   self-check  a fixed script played for 600 frames: checksum lines every 60 frames, then
//               "tweens: PASS <sum>" when the bits are the simulator's (the console must match);
//   bench       256 tweens of every kind and curve: "tweens bench: ... avg X.XX ms worst Y.YY ms".
// START quits. All tweens live in sets bound to their scene: nothing to update or clear by hand.
//
// TWEENS_DEMO picks the first demo: showcase (default), overlay (the showcase with the overlay pushed),
// check, bench. Screenshots (Debug/Development):
//   NNH_EXIT_AFTER_FRAMES=90 NNH_SCREENSHOT=build/tweens.png build/pc-debug/examples/tweens/tweens.exe
// Console test (spec 013): nnh run tweens --script examples/tweens/tests/check.nnhs

#include "bench_scene.hpp"
#include "demos.hpp"
#include "overlay_scene.hpp"
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
    const char* demo = std::getenv("TWEENS_DEMO");
    return demo != nullptr && std::strcmp(demo, value) == 0;
}

/// The scenes, on the heap (the 3DS main-thread stack is small).
struct Game {
    Demos demos;
    ShowcaseScene showcase{demos, env_is("overlay")};
    OverlayScene overlay;
    SelfCheckScene check{demos};
    BenchScene bench{demos};

    Game() { demos = {&showcase, &overlay, &check, &bench}; }
    nnh::Scene& first() {
        if (env_is("check")) return check;
        if (env_is("bench")) return bench;
        return showcase;
    }
};

} // namespace

int main() {
    if (!nnh::init({.title = "NNHGE Tweens", .gfx = {.max_draws = 4096}})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(16, 20, 34));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(24, 26, 36));
    {
        std::unique_ptr<Game> game(new (std::nothrow) Game());
        if (game != nullptr) nnh::run(game->first()); // until START; every scene leaves the stack at the end
    } // scenes and their sets go before the engine
    nnh::shutdown();
    return 0;
}
