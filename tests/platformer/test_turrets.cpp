// The platformer's turrets (examples/platformer/turrets.hpp, spec 019 AC-4 step 2; spec 020 AC-4 step 1: turrets are
// entities of the level's world, bullets come from a pool): sight, firing, bullets moved by rays against the world's
// body storage (no tunnelling, flush on walls, hitting the player, passing turrets), the state word. The pool is not
// bound here: update() flushes it first, so the first free bullet is the first one the old book would have used.

#include "../collision/ascii_grid.hpp"
#include "platformer/level_world.hpp"
#include "platformer/turrets.hpp"
#include "test.hpp"

using nnh::Rect;
using nnh::Vec2;
using nnh::test::AsciiGrid;

namespace {

// 16 x 16 cells, 20 x 6. A wall at column 8 (rows 1..4) with a gap at row 3; a plank at row 2 over columns 12..14.
const char* const kRows[] = {
    "....................",
    "........#...........",
    "........#...===.....",
    "....................",
    "........#...........",
    "####################",
};

/// The grid, a world with the player's entity, and the book (as LevelPlay holds them).
struct World {
    AsciiGrid g{kRows, 6};
    nnh::World world = static_cast<nnh::World&&>(level::create_world().value());
    nnh::Entity player = world.spawn();
    TurretBook book{world};
    World() {
        g.grid.outside = nnh::collision::Cell::Solid;
        world.add<level::Body>(player, {{}, layers::kPlayer});
    }
    nnh::Entity add(Rect box) { return TurretBook::spawn_turret(world, box); }
    /// One frame with the player's box at `p` (LevelPlay writes the player's body once per frame).
    void update(Rect p) {
        world.get<level::Body>(player)->box = p;
        book.update(g.grid, player);
    }
    const level::Turret& turret(nnh::Entity e) { return *world.get<level::Turret>(e); }
    bool active(int slot) { return book.bullets().is_live(slot); }
    const TurretBook::Bullet& bullet(int slot) { return book.bullets().data()[slot]; }
};

const Rect kTurret{32, 48, 16, 16};   // centre (40, 56): row 3, the gap's row
const Rect kPlayerOpen{176, 48, 12, 14}; // centre (182, 55): row 3, through the gap

} // namespace

NNH_TEST(a_turret_sees_the_player_through_a_gap_and_fires_at_once) {
    World w;
    const nnh::Entity t = w.add(kTurret);
    w.update(kPlayerOpen);
    CHECK(w.turret(t).sees);
    CHECK(w.active(0) && w.bullet(0).velocity == Vec2(2, 0));
    CHECK(w.turret(t).cooldown == TurretBook::kFirePeriod);
    CHECK(w.world.has_tag(t, level::kTurretTag));
}

NNH_TEST(a_wall_hides_the_player) {
    World w;
    const nnh::Entity t = w.add(kTurret);
    w.update({176, 66, 12, 12}); // centre (182, 72): row 4, behind the wall's foot
    CHECK(!w.turret(t).sees && !w.active(0));
}

NNH_TEST(planks_never_hide_the_player) {
    World w;
    const nnh::Entity t = w.add({200, 48, 16, 16}); // centre (208, 56) under the plank
    w.update({200, 8, 12, 14});                     // centre (206, 15): above the plank, straight up
    CHECK(w.turret(t).sees);
}

NNH_TEST(the_player_out_of_range_is_not_seen) {
    World w;
    const nnh::Entity t0 = w.add({0, 0, 16, 16});
    const nnh::Entity t1 = w.add({300, 0, 16, 16});
    w.update({300, 0, 12, 14}); // 300 px right of turret 0, on turret 1
    CHECK(!w.turret(t0).sees && w.turret(t1).sees);
    w.update({0, 0, 12, 14}); // 300 px left of turret 1
    CHECK(w.turret(t0).sees && !w.turret(t1).sees);
}

NNH_TEST(planks_never_hide_the_player_below_either) {
    World w;
    const nnh::Entity t = w.add({202, 0, 16, 16}); // centre (210, 8): above the plank of row 2 (not on a grid line)
    w.update({204, 48, 12, 14});                   // centre (210, 55): below it, straight down through its top
    CHECK(w.turret(t).sees);
}

NNH_TEST(a_turret_fires_every_90_frames_while_it_sees) {
    World w;
    const nnh::Entity t = w.add(kTurret);
    int shots = 0;
    for (int frame = 0; frame < 181; ++frame) {
        const int before = w.turret(t).cooldown;
        w.update({120, 48, 12, 14}); // in front of the wall, seen
        shots += w.turret(t).cooldown > before ? 1 : 0;
    }
    CHECK(shots == 3); // frames 0, 90 and 180
}

NNH_TEST(bullets_aim_in_eight_directions) {
    CHECK(TurretBook::aim({100, 10}) == Vec2(2, 0));
    CHECK(TurretBook::aim({-100, -30}) == Vec2(-2, 0));
    CHECK(TurretBook::aim({5, -90}) == Vec2(0, -2));
    CHECK(TurretBook::aim({-20, 60}) == Vec2(0, 2));
    CHECK(TurretBook::aim({50, 50}) == Vec2(1.375f, 1.375f));
    CHECK(TurretBook::aim({-50, 40}) == Vec2(-1.375f, 1.375f));
    CHECK(TurretBook::aim({30, -45}) == Vec2(1.375f, -1.375f));
    CHECK(TurretBook::aim({100, 39}) == Vec2(2, 0) && TurretBook::aim({100, 45}) == Vec2(1.375f, 1.375f));
    CHECK(TurretBook::aim({39, 100}) == Vec2(0, 2) && TurretBook::aim({45, -100}) == Vec2(1.375f, -1.375f));
}

NNH_TEST(a_fast_bullet_stops_flush_at_a_one_cell_wall) {
    World w;
    w.book.shoot({40, 24}, {100, 0}); // 100 px per frame towards the wall at x 128..144
    w.update({300, 48, 12, 14});
    CHECK(w.active(0) && w.bullet(0).stuck == TurretBook::kStuckFrames);
    CHECK(w.bullet(0).position == Vec2(128, 24) && w.bullet(0).normal == Vec2(-1, 0));
    for (int i = 0; i < TurretBook::kStuckFrames; ++i) w.update({300, 48, 12, 14});
    CHECK(!w.active(0));
}

NNH_TEST(a_bullet_falling_on_a_plank_stops_on_it) {
    World w;
    w.book.shoot({200, 4}, {0, 2});
    for (int i = 0; i < 20; ++i) w.update({0, 48, 12, 14});
    CHECK(w.bullet(0).stuck > 0 && w.bullet(0).position == Vec2(200, 32));
}

NNH_TEST(a_bullet_hits_the_player_and_flashes_it_without_moving_it) {
    World w;
    const Rect player{60, 48, 12, 14};
    w.book.shoot({40, 55}, {2, 0});
    int frames = 0;
    while (w.book.hits() == 0 && frames < 20) {
        w.update(player);
        ++frames;
    }
    CHECK(w.book.hits() == 1 && frames == 11 && w.book.flash() == TurretBook::kFlashFrames); // flush at 10: not yet
    CHECK(!w.active(0));
    w.update(player);
    CHECK(w.book.flash() == TurretBook::kFlashFrames - 1);
    CHECK(w.world.get<level::Body>(w.player)->box.x == 60);
}

NNH_TEST(a_player_running_into_a_bullet_is_hit) {
    World w;
    w.book.shoot({70, 55}, {-2, 0});
    w.update({66, 48, 12, 14}); // the bullet is inside the player's box at the start of its step
    CHECK(w.book.hits() == 1 && !w.active(0));
}

NNH_TEST(bullets_pass_through_turrets) {
    World w;
    w.add({100, 0, 16, 16}); // out of sight of the player below; on the bullet's path
    w.book.shoot({60, 8}, {8, 0});
    for (int i = 0; i < 8; ++i) w.update({16, 66, 12, 12});
    CHECK(w.active(0) && w.bullet(0).position.x == 124 && w.bullet(0).stuck == 0);
}

NNH_TEST(bullets_pass_through_coins) {
    World w;
    const nnh::Entity coin = w.world.spawn();
    w.world.add<level::Body>(coin, {{100, 0, 16, 16}, layers::kCoin});
    w.book.shoot({60, 8}, {8, 0});
    for (int i = 0; i < 8; ++i) w.update({16, 66, 12, 12});
    CHECK(w.active(0) && w.bullet(0).position.x == 124 && w.book.hits() == 0);
}

NNH_TEST(a_released_bullet_slot_is_reused_by_the_next_frames_shot) {
    // Old book: the first inactive bullet. Pool: the lowest free slot after the flush at the start of update().
    World w;
    w.book.shoot({70, 55}, {-2, 0});
    w.book.shoot({40, 24}, {100, 0});
    w.update({66, 48, 12, 14}); // bullet 0 hits the player (released), bullet 1 sticks to the wall
    CHECK(!w.active(0) && w.active(1));
    w.update({300, 48, 12, 14}); // flushed first
    CHECK(w.book.shoot({0, 0}, {1, 0}));
    CHECK(w.active(0));
}

NNH_TEST(the_state_word_follows_bullets_and_hits) {
    World a, b;
    a.add(kTurret);
    b.add(kTurret);
    CHECK(a.book.state() == b.book.state());
    a.update(kPlayerOpen);
    CHECK(a.book.state() != b.book.state());
    b.update(kPlayerOpen);
    CHECK(a.book.state() == b.book.state());
    a.book.restart();
    CHECK(a.book.state() != b.book.state() && a.book.hits() == 0 && !a.active(0));
}

NNH_TEST(the_state_word_follows_a_stuck_bullet_frame_by_frame) {
    // Mutation testing (plan 020 T8.3): a stuck bullet keeps its position; only its countdown changes.
    World w;
    w.book.shoot({40, 24}, {100, 0});
    w.update({300, 48, 12, 14}); // stuck on the wall
    const std::uint32_t before = w.book.state();
    w.update({300, 48, 12, 14});
    CHECK(w.bullet(0).position == Vec2(128, 24) && w.book.state() != before);
}
