// A fresh world transform on demand (spec 022 FR-9; plan 022 "world_now"): the node's ancestors collected by a climb
// (≤ 16 slots on the stack), then the root rule and the compositions downward with the pass's inline code, reading
// each node's sine cache when its bits match: bit for bit what propagate() would store now. Stores nothing.

#include "hierarchy/node_compute.hpp"

namespace nnh {

WorldTransform2D Hierarchy::world_now(Entity e) const {
    const detail::Tree t = detail::tree_of(*this);
    if (!t.valid()) return {};
    std::uint16_t s = t.slot_of(e);
    if (s == detail::kNoNode) return {};
    std::uint16_t chain[kMaxHierarchyDepth];
    int n = 0;
    for (; s != detail::kNoNode && n < kMaxHierarchyDepth; s = detail::NA::parent(t.nodes[s])) chain[n++] = s;
    WorldTransform2D world;
    bool bad = false;
    detail::compute_world(t.nodes[chain[n - 1]], nullptr, false, world, bad);
    for (int i = n - 2; i >= 0; --i) {
        const WorldTransform2D parent = world;
        detail::compute_world(t.nodes[chain[i]], &parent, false, world, bad);
    }
    return world;
}

} // namespace nnh
