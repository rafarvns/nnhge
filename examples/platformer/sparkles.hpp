#pragma once
// A taken coin bursts into four sparkles (spec 020 AC-4 step 2), pure (the level's world and its prefabs: tested in
// tests/platformer/test_sparkles.cpp). Each sparkle is the level's `spark` prefab (level_prefabs.hpp, spec 021): an
// entity tagged "fx" with a level::Spark placed at the coin's centre and a level::Velocity given per copy (short binary
// fractions: exact sums). The prefab's hook fades its alpha from 1 to 0 over 24 frames with the level's tween set
// through a setter that captures the world and the entity's handle, so it writes only while the sparkle lives; the
// fade's on_finish destroys the sparkle (a destroy from inside the set's update), and Spark's remove hook kills the
// fade when a sparkle goes first (a restart). Sparkles have no Body: no collision query ever sees them.

#include "level_prefabs.hpp"
#include "level_world.hpp"

class Sparkles {
  public:
    static constexpr int kPerCoin = 4;
    static constexpr int kFadeFrames = level::kSparkFadeFrames;

    /// `world` (from level::create_world(&tweens)) and `prefabs` (level::create_level_prefabs(&tweens)) must
    /// outlive it.
    Sparkles(nnh::World& world, level::LevelPrefabs& prefabs) : world_(world), prefabs_(prefabs) {}

    /// Four sparkles at `at` (a coin's centre).
    void burst(nnh::Vec2 at);
    /// One frame: every sparkle moves by its velocity.
    void update();

  private:
    nnh::World& world_;
    level::LevelPrefabs& prefabs_;
};
