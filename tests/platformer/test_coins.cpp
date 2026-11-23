// The platformer's coins (examples/platformer/coins.hpp, spec 020 AC-4 step 1; before: test_coin_book.cpp): coin
// entities with a body on layers::kCoin and a value, tagged "coin", taken once by a layered overlap query over the
// world's body storage (spec 019) and destroyed; a restart is clear() plus the same spawns. The cases and expected
// results of the coin book's tests, over the world; since spec 021 the coins come from the level's `coin` prefab
// (level_prefabs.hpp), placed and valued per copy as the map's objects place and value them.

#include "../entities/world_fixture.hpp"
#include "platformer/coins.hpp"
#include "platformer/level_prefabs.hpp"
#include "platformer/level_world.hpp"
#include "test.hpp"

using level::Body;
using level::Coin;
using nnh::Entity;
using nnh::World;

namespace {

constexpr nnh::collision::Layers kMask = layers::kPlayerMask;

/// The level's prefabs; spawn_coin() is a map object's coin: its bounds and its `value`.
struct Coins {
    level::LevelPrefabs p = static_cast<level::LevelPrefabs&&>(level::create_level_prefabs(nullptr).value());
    Entity spawn_coin(World& w, nnh::Rect bounds, int value) {
        return p.library.spawn_with(w, p.coin, Body{bounds, layers::kCoin}, Coin{value});
    }
};

struct Two : Coins {
    World world = static_cast<World&&>(level::create_world().value());
    Entity a = spawn_coin(world, {100, 50, 12, 12}, 1);
    Entity b = spawn_coin(world, {120, 50, 12, 12}, 5);
};

int coins(World& w) { return w.storage<Coin>().count(); }

} // namespace

NNH_TEST(coins_start_with_none) {
    World w = static_cast<World&&>(level::create_world().value());
    CHECK(coins(w) == 0);
}

NNH_TEST(coins_keep_their_bounds_value_and_tag_in_spawn_order) {
    Two t;
    CHECK(coins(t.world) == 2);
    CHECK(t.world.get<Body>(t.b)->box.x == 120 && t.world.get<Coin>(t.b)->value == 5);
    CHECK(t.world.get<Body>(t.a)->layers == layers::kCoin);
    CHECK(t.world.has_tag(t.a, level::kCoinTag));
    CHECK(t.world.storage<Coin>().slot_of(t.a) == 0 && t.world.storage<Coin>().slot_of(t.b) == 1);
}

NNH_TEST(coins_a_box_takes_the_coin_it_overlaps) {
    Two t;
    TakenCoin taken[4] = {};
    CHECK(collect_coins(t.world, {95, 55, 10, 14}, kMask, taken, 4) == 1);
    CHECK(taken[0].entity == t.a && taken[0].value == 1);
    CHECK(taken[0].centre == nnh::Vec2(106, 56)); // the box's centre: the sparkles burst there
    CHECK(!t.world.alive(t.a) && t.world.alive(t.b));
}

NNH_TEST(coins_are_taken_only_once) {
    Two t;
    TakenCoin taken[4];
    collect_coins(t.world, {95, 55, 10, 14}, kMask, taken, 4);
    CHECK(collect_coins(t.world, {95, 55, 10, 14}, kMask, taken, 4) == 0);
    CHECK(coins(t.world) == 1);
}

NNH_TEST(coins_a_box_that_only_touches_takes_nothing) {
    Two t;
    TakenCoin taken[4];
    CHECK(collect_coins(t.world, {90, 50, 10, 14}, kMask, taken, 4) == 0); // right edge at the coin's left edge
}

NNH_TEST(coins_every_coin_overlapped_in_one_frame_is_taken) {
    Two t;
    TakenCoin taken[4] = {};
    CHECK(collect_coins(t.world, {105, 50, 20, 14}, kMask, taken, 4) == 2);
    CHECK(taken[0].entity == t.a && taken[1].entity == t.b && taken[1].value == 5);
}

NNH_TEST(coins_writes_no_more_than_the_capacity) {
    Two t;
    TakenCoin taken[2] = {};
    CHECK(collect_coins(t.world, {105, 50, 20, 14}, kMask, taken, 1) == 2);
    CHECK(taken[0].entity == t.a);
    CHECK(!taken[1].entity);
    CHECK(!t.world.alive(t.b)); // taken all the same
}

NNH_TEST(coins_a_restart_is_clear_plus_the_same_spawns_in_the_same_slots) {
    Two t;
    TakenCoin taken[4];
    collect_coins(t.world, {105, 50, 20, 14}, kMask, taken, 4);
    t.world.clear();
    const Entity a = t.spawn_coin(t.world, {100, 50, 12, 12}, 1);
    const Entity b = t.spawn_coin(t.world, {120, 50, 12, 12}, 5);
    CHECK(a.slot() == t.a.slot() && b.slot() == t.b.slot());
    CHECK(t.world.storage<Body>().slot_of(b) == t.world.storage<Body>().slot_of(b));
    CHECK(coins(t.world) == 2);
}

namespace {
Coins* g_coins = nullptr;
World* g_world = nullptr;
Entity g_extra{};
void one_coin_too_many() { g_extra = g_coins->spawn_coin(*g_world, {0, 100, 12, 12}, 1); }
} // namespace

NNH_TEST(coins_are_refused_past_their_capacity) {
    // The level's budget: a full Coin kind refuses the whole coin (no body left behind) and asserts (Debug/Development).
    Coins c;
    World w = static_cast<World&&>(level::create_world().value());
    for (int i = 0; i < level::kMaxCoins; ++i) CHECK(c.spawn_coin(w, {static_cast<float>(i * 16), 0, 12, 12}, 1));
    g_coins = &c;
    g_world = &w;
    CHECK(world_test::asserts_or_is_ignored(one_coin_too_many));
    CHECK(!g_extra && coins(w) == level::kMaxCoins && w.storage<Body>().count() == level::kMaxCoins);
    g_coins = nullptr;
    g_world = nullptr;
}

NNH_TEST(coins_clear_forgets_every_coin) {
    Two t;
    t.world.clear();
    CHECK(coins(t.world) == 0 && !t.world.alive(t.a));
}

NNH_TEST(coins_a_mask_without_coins_takes_nothing) {
    Two t;
    TakenCoin taken[4];
    CHECK(collect_coins(t.world, {105, 50, 20, 14}, layers::kWorld | layers::kPlayer, taken, 4) == 0);
    CHECK(coins(t.world) == 2);
}

NNH_TEST(coins_are_taken_in_ascending_slots_and_their_bodies_go_to_no_layers) {
    Coins c;
    World w = static_cast<World&&>(level::create_world().value());
    const Entity c0 = c.spawn_coin(w, {140, 50, 12, 12}, 1);
    const Entity c1 = c.spawn_coin(w, {100, 50, 12, 12}, 1);
    const Entity c2 = c.spawn_coin(w, {120, 50, 12, 12}, 1);
    TakenCoin taken[4] = {};
    CHECK(collect_coins(w, {95, 50, 60, 14}, kMask, taken, 4) == 3);
    CHECK(taken[0].entity == c0 && taken[1].entity == c1 && taken[2].entity == c2);
    CHECK(w.storage<Body>().data()[1].layers == nnh::collision::kNoLayers); // Body{} in the destroyed slot
}

NNH_TEST(coins_ignore_other_bodies_in_the_storage) {
    World w = static_cast<World&&>(level::create_world().value());
    const Entity player = w.spawn();
    w.add<Body>(player, {{100, 50, 12, 14}, layers::kPlayer});
    Coins coin;
    const Entity c = coin.spawn_coin(w, {100, 50, 12, 12}, 3);
    TakenCoin taken[4] = {};
    CHECK(collect_coins(w, {100, 50, 12, 14}, kMask, taken, 4) == 1);
    CHECK(taken[0].entity == c && w.alive(player));
}
