#pragma once
// The self-check's 12 prefabs twice (spec 022 AC-2): registered in a library (register_prefabs) and written out
// flattened by hand (hand_def) — creation order, values, tags, parent indices, entry locals, hooks — by code that never
// reads the library. The script spawns the first through the library and the second by spawn/add/add_tag/attach.

#include "check_kinds.hpp"
#include "nnh/prefabs.hpp"

namespace hcheck {

/// One entity of a hand-written tree.
struct HandEntity {
    int parent = -1;             ///< index in the tree, −1 for the root
    nnh::Transform2D local{};    ///< its node's local (the root's position replaced by a spawn's position)
    bool has[kKinds] = {};       ///< which kinds it has (kNode first)
    A a{};
    B b{};
    C c{};
    D d{};
    K k{};
    nnh::Tag tags[2] = {};
    int tag_count = 0;
    int hook = kNoHook;
};

/// A prefab flattened: its entities in creation order (parent first, children depth-first in entry order).
struct HandDef {
    int count = 0;
    HandEntity e[32];
    bool placed = false; ///< the root's node local takes a spawn's position
    int height = 0;      ///< levels below the root
    int need[kKinds] = {}; ///< components of each kind
};

/// The hand-written table (built once).
const HandDef& hand_def(int def);

/// Where the prefab side's hooks go (the hand side calls the same function directly): a function and its side.
struct HookTarget {
    void (*run)(void* side, int hook, nnh::World& world, nnh::Entity e) = nullptr;
    void* side = nullptr;
};

/// Registers the 12 prefabs in `lib` (in Def order); hooks call `target`. False if a registration failed.
bool register_prefabs(nnh::PrefabLibrary& lib, nnh::Prefab* out, const HookTarget* target);

} // namespace hcheck
