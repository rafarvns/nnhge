#pragma once
// The prefabs bench's workloads (spec 021 AC-7), pure (bench_scene.cpp times them):
//   spawn      256 spawns of a prefab with 3 components (one a 64-byte struct) and 2 tags, 256 destroys, one flush;
//   hand       the same workload written by hand with spec 020's calls (the ratio's reference);
//   overrides  the same 256 spawns with a component override and a position each;
//   map        512 map objects (two prefabs, a hook reading one property each) spawned into an empty world;
//   find       1000 finds by run-time name over a library of 128 prefabs.

#include <cstdint>
#include <nnh/entities.hpp>
#include <nnh/math.hpp>
#include <nnh/prefabs.hpp>
#include <nnh/tilemap.hpp>

class PrefabsBench {
  public:
    static constexpr int kSpawns = 256;
    static constexpr int kMapObjects = 512;
    static constexpr int kLibrary = 128;
    static constexpr int kFinds = 1000;

    struct Body {
        nnh::Rect box{};
        std::uint32_t layers = 0;
    };
    struct Big {
        std::int32_t v[16] = {};
    };
    struct Velocity {
        nnh::Vec2 v{};
    };
    struct Coin {
        std::int32_t value = 1;
    };

    /// Worlds, libraries and the map (false: out of memory).
    bool start();
    int run_spawn();
    int run_hand();
    int run_overrides();
    /// Untimed: empties the map workload's world.
    void reset_map();
    int run_map();
    int run_find();

  private:
    bool make_map();

    nnh::World spawn_world_, hand_world_, map_world_;
    nnh::PrefabLibrary library_, map_library_, find_library_;
    nnh::Prefab prefab_{};
    nnh::Tilemap map_;
    nnh::Entity spawned_[kSpawns] = {};
    char names_[kLibrary][12] = {};
    std::uint8_t order_[kFinds] = {};
};
