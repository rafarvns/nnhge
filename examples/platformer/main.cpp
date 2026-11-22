// platformer: tilemaps (spec 012). The bonus level built entirely in code (parallax backdrops, solid and one-way
// tiles, a front layer over the player, animated water, coins as map objects), a tile benchmark and a flip test.
// The Tiled level and replay mode come in stage 7. SELECT cycles bonus level → benchmark → flip test; START exits.
//
// Screenshots (Debug/Development):
//   NNH_SIM_INPUT="buttons=Right" NNH_EXIT_AFTER_FRAMES=120 NNH_SCREENSHOT=build/platformer.png \
//   build/pc-debug/examples/platformer/platformer.exe
//   PLATFORMER_DEMO=bench (4 full layers + 300 sprites, the gate) or PLATFORMER_DEMO=flips.
//   PLATFORMER_DEMO=gate starts the benchmark's automatic gate at once (A in the benchmark does too); it logs
//   each trial and ends with `bench: PASS ...` or `bench: FAIL ...` (about 9000 frames when every trial
//   passes). Console test: nnh run platformer --script examples/platformer/tests/gate.nnhs

#include "art.hpp"
#include "bench.hpp"
#include "bonus.hpp"
#include "bonus_level.hpp"
#include "flips.hpp"

#include <cstdlib>
#include <cstring>
#include <nnh/app.hpp>
#include <nnh/debug.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/log.hpp>
#include <utility>

NNH_LOG_CHANNEL(platformer);

namespace {

enum class Demo { Bonus, Bench, Flips };

bool env_is(const char* value) {
    const char* demo = std::getenv("PLATFORMER_DEMO");
    return demo != nullptr && std::strcmp(demo, value) == 0;
}

Demo demo_from_env() {
    if (env_is("bench") || env_is("gate")) return Demo::Bench;
    if (env_is("flips")) return Demo::Flips;
    return Demo::Bonus;
}

Demo next(Demo d) {
    return d == Demo::Bonus ? Demo::Bench : (d == Demo::Bench ? Demo::Flips : Demo::Bonus);
}

nnh::Texture load(const char* name, const char*& error) {
    auto result = nnh::load_texture(name);
    if (result) return std::move(result.value());
    NNH_LOG_ERROR(platformer, "%s", result.error().message);
    error = "art is missing: see the log";
    return {};
}

/// The demos; built once (each map is one allocation at load time).
struct Demos {
    nnh::Tilemap bonus_map;
    Bonus bonus;
    Bench bench;
    Flips flips;
    Demos(const Art& art, nnh::Tilemap map)
        : bonus_map(std::move(map)), bonus(art, bonus_map), bench(art), flips(art) {}
};

} // namespace

int main() {
    // Tile quads per frame: the benchmark's 12 full layers. Sprites: up to 2000 + the HUD text.
    if (!nnh::init({.title = "NNHGE Platformer", .gfx = {.max_draws = 4096, .max_tiles = Bench::kMaxTiles}}))
        return 1;
    nnh::input::set_circle_pad_mirrors_dpad(true);
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(120, 190, 240));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(24, 26, 36));
    {
        const char* error = nullptr;
        Art art{load("art/tiles", error), load("art/player", error)};
        nnh::Tilemap level;
        if (error == nullptr) {
            auto built = build_bonus_level(art.tiles);
            if (built) {
                level = std::move(built.value());
            } else {
                NNH_LOG_ERROR(platformer, "bonus level: %s", built.error().message);
                error = "the bonus level could not be built: see the log";
            }
        }
        Demos demos(art, std::move(level)); // maps are destroyed before their textures (declared after `art`)
        if (error == nullptr && (!demos.bench.ready() || !demos.flips.ready()))
            error = "out of memory for the demos";
        Demo demo = demo_from_env();
        if (error == nullptr && env_is("gate")) demos.bench.start_gate();
        while (nnh::frame()) {
            if (nnh::input::pressed(nnh::Button::Start)) break;
            if (error != nullptr) {
                nnh::debug::print(nnh::Screen::Top, 8, 8, error, nnh::colors::red);
                continue;
            }
            if (nnh::input::pressed(nnh::Button::Select)) {
                if (demo == Demo::Bench) demos.bench.leave();
                demo = next(demo);
            }
            switch (demo) {
            case Demo::Bonus:
                demos.bonus.update();
                demos.bonus.draw();
                break;
            case Demo::Bench:
                demos.bench.update();
                demos.bench.draw();
                break;
            case Demo::Flips:
                demos.flips.update();
                demos.flips.draw();
                break;
            }
        }
    }
    nnh::shutdown();
    return 0;
}
