#pragma once
// The self-check's two sides (spec 022 AC-2): the same world description, hooks and operations; Side<true> spawns
// through the prefab library, Side<false> by the hand-written table (spawn, add, add_tag, attach keep-local, then the
// hooks in post-order), checking room first from its own counters of live and quarantined slots.

#include <cstdint>

#include "self_check_defs.hpp"

namespace hcheck {

struct SideStats {
    int spawns = 0, misses = 0, hook_spawns = 0, d_hooks = 0, cascades = 0, refusals = 0;
};

template <bool kPrefab>
class Side {
public:
    Side() = default;
    Side(const Side&) = delete;
    Side& operator=(const Side&) = delete;

    /// The world, its hierarchy and (prefab side) the library. False: out of memory.
    bool start();
    nnh::World& world() { return world_; }
    nnh::Hierarchy& tree() { return tree_; }

    /// Def `def` as a root (at `at` when given) or under `parent`; null when refused (counted in stats.misses).
    nnh::Entity spawn(int def, const nnh::Vec2* at, nnh::Entity parent);
    /// After a flush or a clear: nothing is quarantined.
    void flushed();
    /// A hash of the whole visible state: live handles, components, tags, and per node its links and transforms.
    std::uint32_t state();

    nnh::Entity handle(std::uint32_t i) const { return remembered_[i & (kRemembered - 1)]; }
    void remember(nnh::Entity e);
    void mix(std::uint32_t word);
    std::uint32_t sum() const { return sum_; }
    int misses() const;
    SideStats stats{};

    static void run_hook(void* side, int hook, nnh::World& world, nnh::Entity e);

private:
    static constexpr int kRemembered = 512;
    bool room(const HandDef& d, nnh::Entity parent);
    nnh::Entity by_hand(const HandDef& d, const nnh::Vec2* at, nnh::Entity parent);
    void d_hook(nnh::World& w, D& d);

    nnh::World world_;
    nnh::Hierarchy tree_;
    nnh::PrefabLibrary lib_;
    nnh::Prefab prefabs_[kPrefabs] = {};
    HookTarget target_{};
    int quarantined_[kKinds] = {};
    int quarantined_entities_ = 0;
    nnh::Entity remembered_[kRemembered] = {};
    std::uint32_t next_ = 0;
    std::uint32_t d_calls_ = 0;
    std::uint32_t sum_ = 2166136261u;
};

extern template class Side<true>;
extern template class Side<false>;

} // namespace hcheck
