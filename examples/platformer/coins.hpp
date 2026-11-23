#pragma once
// The coins of a level (spec 020 AC-4 step 1; before: the coin book): entities with a Body on layers::kCoin and a
// Coin value, tagged "coin". Taking them is one collision::boxes_overlapping over the world's whole body storage with
// the player's mask: the player's and the turrets' bodies are on other layers, free slots on none, so only coins pass.
// A taken coin's entity is destroyed; a level restart is World::clear() plus the same spawns (tested in
// tests/platformer/test_coins.cpp).

#include "level_world.hpp"

/// Spawns a coin. Null when the level already has level::kMaxCoins coins.
nnh::Entity spawn_coin(nnh::World& world, nnh::Rect bounds, int value);

/// A coin taken by collect_coins().
struct TakenCoin {
    nnh::Entity entity{};
    int value = 0;
};

/// Takes every coin `box` overlaps (shared area > 0: touching edges don't count) when `mask` detects coins, in
/// ascending body slot (= spawn order). Writes up to `capacity` of them into `taken` and returns how many were taken
/// (all of them are, also past `capacity`).
int collect_coins(nnh::World& world, nnh::Rect box, nnh::collision::Layers mask, TakenCoin* taken, int capacity);
