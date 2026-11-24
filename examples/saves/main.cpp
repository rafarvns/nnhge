// saves: structured saves with versions (spec 027). Three scenes:
//   showcase  a tiny game whose state is a versioned save type (progress.hpp): three slots with their
//             "Load game" summaries, a settings file of its own, the last load report; golden v1 loaded
//             with its migration; a slot damaged on purpose to load its backup;
//   check     the golden v1/v2/v3 files copied to the data folder and loaded, the reference encoded, a slot
//             stored, damaged and loaded from its backup: "saves: ..." lines, then "saves: PASS" when they
//             are the simulator's;
//   bench     AC-8's workloads timed: "saves bench: ..." lines and a verdict.
// SELECT: check, Y: bench (both come back with SELECT). START quits.
//
// SAVES_DEMO picks the first scene: showcase (default), check, bench. SAVES_SHOT=empty|saved|golden|backup
// plays fixed actions for screenshots; NNH_SAVE_DUMP=1 writes slot1.sav.txt beside slot1.sav (Debug and
// Development, simulator):
//   SAVES_SHOT=saved NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/saves.png
//       build/pc-debug/examples/saves/saves.exe   (one command)
// Console test (spec 013): nnh run saves --script examples/saves/tests/check.nnhs

#include "bench_scene.hpp"
#include "check_scene.hpp"
#include "demos.hpp"
#include "showcase_scene.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <nnh/app.hpp>
#include <nnh/gfx.hpp>

NNH_LOG_CHANNEL(example);

bool demo_is(const char* name) {
    const char* demo = std::getenv("SAVES_DEMO");
    return demo != nullptr && std::strcmp(demo, name) == 0;
}

bool shot_is(const char* name) {
    static const char* shot = std::getenv("SAVES_SHOT");
    return shot != nullptr && std::strcmp(shot, name) == 0;
}

namespace {

/// The scenes, on the heap (each holds a Progress and a buffer: about 10 KB; the 3DS main-thread stack is
/// small).
struct Game {
    Demos demos;
    ShowcaseScene showcase{demos};
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
    if (!nnh::init({.title = "NNHGE Saves", .data_folder = "saves"})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(12, 14, 24));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(12, 14, 24));
    {
        std::unique_ptr<Game> game(new (std::nothrow) Game());
        if (game != nullptr) nnh::run(game->first()); // until START
    }
    nnh::shutdown();
    return 0;
}
