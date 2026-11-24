// transitions: scene transitions (spec 023). A title, two rooms and a loading scene:
//   title         press A: a fade into the first room;
//   room          the Circle Pad moves the square; LEFT/RIGHT pick the effect (fade, wipe, split, blinds, iris,
//                 diamond, curtain: a custom effect made of the example's sprites), L the reveal (reverse,
//                 continue), R the screens (both, top only, tall area); a touch on the bottom screen sets the iris
//                 centre; A changes room, B changes room with a loading step (40 slices, the curtain's progress
//                 bar, a 20-frame hold), X goes through the loading scene; the music fades with the picture;
//   self-check    SELECT: a fixed 1200-frame script on the real transitions, "transitions: <frame> sum ..." every
//                 120 frames, then "transitions: PASS <sum>" when the bits are the simulator's (the console must
//                 match); back to the room;
//   bench         Y: each effect measured over 300 frames, "transitions bench: ..." lines and a verdict.
// START quits.
//
// TRANSITIONS_DEMO picks the first scene: showcase (default), check, bench. TRANSITIONS_SHOT=<fade|wipe|split|
// blinds|iris|diamond|curtain|tall|loading> starts in a room and begins a change at frame 20 (out 40: mid-coverage at
// frame 40, full at 60, the loading bar from 62). Screenshots (Debug/Development):
//   TRANSITIONS_SHOT=iris NNH_EXIT_AFTER_FRAMES=40 NNH_SCREENSHOT=build/iris.png
//       build/pc-debug/examples/transitions/transitions.exe
// Console test (spec 013): nnh run transitions --script examples/transitions/tests/check.nnhs

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <nnh/app.hpp>
#include <nnh/gfx.hpp>

#include "bench_scene.hpp"
#include "check_script.hpp"
#include "demos.hpp"
#include "room_scene.hpp"
#include "title_scene.hpp"

NNH_LOG_CHANNEL(example);

bool demo_is(const char* name) {
    const char* demo = std::getenv("TRANSITIONS_DEMO");
    return demo != nullptr && std::strcmp(demo, name) == 0;
}

const char* shot_mode() { return std::getenv("TRANSITIONS_SHOT"); }

namespace {

void log_checkpoint(const TransitionsCheck& check) {
    char line[96];
    format_transitions_checkpoint(line, sizeof line, check.frame(), check.checksum());
    NNH_LOG_INFO(example, "%s", line);
    if (!check.done()) return;
    format_transitions_verdict(line, sizeof line, check);
    if (std::strncmp(line, "transitions: PASS", 17) == 0) {
        NNH_LOG_INFO(example, "%s", line);
    } else {
        NNH_LOG_ERROR(example, "%s", line);
    }
}

/// The scenes, on the heap (the 3DS main-thread stack is small).
struct Game {
    World world;
    Curtain curtain;
    InputLog input;
    Demos demos;
    TitleScene title{demos};
    RoomScene room1{demos, 0};
    RoomScene room2{demos, 1};
    LoadingScene loading{demos};
    BenchScene bench{demos};
    TransitionsCheck check;

    Game() {
        demos.world = &world;
        demos.curtain = &curtain;
        demos.input = &input;
        demos.title = &title;
        demos.rooms[0] = &room1;
        demos.rooms[1] = &room2;
        demos.loading = &loading;
        demos.bench = &bench;
        demos.check = &check;
        check.on_checkpoint = log_checkpoint;
    }
    nnh::Scene& first() {
        if (demo_is("check")) {
            check.return_to = &room1;
            NNH_LOG_INFO(example, "transitions: check started");
            return check.driver();
        }
        if (demo_is("bench")) return bench;
        if (shot_mode() != nullptr) return room1;
        return title;
    }
};

} // namespace

int main() {
    // The iris with 1-pixel bands draws up to 482 rectangles per screen; the curtain about 700 sprites.
    if (!nnh::init({.title = "NNHGE Transitions", .gfx = {.max_draws = 4096, .max_tiles = 2048}})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(12, 10, 24));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(20, 18, 30));
    {
        std::unique_ptr<Game> game(new (std::nothrow) Game());
        if (game != nullptr && game->world.build()) {
            game->curtain.set_tiles(&game->world.tiles);
            game->input.attach();
            nnh::run(game->first()); // until START
            game->input.detach();
        }
    } // scenes, maps and textures go before the engine
    nnh::shutdown();
    return 0;
}
