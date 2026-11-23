#pragma once
// A taken coin bursts into four sparkles (spec 020 AC-4 step 2), pure (the level's world and a tween set: tested in
// tests/platformer/test_sparkles.cpp). Each sparkle is an entity tagged "fx" with a level::Spark and a level::Velocity
// (short binary fractions: exact sums). Its alpha is faded from 1 to 0 over 24 frames by the tween set through a
// setter that captures the world and the entity's handle, so it writes only while the sparkle lives; the fade's
// on_finish destroys the sparkle (a destroy from inside the set's update), and Spark's remove hook kills the fade
// when a sparkle goes first (a restart). Sparkles have no Body: no collision query ever sees them.

#include "level_world.hpp"

#include <nnh/tween.hpp>

class Sparkles {
  public:
    static constexpr int kPerCoin = 4;
    static constexpr int kFadeFrames = 24;

    /// `world` (from level::create_world(&tweens)) and `tweens` must outlive it.
    Sparkles(nnh::World& world, nnh::TweenSet& tweens) : world_(world), tweens_(tweens) {}

    /// Four sparkles at `at` (a coin's centre).
    void burst(nnh::Vec2 at);
    /// One frame: every sparkle moves by its velocity.
    void update();

  private:
    nnh::World& world_;
    nnh::TweenSet& tweens_;
};
