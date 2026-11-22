// The platformer's player physics (examples/platformer/player.hpp) on a grid drawn as text: runs, jumps,
// lands, drops through one-way planks. Input comes as PlayerInput, so a scripted replay plays exactly like
// buttons.

#include "platformer/player.hpp"
#include "test.hpp"

namespace {

using nnh::collision::Cell;

// 8 x 6 cells of 16 px: a plank row (y 3) over a floor (y 5).
constexpr Cell E = Cell::Empty, S = Cell::Solid, O = Cell::OneWay;
constexpr Cell kCells[] = {E, E, E, E, E, E, E, E, //
                           E, E, E, E, E, E, E, E, //
                           E, E, E, E, E, E, E, E, //
                           E, O, O, O, O, E, E, E, //
                           E, E, E, E, E, E, E, E, //
                           S, S, S, S, S, S, S, S};

nnh::collision::Grid grid() {
    nnh::collision::Grid g = nnh::collision::grid_view(kCells, 8, 6, 16, 16);
    g.outside = Cell::Solid;
    return g;
}

/// Runs `frames` frames with `input` (a jump only on the first one).
void run(Player& player, PlayerInput input, int frames) {
    for (int i = 0; i < frames; ++i) {
        player.update(grid(), input);
        input.jump = false;
    }
}

float feet(const Player& player) {
    return player.box().y + player.box().h;
}

} // namespace

NNH_TEST(player_falls_and_lands_on_the_floor) {
    Player player;
    player.spawn({100, 20});
    run(player, {}, 60);
    CHECK(player.on_ground());
    CHECK(feet(player) == 80);
}

NNH_TEST(player_lands_on_a_one_way_plank_from_above) {
    Player player;
    player.spawn({40, 10});
    run(player, {}, 60);
    CHECK(player.on_ground());
    CHECK(feet(player) == 48);
}

NNH_TEST(player_runs_right_at_a_fixed_speed) {
    Player player;
    player.spawn({100, 80});
    run(player, {}, 2);
    const float x = player.box().x;
    run(player, {.run = 1}, 10);
    CHECK(player.box().x == x + 17.5f);
}

NNH_TEST(player_jumps_from_the_ground) {
    Player player;
    player.spawn({100, 80});
    run(player, {}, 2);
    run(player, {.jump = true}, 5);
    CHECK(!player.on_ground());
    CHECK(feet(player) < 80);
}

NNH_TEST(player_does_not_jump_in_the_air) {
    Player player;
    player.spawn({100, 20});
    run(player, {}, 2);
    const float before = feet(player);
    run(player, {.jump = true}, 1);
    CHECK(feet(player) > before); // still falling
}

NNH_TEST(player_jumps_up_through_a_plank_and_lands_on_it) {
    Player player;
    player.spawn({40, 80});
    run(player, {}, 2);
    run(player, {.jump = true}, 60);
    CHECK(player.on_ground());
    CHECK(feet(player) == 48);
}

NNH_TEST(player_drops_through_a_plank_with_down_and_jump) {
    Player player;
    player.spawn({40, 10});
    run(player, {}, 60);
    run(player, {.jump = true, .down = true}, 60);
    CHECK(player.on_ground());
    CHECK(feet(player) == 80);
}

NNH_TEST(player_spawn_places_the_feet_and_stops_it) {
    Player player;
    player.spawn({50, 64});
    CHECK(feet(player) == 64);
    CHECK(player.box().x + player.box().w / 2 == 50);
    CHECK(!player.on_ground());
}
