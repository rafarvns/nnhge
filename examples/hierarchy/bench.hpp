#pragma once
// The hierarchy bench's workloads (spec 022 AC-7), pure: what bench_scene.cpp times on the console. Each run_*() is the
// measured call; the prepare_*() calls run outside the profile scope.
//   pass_turn  1024 nodes (256 hierarchies of 4: a root, two children, a grandchild) built by prefab in visiting
//              order; every root moved and turned and one child turned, then propagate()
//   pass_move  the same world, only positions changed (rotations and scales as at the last pass), then propagate()
//   world_now  256 chains of 4 nodes built by hand in a seeded shuffled order (records scatter); world_now of the 256
//              depth-3 nodes in a shuffled order
//   spawn      64 prefab hierarchies of 4 entities (a node and two other components each, one tag) spawned, then
//              destroyed by destroying their roots (cascades), one flush; the same by hand on alternate frames
//   reparent   16 rotated, scaled roots with a child each (32 parents) and 64 items: 256 keep-world then 256
//              keep-local reparents

#include <cstdint>

#include <nnh/hierarchy.hpp>
#include <nnh/prefabs.hpp>

class HierarchyBench {
public:
    static constexpr int kHierarchies = 256;
    static constexpr int kSpawned = 64;
    static constexpr int kReparents = 256;

    bool start(); ///< false: out of memory
    void prepare_turn();
    int run_pass();
    void prepare_move();
    int run_world_now();
    int run_spawn(); ///< by prefab
    int run_hand();  ///< the same by hand
    int run_reparent();

private:
    struct Payload {
        std::uint32_t a = 0, b = 0;
    };
    struct Extra {
        std::uint16_t v = 0;
    };
    static nnh::Result<nnh::World> make_world(int entities);

    nnh::World pass_world_, now_world_, spawn_world_, hand_world_, reparent_world_;
    nnh::Hierarchy pass_tree_, now_tree_, spawn_tree_, hand_tree_, reparent_tree_;
    nnh::PrefabLibrary lib_;
    nnh::Prefab tree_prefab_{}, tower_prefab_{};
    nnh::Entity roots_[kHierarchies] = {};
    nnh::Entity turned_[kHierarchies] = {};
    nnh::Entity deep_[kHierarchies] = {};
    nnh::Entity parents_[32] = {};
    nnh::Entity items_[64] = {};
    nnh::Entity spawned_[kSpawned] = {}; ///< run_spawn/run_hand's roots (a member: no array cleared per frame)
    std::uint32_t frame_ = 0;
};
