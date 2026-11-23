#pragma once
// The entities bench's workloads (spec 020 AC-7), pure (no clock, no drawing): bench_scene.cpp times them. Each run_*
// returns a value folded from what it touched, so the work can't be optimised away.
//   each     world.each<Body, Velocity> over 1024 entities having both: box += velocity     (target < 0.5 ms)
//   spawn    256 × (spawn, 3 adds, 2 tags), then 256 destroys and one flush                   (target < 1 ms)
//   find     find() of one tag over 1024 entities with 4 tags each                            (target < 0.1 ms)
//   get      10 000 world.get<Body>(handle) over seeded handles of the 1024                   (target < 0.5 ms)
//   storage  the same lookups through Storage<Body>::get (kind lookup hoisted; information)
//   pool     1000 acquire/release pairs on a Pool<Item, 1024>, then a flush                   (target < 0.3 ms)

#include "components.hpp"

#include <nnh/entities.hpp>
#include <nnh/pool.hpp>

class EntitiesBench {
  public:
    static constexpr int kEntities = 1024;
    static constexpr int kSpawns = 256;
    static constexpr int kLookups = 10000;
    static constexpr int kPairs = 1000;

    struct Item {
        std::int32_t a = 0, b = 0;
    };

    /// Creates the worlds and fills them (false: out of memory).
    bool start();

    int run_each();
    int run_spawns();
    int run_find();
    int run_get();
    int run_get_storage();
    int run_pool();

  private:
    nnh::World moving_;  ///< Body + Velocity on all 1024
    nnh::World spawner_; ///< Body, Velocity, Coin; 256 entities, 2 tags
    nnh::World tagged_;  ///< 1024 entities with 4 tags each
    nnh::Entity handles_[kLookups] = {};
    nnh::Entity spawned_[kSpawns] = {};
    nnh::Entity found_[kEntities] = {};
    nnh::Pool<Item, kEntities> pool_;
};
