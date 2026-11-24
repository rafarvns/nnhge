#pragma once
// The level's prefabs (spec 021 AC-4): the things a level spawns, declared once. `coin` (a Body on layers::kCoin placed
// by its box — a rectangle object's bounds — and a Coin whose value the hook reads from the object's int property
// `value`, 1 when absent; tag "coin"), `turret` (a Body on layers::kEnemy placed by its box and a Turret; tag
// "turret"; spec 022: a Node at the box's centre, set by its hook, and a `barrel` child — a Node and a Barrel, tag
// "barrel" — the turret's turning gun), and `spark` (a Spark placed at its position and a Velocity; tag "fx"; its hook
// starts the fade that destroys it). The level's map objects name them by type (LevelPlay calls spawn_map_objects); doors and the spawn
// point are the level's own (skipped types). Each spawn is bit for bit what hand-written spawns make
// (tests/platformer/test_level_prefabs.cpp); the barrels changed the replay's checksums (spec 022 T6.3).

#include "level_world.hpp"

#include <nnh/prefabs.hpp>
#include <nnh/result.hpp>
#include <nnh/tween.hpp>

namespace level {

inline constexpr int kSparkFadeFrames = 24; ///< a sparkle fades from 1 to 0 over this many frames, then goes

struct LevelPrefabs {
    nnh::PrefabLibrary library;
    nnh::Prefab coin{};
    nnh::Prefab turret{};
    nnh::Prefab spark{};
    nnh::Prefab barrel{};
};

/// The level's library (4 prefabs, 9 entries, 1 child entry). `tweens` fades the sparks (with null, a spark keeps alpha 1 and
/// lives on: tests). Fails only when out of memory.
nnh::Result<LevelPrefabs> create_level_prefabs(nnh::TweenSet* tweens);

} // namespace level
