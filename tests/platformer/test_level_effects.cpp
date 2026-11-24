// The platformer's particle effects (spec 024 AC-5; T6.1): dust when the player lands and when it starts running, a puff
// where a bullet stops on a wall or hits the player, a flash at a turret's muzzle when it fires; nothing without a
// trigger; the effects read the player, the bullets, the turrets and the barrels without changing them.

#include "../collision/ascii_grid.hpp"
#include "platformer/level_effects.hpp"
#include "platformer/level_prefabs.hpp"
#include "platformer/level_world.hpp"
#include "test.hpp"

using nnh::Rect;
using nnh::Vec2;
using nnh::test::AsciiGrid;

namespace {

// 16 x 16 cells, 20 x 6: a wall at column 8 (rows 1..4) with a gap at row 3, a floor.
const char* const kRows[] = {
    "....................",
    "........#...........",
    "........#...........",
    "....................",
    "........#...........",
    "####################",
};

struct Level {
    AsciiGrid g{kRows, 6};
    nnh::World world = static_cast<nnh::World&&>(level::create_world().value());
    level::LevelPrefabs prefabs = static_cast<level::LevelPrefabs&&>(level::create_level_prefabs(nullptr).value());
    nnh::Entity player_entity = world.spawn();
    TurretBook book{world};
    nnh::Hierarchy tree{world};
    Player player;
    LevelEffects fx;
    Level() {
        g.grid.outside = nnh::collision::Cell::Solid;
        world.add<level::Body>(player_entity, {{}, layers::kPlayer});
    }
    /// One frame as LevelPlay runs it: the player, the turrets, the pass, then the effects.
    void frame(PlayerInput input = {}) {
        player.update(g.grid, input);
        world.get<level::Body>(player_entity)->box = player.box();
        book.update(g.grid, player_entity);
        tree.propagate();
        const std::uint32_t state = book.state();
        const int count = world.count();
        fx.update({player, book, world, tree});
        CHECK(book.state() == state && world.count() == count); // read only
    }
};

} // namespace

NNH_TEST(level_effects_dust_when_the_player_lands_and_starts_running) {
    Level l;
    l.player.spawn({200, 40}); // in the air
    int landed_at = -1;
    for (int f = 0; f < 60 && landed_at < 0; ++f) {
        l.frame();
        if (l.player.on_ground()) landed_at = f;
    }
    CHECK(landed_at > 0 && l.fx.dust().alive() == LevelEffects::kLandingDust);
    const Vec2 feet = l.player.feet();
    for (int i = 0; i < l.fx.dust().alive(); ++i) {
        const Vec2 p = l.fx.dust().particle(i).position;
        CHECK(p.x > feet.x - 8 && p.x < feet.x + 8 && p.y > feet.y - 6 && p.y < feet.y + 2);
    }
    for (int f = 0; f < 30; ++f) l.frame(); // standing: nothing new
    CHECK(l.fx.dust().alive() == 0 && l.fx.dust().stats().emitted == LevelEffects::kLandingDust);
    for (int f = 0; f < 5; ++f) l.frame({.run = 1});
    CHECK(l.fx.dust().stats().emitted == LevelEffects::kLandingDust + LevelEffects::kRunDust);
}

NNH_TEST(level_effects_puff_where_a_bullet_stops_on_a_wall) {
    Level l;
    l.player.spawn({300, 80});
    l.frame();
    CHECK(l.book.shoot({100, 24}, {2, 0})); // row 1: into the wall at x = 128
    int frames = 0;
    while (l.fx.puff().alive() == 0 && frames < 30) {
        l.frame();
        ++frames;
    }
    CHECK(l.fx.puff().alive() == LevelEffects::kWallPuff);
    for (int f = 0; f < 5; ++f) l.frame(); // still stuck: no second puff
    CHECK(l.fx.puff().stats().emitted == LevelEffects::kWallPuff);
    for (int i = 0; i < l.fx.puff().alive(); ++i) CHECK(l.fx.puff().particle(i).velocity.x < 0); // along the normal
}

NNH_TEST(level_effects_flash_at_the_muzzle_and_puff_on_a_hit) {
    Level l;
    l.prefabs.library.spawn_with(l.world, l.prefabs.turret, level::Body{Rect{32, 64, 16, 16}, layers::kEnemy});
    l.player.spawn({100, 80}); // on the floor in front of the wall, level with the turret
    l.frame();
    CHECK(l.fx.flash().stats().emitted == LevelEffects::kMuzzleFlash);
    l.frame(); // cooling down: no flash
    CHECK(l.fx.flash().stats().emitted == LevelEffects::kMuzzleFlash);
    CHECK(l.fx.flash().particle(0).position.x > 40); // out of the barrel, to the right
    int frames = 0;
    while (l.book.hits() == 0 && frames < 120) {
        l.frame();
        ++frames;
    }
    CHECK(l.book.hits() == 1 && l.fx.puff().stats().emitted == LevelEffects::kHitPuff);
}

NNH_TEST(level_effects_clear_forgets_everything) {
    Level l;
    l.player.spawn({200, 40});
    for (int f = 0; f < 60; ++f) l.frame();
    l.fx.clear();
    CHECK(l.fx.dust().alive() == 0 && l.fx.puff().alive() == 0 && l.fx.flash().alive() == 0);
}
