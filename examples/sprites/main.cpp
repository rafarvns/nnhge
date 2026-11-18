// sprites: textures and a level file from the game's assets (spec 007), sprite-sheet animation, layers,
// a camera that follows the player, a camera-free HUD, rotation/tint/opacity, and shapes (a minimap on
// the bottom screen).
// D-Pad or Circle Pad to walk, SELECT for the benchmark (many sprites), START to exit.
//
// Screenshot after walking right for a second and a half (Debug/Development):
//   NNH_SIM_INPUT="buttons=Right" NNH_EXIT_AFTER_FRAMES=90 NNH_SCREENSHOT=build/sprites.png \
//   build/pc-debug/examples/sprites/sprites.exe
// Benchmark screenshot: SPRITES_DEMO=bench (PC; on the console press SELECT).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include <nnh/app.hpp>
#include <nnh/assets.hpp>
#include <nnh/debug.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/log.hpp>

#include "bench.hpp"
#include "world.hpp"

NNH_LOG_CHANNEL(sprites);

namespace {

/// Loads a texture; on failure logs why and keeps the message to show it on screen.
nnh::Texture load(const char* name, const char*& error) {
    auto result = nnh::load_texture(name);
    if (result) return std::move(result.value());
    NNH_LOG_ERROR(sprites, "%s", result.error().message);
    static char message[64];
    std::snprintf(message, sizeof message, "%s", result.error().message);
    error = message;
    return {};
}

} // namespace

int main() {
    // The benchmark draws up to 8000 sprites per frame: raise the default 2048.
    if (!nnh::init({.title = "NNHGE Sprites", .gfx = {.max_draws = 8192}})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(110, 180, 230));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(24, 24, 32));
    {
        const char* error = nullptr;
        Art art{load("art/hero", error), load("art/tiles", error)};
        static Level level;
        // A data file from the assets: read whole, parsed, released at the end of this block.
        auto file = nnh::assets::load("levels/level1.txt");
        if (!file || !parse_level(static_cast<const char*>(file.value().data()), file.value().size(), level)) {
            error = "levels/level1.txt is missing or invalid";
        }
        World world(art, level);
        static Bench bench(art.hero); // ~128 KB of positions: static, not on the stack
        const char* demo = std::getenv("SPRITES_DEMO");
        bool benchmark = demo != nullptr && std::strcmp(demo, "bench") == 0;
        while (nnh::frame()) {
            if (nnh::input::pressed(nnh::Button::Start)) break;
            if (nnh::input::pressed(nnh::Button::Select)) benchmark = !benchmark;
            if (error != nullptr) {
                nnh::debug::print(nnh::Screen::Top, 8, 8, error, nnh::colors::red);
                continue;
            }
            if (benchmark) {
                bench.update();
                bench.draw();
            } else {
                world.update();
                world.draw();
            }
        }
    } // textures freed here (shutdown() would free them too)
    nnh::shutdown();
    return 0;
}
