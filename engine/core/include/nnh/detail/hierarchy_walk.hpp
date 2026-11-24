#pragma once
/// @file nnh/detail/hierarchy_walk.hpp
/// What <nnh/hierarchy.hpp>'s templates and ranges need (spec 022, D37): the link sentinel, the engine's node remove
/// hook and the descendant walk behind Hierarchy::each_descendant (its steps are out of line). Engine-internal: use
/// <nnh/hierarchy.hpp>.

#include <cstdint>

#include "nnh/detail/world_layout.hpp"

namespace nnh {

class Hierarchy;
class Node;

namespace detail {

/// A link to no node (links are node slots in the node kind's storage).
inline constexpr std::uint16_t kNoNode = 0xFFFFu;
inline constexpr int kWalkLevels = 17; ///< a subtree's root and 16 levels below it

/// Engine code and tests: a node's private fields.
struct NodeAccess;

/// The engine's node remove hook (spec 022 FR-11): unlinks the node; a dying entity's children are destroyed through the
/// world's queue (a cascade), a removed node's children become roots keeping their stored worlds.
void node_removed(World& world, Entity e, Node& node);

/// Hierarchy::each_descendant's walk: pre-order, children in attach order, with spec 020's iteration rules (an
/// iteration guard: what is created during it is not visited) and cursor fix-ups when a node is unlinked or appended
/// (src/hierarchy/descendant_walk.cpp). Lives on the caller's stack, linked into the world's module state while active.
class DescendantWalk {
public:
    DescendantWalk(Hierarchy& hierarchy, Entity root);
    ~DescendantWalk();
    DescendantWalk(const DescendantWalk&) = delete;
    DescendantWalk& operator=(const DescendantWalk&) = delete;
    /// The next descendant, or null when the walk is over.
    Entity next();

    // Read by the link code's fix-ups (node_links.cpp).
    WorldHot* hot = nullptr;
    int kind = 0;
    DescendantWalk* outer = nullptr; ///< the walk started before this one (nested walks), or null
    Entity path[kWalkLevels] = {};   ///< path[0] = the root, path[level] = the node last visited
    std::uint16_t pending[kWalkLevels] = {}; ///< per level ≥ 1: the next sibling to visit there (kNoNode: none)
    int level = -1; ///< −1: over (or never started); 0: only the root so far

private:
    IterationGuard guard_; ///< after `hot` (initialised from it)
};

} // namespace detail
} // namespace nnh
