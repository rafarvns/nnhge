#pragma once
// The two sides of the self-check (spec 021 AC-2): Side<true> spawns through a prefab library (with variants, hooks,
// placement members and overrides), Side<false> writes the same entities by hand from the flattened table
// (check_kinds.hpp) with spawn() / add<T>(e, value) / add_tag, all or nothing by hand. The operations driving them
// are in self_check.cpp; both sides run the same hook code.

#include "check_kinds.hpp"

#include <cstdint>
#include <nnh/prefabs.hpp>

namespace check {

inline constexpr int kEntities = 1024;
inline constexpr int kLiveLimit = 700;

/// One copy's overrides: up to two kinds, their values in `v`.
struct Overrides {
    int kinds[2] = {};
    int count = 0;
    Values v{};
};

struct SideStats {
    int spawns = 0, misses = 0, hook_spawns = 0, suicides = 0, d_spawns = 0, visits = 0;
};

template <bool kPrefab>
class Side {
public:
    /// The world (and, for the prefab side, the library). False: out of memory or a refused registration.
    bool start();
    /// Prefab `def` with `overrides` (may be null) at `at` (may be null). Null when refused: a full capacity (a miss)
    /// or inside clear().
    nnh::Entity spawn(int def, const Overrides* overrides, const nnh::Vec2* at);
    nnh::World& world() { return world_; }
    nnh::PrefabLibrary& library() { return library_; }

    void mix(std::uint32_t word);
    std::uint32_t sum() const { return sum_; }
    /// FNV-1a of every live entity (handle, tags) and every kind's live slots (slot, owner, bytes).
    std::uint32_t state();

    /// Room bookkeeping, the same on both sides: slots used (live or quarantined) since the last flush.
    bool room(int kind) const { return used_[kind] < kCapacity[kind]; }
    void took(int kind) { ++used_[kind]; }
    void flushed();
    nnh::Entity handle(int index) const { return handles_[index % kEntities]; }
    void remember(nnh::Entity e) {
        if (e) handles_[e.slot()] = e;
    }

    void run_hook(int hook, nnh::World& w, nnh::Entity e, const nnh::SpawnInfo& info);
    void on_d_removed(nnh::World& w, std::uint32_t value);

    SideStats stats;

private:
    nnh::Entity spawn_by_hand(int def, const Overrides* overrides, const nnh::Vec2* at);
    nnh::Entity spawn_by_prefab(int def, const Overrides* overrides, const nnh::Vec2* at);
    bool register_prefabs();

    nnh::World world_;
    nnh::PrefabLibrary library_; ///< the prefab side's only
    nnh::Prefab prefabs_[kPrefabs] = {};
    nnh::Entity handles_[kEntities] = {};
    int used_[kKinds] = {};
    int used_entities_ = 0;
    std::uint32_t sum_ = 2166136261u;
};

extern template class Side<true>;
extern template class Side<false>;

} // namespace check
