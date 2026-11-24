// The world-transform pass (spec 022 FR-8, FR-21; plan 022 "The pass" and "ARM11 cost plan"): roots in ascending node
// slots, each subtree depth-first in child order through parent links (no stack), each node once from its parent's
// fresh world and its own current local. Only the node array is walked (no entity record, no handle lookup), the
// composition and the sine are inline, the next records are prefetched, and no game code is called. Stats once at
// the end.

#include "hierarchy/node_compute.hpp"

namespace nnh {
namespace detail {
namespace {

/// Two nodes ahead (a prefab-built hierarchy sits in consecutive slots in visiting order): the ARM11 has no hardware
/// prefetcher. Integer arithmetic: an address past the array is a hint PLD never faults on.
NNH_DET_INLINE void prefetch_ahead(const Node* node) {
#if defined(__GNUC__) || defined(__clang__)
    const std::uintptr_t at = reinterpret_cast<std::uintptr_t>(node) + 2u * sizeof(Node);
    __builtin_prefetch(reinterpret_cast<const void*>(at));
    __builtin_prefetch(reinterpret_cast<const void*>(at + 48u));
#else
    (void)node;
#endif
}

struct PassCounts {
    int computed = 0, roots = 0, deepest = 0, bad = 0;
};

/// `root`'s subtree, depth-first in child order (the root computed first).
NNH_DET_INLINE void walk_subtree(Node* nodes, std::uint16_t root, PassCounts& c) {
    bool bad = false;
    compute_world(nodes[root], nullptr, true, NA::world(nodes[root]), bad);
    c.bad += bad ? 1 : 0;
    ++c.computed;
    std::uint16_t n = root;
    for (;;) {
        std::uint16_t next = NA::first(nodes[n]);
        if (next == kNoNode) {
            while (n != root && NA::next(nodes[n]) == kNoNode) n = NA::parent(nodes[n]);
            if (n == root) return;
            next = NA::next(nodes[n]);
        }
        n = next;
        Node& node = nodes[n];
        prefetch_ahead(&node);
        bad = false;
        compute_world(node, &NA::world(nodes[NA::parent(node)]), true, NA::world(node), bad);
        c.bad += bad ? 1 : 0;
        ++c.computed;
        const int d = NA::depth(node);
        c.deepest = d > c.deepest ? d : c.deepest;
    }
}

} // namespace
} // namespace detail

int Hierarchy::propagate() {
    const detail::Tree t = detail::tree_of(*this);
    if (!t.valid()) return 0;
    if (detail::world_busy(*t.w)) {
        detail::hierarchy_misuse("Hierarchy::propagate() inside an iteration of its world or a remove hook (ignored)");
        return 0;
    }
    const detail::SlotTable& slots = t.w->kinds[t.kind].slots;
    Node* const nodes = t.nodes;
    detail::PassCounts c;
    const int words = detail::words_for(slots.extent);
    for (int w = 0; w < words; ++w) {
        for (std::uint32_t live = slots.live[w]; live != 0; live &= live - 1) {
            const auto slot = static_cast<std::uint16_t>((w << 5) + detail::lowest_bit(live));
            if (detail::NA::parent(nodes[slot]) != detail::kNoNode) continue;
            ++c.roots;
            detail::walk_subtree(nodes, slot, c);
        }
    }
    std::uint32_t* words_out = t.modules().words;
    words_out[detail::kRoots] = static_cast<std::uint32_t>(c.roots);
    words_out[detail::kDeepest] = static_cast<std::uint32_t>(c.deepest);
    words_out[detail::kComputed] = static_cast<std::uint32_t>(c.computed);
    if (c.bad > 0) {
        words_out[detail::kBadLocals] += static_cast<std::uint32_t>(c.bad);
        detail::hierarchy_misuse("Hierarchy::propagate(): a non-finite local transform (treated as the identity)");
    }
    return c.computed;
}

} // namespace nnh
