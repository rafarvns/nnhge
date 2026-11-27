// reload: hot reload of assets (spec 031). Run a Debug or Development build with `nnh run reload [--pc]`, then edit
// and save anything in examples/reload/assets/ (art/*.png, levels/room.tmj, enemies/*.enemy, sfx/blip.wav,
// strings/*.strings, models/gem.glb; art/make_assets.py makes the art) and the running game shows the change.
// Textures, the sound, the gem and the strings change behind their handles; the room and the enemies are loaded again
// when a batch names them (world.cpp). RELOAD_DEMO=bench times the idle frame and asset reads instead.
//   NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/reload.png build/pc-debug/examples/reload/reload.exe
// Test script (simulator or console): nnh run reload [--pc] --script examples/reload/tests/check.nnhs

#include <memory>
#include <new>
#include <nnh/app.hpp>
#include <nnh/gfx.hpp>
#include <nnh/localization.hpp>

#include "bench_scene.hpp"
#include "reload_scene.hpp"
#include "world.hpp"

namespace {

/// The world and the scenes, on the heap (the 3DS main-thread stack is small).
struct Game {
    World world;
    ReloadScene showcase{world};
    BenchScene bench{world};
    nnh::Scene& first() { return demo_is("bench") ? static_cast<nnh::Scene&>(bench) : showcase; }
};

} // namespace

int main() {
    if (!nnh::init({.title = "NNHGE Reload", .gfx = {.max_draws = 512, .max_tiles = 512, .max_draws_3d = 8}})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(10, 12, 24));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(16, 18, 28));
    if (nnh::loc::start({.language = "en"})) {
        {
            std::unique_ptr<Game> game(new (std::nothrow) Game());
            if (game != nullptr && game->world.load()) nnh::run(game->first()); // until START
        } // the scenes and the world go before the engine
        nnh::loc::stop();
    }
    nnh::shutdown();
    return 0;
}
