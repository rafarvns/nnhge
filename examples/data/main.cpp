// data: data assets (spec 014) — a small bestiary and shop. The game's types are plain structs
// (data_types.hpp) whose schema is declared once in data_types.cpp (DATA_SCHEMA in CMakeLists.txt); every
// enemy, item and the game's settings are hand-written text files in assets/ (enemies/goblin.enemy, ...),
// checked and converted by the build. Change a number there and rebuild: no C++ is recompiled.
//   UP/DOWN select an enemy · A its hurt sound · SELECT the self-check · START quit
//
// DATA_DEMO=check starts with the self-check (a PASS/FAIL banner and the line the console test expects).
// Screenshots (Debug/Development):
//   NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/data.png build/pc-debug/examples/data/data.exe
// Console test (spec 013): nnh run data --script examples/data/tests/check.nnhs

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <nnh/app.hpp>

#include "bestiary_scene.hpp"
#include "check_scene.hpp"

namespace {

/// The scenes, on the heap (the 3DS main-thread stack is small).
struct Game {
    CheckScene check;
    BestiaryScene bestiary{check};
};

} // namespace

int main() {
    if (!nnh::init()) return 1; // the window title comes from TITLE in CMakeLists.txt
    {
        std::unique_ptr<Game> game(new (std::nothrow) Game());
        const char* demo = std::getenv("DATA_DEMO");
        if (game != nullptr) {
            if (demo != nullptr && std::strcmp(demo, "check") == 0) {
                nnh::run(game->check);
            } else {
                nnh::run(game->bestiary);
            }
        }
    } // textures and sounds go before the engine
    nnh::shutdown();
    return 0;
}
