// The engine's node remove hook (spec 022 FR-11; plan 022 "Cascades and removal"), run after the game's: the node
// leaves its parent; then, if its entity is being destroyed, each child is unlinked and destroyed — queued, since hooks
// are running, so a cascade is breadth-first in child order with no recursion, every descendant's hooks running with
// its data intact and its parent already stale; if only the node component is removed, each child becomes a root
// that keeps its stored world (its local set from it) and its subtree's depths are rewritten.

#include "hierarchy/hierarchy_internal.hpp"

namespace nnh::detail {

void node_removed(World& world, Entity e, Node& node) {
    const Tree t = tree_of(world);
    if (!t.valid()) return;
    const auto slot = static_cast<std::uint16_t>(&node - t.nodes);
    unlink(t, slot);
    const bool dying = world_dying(world, e);
    for (std::uint16_t c = NA::first(node); c != kNoNode;) {
        const std::uint16_t next = NA::next(t.nodes[c]);
        unlink(t, c);
        if (dying) {
            world.destroy(t.entity_of(c)); // queued: the cascade
        } else {
            Node& child = t.nodes[c];
            const WorldTransform2D& w = child.world();
            child.local = {w.map.origin, w.rotation, w.scale};
            rewrite_depths(t, c);
        }
        c = next;
    }
}

} // namespace nnh::detail
