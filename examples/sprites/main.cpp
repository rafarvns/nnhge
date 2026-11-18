// sprites: textures from the game's IMAGES, sprite-sheet animation, layers, a camera that follows the
// player, a camera-free HUD, rotation/tint/opacity, and shapes (a minimap on the bottom screen).
// D-Pad or Circle Pad to walk, START to exit.
//
// Screenshot after walking right for a second and a half (Debug/Development):
//   NNH_SIM_INPUT="buttons=Right" NNH_EXIT_AFTER_FRAMES=90 NNH_SCREENSHOT=build/sprites.png \
//   build/pc-debug/examples/sprites/sprites.exe

#include <cstdio>
#include <utility>

#include <nnh/app.hpp>
#include <nnh/debug.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/log.hpp>

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
    if (!nnh::init({.title = "NNHGE Sprites"})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(110, 180, 230));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(24, 24, 32));
    {
        const char* error = nullptr;
        Art art{load("hero", error), load("tiles", error)};
        World world(art);
        while (nnh::frame()) {
            if (nnh::input::pressed(nnh::Button::Start)) break;
            if (error != nullptr) {
                nnh::debug::print(nnh::Screen::Top, 8, 8, error, nnh::colors::red);
                continue;
            }
            world.update();
            world.draw();
        }
    } // textures freed here (shutdown() would free them too)
    nnh::shutdown();
    return 0;
}
