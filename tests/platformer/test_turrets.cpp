// The platformer's turrets (examples/platformer/turrets.hpp, spec 019 AC-4 step 2): sight, firing, bullets moved by
// rays (no tunnelling, flush on walls, hitting the player, passing turrets), the state word.

#include "../collision/ascii_grid.hpp"
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

struct World {
    AsciiGrid g{kRows, 6};
    World() { g.grid.outside = nnh::collision::Cell::Solid; }
};

const Rect kTurret{32, 48, 16, 16};   // centre (40, 56): row 3, the gap's row
const Rect kPlayerOpen{176, 48, 12, 14}; // centre (182, 55): row 3, through the gap

} // namespace

NNH_TEST(a_turret_sees_the_player_through_a_gap_and_fires_at_once) {
    World w;
    TurretBook book;
    book.add(kTurret);
    book.update(w.g.grid, kPlayerOpen);
    CHECK(book.turret(0).sees);
    CHECK(book.bullet(0).active && book.bullet(0).velocity == Vec2(2, 0));
    CHECK(book.turret(0).cooldown == TurretBook::kFirePeriod);
}

NNH_TEST(a_wall_hides_the_player) {
    World w;
    TurretBook book;
    book.add(kTurret);
    book.update(w.g.grid, {176, 66, 12, 12}); // centre (182, 72): row 4, behind the wall's foot
    CHECK(!book.turret(0).sees && !book.bullet(0).active);
}

NNH_TEST(planks_never_hide_the_player) {
    World w;
    TurretBook book;
    book.add({200, 48, 16, 16});        // centre (208, 56) under the plank
    book.update(w.g.grid, {200, 8, 12, 14}); // centre (206, 15): above the plank, straight up
    CHECK(book.turret(0).sees);
}

NNH_TEST(the_player_out_of_range_is_not_seen) {
    World w;
    TurretBook book;
    book.add({0, 0, 16, 16});
    book.update(w.g.grid, {300, 0, 12, 14}); // 300 px away: beyond kRangeX
    CHECK(!book.turret(0).sees);
}

NNH_TEST(a_turret_fires_every_90_frames_while_it_sees) {
    World w;
    TurretBook book;
    book.add(kTurret);
    int shots = 0;
    for (int frame = 0; frame < 181; ++frame) {
        const int before = book.turret(0).cooldown;
        book.update(w.g.grid, {120, 48, 12, 14}); // in front of the wall, seen
        shots += book.turret(0).cooldown > before ? 1 : 0;
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
}

NNH_TEST(a_fast_bullet_stops_flush_at_a_one_cell_wall) {
    World w;
    TurretBook book;
    book.shoot({40, 24}, {100, 0}); // 100 px per frame towards the wall at x 128..144
    book.update(w.g.grid, {300, 48, 12, 14});
    CHECK(book.bullet(0).active && book.bullet(0).stuck == TurretBook::kStuckFrames);
    CHECK(book.bullet(0).position == Vec2(128, 24) && book.bullet(0).normal == Vec2(-1, 0));
    for (int i = 0; i < TurretBook::kStuckFrames; ++i) book.update(w.g.grid, {300, 48, 12, 14});
    CHECK(!book.bullet(0).active);
}

NNH_TEST(a_bullet_falling_on_a_plank_stops_on_it) {
    World w;
    TurretBook book;
    book.shoot({200, 4}, {0, 2});
    for (int i = 0; i < 20; ++i) book.update(w.g.grid, {0, 48, 12, 14});
    CHECK(book.bullet(0).stuck > 0 && book.bullet(0).position == Vec2(200, 32));
}

NNH_TEST(a_bullet_hits_the_player_and_flashes_it_without_moving_it) {
    World w;
    TurretBook book;
    const Rect player{60, 48, 12, 14};
    book.shoot({40, 55}, {2, 0});
    int frames = 0;
    while (book.hits() == 0 && frames < 20) {
        book.update(w.g.grid, player);
        ++frames;
    }
    CHECK(book.hits() == 1 && frames == 11 && book.flash() == TurretBook::kFlashFrames); // flush at 10: not yet
    CHECK(!book.bullet(0).active);
    book.update(w.g.grid, player);
    CHECK(book.flash() == TurretBook::kFlashFrames - 1);
}

NNH_TEST(a_player_running_into_a_bullet_is_hit) {
    World w;
    TurretBook book;
    book.shoot({70, 55}, {-2, 0});
    book.update(w.g.grid, {66, 48, 12, 14}); // the bullet is inside the player's box at the start of its step
    CHECK(book.hits() == 1 && !book.bullet(0).active);
}

NNH_TEST(bullets_pass_through_turrets) {
    World w;
    TurretBook book;
    book.add({100, 0, 16, 16}); // out of sight of the player below; on the bullet's path
    book.shoot({60, 8}, {8, 0});
    for (int i = 0; i < 8; ++i) book.update(w.g.grid, {16, 66, 12, 12});
    CHECK(book.bullet(0).active && book.bullet(0).position.x == 124 && book.bullet(0).stuck == 0);
}

NNH_TEST(the_state_word_follows_bullets_and_hits) {
    World w;
    TurretBook a, b;
    a.add(kTurret);
    b.add(kTurret);
    CHECK(a.state() == b.state());
    a.update(w.g.grid, kPlayerOpen);
    CHECK(a.state() != b.state());
    b.update(w.g.grid, kPlayerOpen);
    CHECK(a.state() == b.state());
    a.restart();
    CHECK(a.state() != b.state() && a.hits() == 0 && !a.bullet(0).active);
}
