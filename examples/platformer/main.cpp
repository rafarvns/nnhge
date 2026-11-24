// platformer: tilemaps and collision (spec 012). Four demos, each a scene (SELECT: the next one):
//   level 1      a level made in Tiled (assets/levels/level1.tmj, converted by the build): parallax
//   backdrops,
//                a collision layer, one-way planks, animated water, coins, a spawn point, a door, properties,
//                and two turrets that watch the player (line of sight) and shoot bullets moved by rays (spec 019:
//                collision layers and queries; the player, coins and bullets have layers: layers.hpp);
//   bonus level  the same kind of level built entirely in code (bonus_level.cpp), no file and no editor;
//   benchmark    full tile layers + sprites, with the automatic 60 Hz gate (A);
//   flip test    Tiled's 8 tile orientations next to sprite flips.
// In a level, X starts the replay: a scripted run of level 1 then the bonus level that logs a checksum every
// 60 frames (compare the PC's and the console's logs) and a 60 Hz verdict. START exits.
//
// PLATFORMER_DEMO picks the first demo: level1 (default), bonus, bench, gate (the benchmark's gate at once),
// flips, replay (level 1 with the replay at once), doors (the door run: ten iris transitions through the levels'
// doors, spec 023; Y in a level starts it too). Screenshots (Debug/Development):
//   NNH_SIM_INPUT="buttons=Right" NNH_EXIT_AFTER_FRAMES=120 NNH_SCREENSHOT=build/platformer.png \
//   build/pc-debug/examples/platformer/platformer.exe
// Console tests (spec 013): nnh run platformer --script examples/platformer/tests/level.nnhs (levels, replay)
// and tests/gate.nnhs (the tile benchmark).

#include "art.hpp"
#include "bench.hpp"
#include "bonus_level.hpp"
#include "demos.hpp"
#include "flips.hpp"
#include "level_scene.hpp"
#include "replay.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <nnh/app.hpp>
#include <nnh/debug.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/log.hpp>
#include <utility>

NNH_LOG_CHANNEL(platformer);

namespace {

bool env_is(const char* value) {
    const char* demo = std::getenv("PLATFORMER_DEMO");
    return demo != nullptr && std::strcmp(demo, value) == 0;
}

Demo demo_from_env() {
    if (env_is("bonus")) return Demo::Bonus;
    if (env_is("bench") || env_is("gate")) return Demo::Bench;
    if (env_is("flips")) return Demo::Flips;
    return Demo::Level1;
}

nnh::Texture load(const char* name, const char*& error) {
    auto result = nnh::load_texture(name);
    if (result) return std::move(result.value());
    NNH_LOG_ERROR(platformer, "%s", result.error().message);
    error = "art is missing: see the log";
    return {};
}

/// Loads level 1 (levels/level1.nnhmap, converted from the Tiled file by the build; it loads its own
/// tileset).
nnh::Tilemap load_level1(const char*& error) {
    auto loaded = nnh::load_tilemap("levels/level1");
    if (!loaded) {
        NNH_LOG_ERROR(platformer, "level 1: %s", loaded.error().message);
        error = "level 1 could not be loaded: see the log";
        return {};
    }
    const nnh::Properties props = loaded.value().properties();
    NNH_LOG_INFO(platformer, "level 1: %dx%d tiles, %d layers, %d objects, music \"%s\"",
                 loaded.value().width(), loaded.value().height(), loaded.value().tile_layer_count(),
                 loaded.value().object_count(), props.get_string("music"));
    return std::move(loaded.value());
}

nnh::Tilemap build_bonus(const nnh::Texture& tiles, const char*& error) {
    auto built = build_bonus_level(tiles);
    if (built) return std::move(built.value());
    NNH_LOG_ERROR(platformer, "bonus level: %s", built.error().message);
    error = "the bonus level could not be built: see the log";
    return {};
}

/// Everything the demos use, made once at load time (on the heap: the 3DS stack is small). Members are
/// destroyed in reverse order: scenes before the maps, maps before the textures.
struct Game {
    Art art;
    nnh::Tilemap level1;
    nnh::Tilemap bonus;
    Replay replay;
    DoorRun doors;
    LevelScene level1_scene;
    LevelScene bonus_scene;
    Bench bench;
    Flips flips;
    Demos demos;

    Game(Art a, nnh::Tilemap l1, nnh::Tilemap b)
        : art(std::move(a)), level1(std::move(l1)), bonus(std::move(b)),
          level1_scene(Demo::Level1, art, level1, "made in Tiled: levels/level1.tmj", replay, doors),
          bonus_scene(Demo::Bonus, art, bonus, "built in code: no file, no editor", replay, doors), bench(art),
          flips(art), demos({&level1_scene, &bonus_scene, &bench, &flips}, replay, doors) {}
    bool ready() const { return bench.ready() && flips.ready(); }
};

std::unique_ptr<Game> make_game(const char*& error) {
    Art art{load("art/tiles", error), load("art/player", error)};
    if (error != nullptr) return nullptr;
    nnh::Tilemap level1 = load_level1(error);
    nnh::Tilemap bonus = error == nullptr ? build_bonus(art.tiles, error) : nnh::Tilemap{};
    if (error != nullptr) return nullptr;
    std::unique_ptr<Game> game(new (std::nothrow) Game(std::move(art), std::move(level1), std::move(bonus)));
    if (game == nullptr || !game->ready()) error = "out of memory for the demos";
    return game;
}

} // namespace

int main() {
    // Tile quads per frame: the benchmark's 12 full layers. Sprites: up to 2000 + the HUD text.
    if (!nnh::init({.title = "NNHGE Platformer", .gfx = {.max_draws = 4096, .max_tiles = Bench::kMaxTiles}}))
        return 1;
    nnh::input::set_circle_pad_mirrors_dpad(true);
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(120, 190, 240));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(24, 26, 36));
    const char* error = nullptr;
    std::unique_ptr<Game> game = make_game(error);
    if (error == nullptr) {
        if (env_is("gate")) game->bench.start_gate();
        if (env_is("replay")) game->replay.start();
        if (env_is("doors")) game->doors.start(); // ten irises through the levels' doors (spec 023)
        nnh::run(game->demos.first(demo_from_env())); // until START; exits every scene at the end
    } else {
        while (nnh::frame() && !nnh::input::pressed(nnh::Button::Start))
            nnh::debug::print(nnh::Screen::Top, 8, 8, error, nnh::colors::red);
    }
    game.reset(); // maps and textures go before the engine
    nnh::shutdown();
    return 0;
}
