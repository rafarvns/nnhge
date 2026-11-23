#pragma once
// The showcase (spec 020 AC-5): the arena on the top screen; the bottom screen shows it through a hand-set Camera2D
// at (40, 0), so a touch at (x, y) is the world point (40 + x, y). Controls: Circle Pad / D-Pad move the ship, A fires
// (holding it past the pool's 32 bullets counts misses: "pool: full, misses N" when released), B zaps the missile's
// target (the missile then finds its handle stale: "target lost (stale handle slot n gen g)"), X cycles the tag
// filter none → enemy → flying → coin ("filter: <tag> found N", find() outlined), L freezes the filtered group
// (each_tagged: "freeze: <tag> N"), R unfreezes ("unfreeze: N"), a touch destroys what is under it
// ("destroyed: slot n gen g tags ..."), SELECT the self-check, Y the bench, START quits.

#include "arena.hpp"
#include "demos.hpp"

class ShowcaseScene final : public nnh::Scene {
  public:
    explicit ShowcaseScene(const Demos& demos) : demos_(demos), arena_(*this) {}

  private:
    void on_enter() override;
    void update() override;
    void draw() override;
    void cycle_filter();
    void refresh_filter();

    const Demos& demos_;
    arena::Arena arena_;
    int filter_ = 0; ///< index in the filter cycle (0: none)
    nnh::Entity found_[arena::kMaxEntities] = {};
    int found_count_ = 0;
};
