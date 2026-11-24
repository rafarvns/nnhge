// Hierarchy::each_descendant's walk (spec 022 FR-7; plan 022 "Walks"): pre-order, children in attach order, holding the
// world's iteration guard (nodes created during the walk are fresh: skipped) and linked into the world's module state,
// so the link code moves a remembered next sibling on when it is unlinked and gives a level with none left an appended
// child (node_links.cpp). Before each step the path is checked from the top: a node destroyed or moved out of the
// walked subtree is not descended into, and the walk continues with what was remembered at its level.

#include "hierarchy/hierarchy_internal.hpp"

namespace nnh::detail {
namespace {

Tree tree_from(WorldHot* hot, int kind) {
    Tree t;
    if (hot == nullptr) return t;
    t.w = hot;
    t.kind = kind;
    t.nodes = static_cast<Node*>(hot->kinds[kind].data);
    return t;
}

bool fresh(const Tree& t, std::uint16_t slot) {
    return (t.w->pending & kPendingFresh) != 0 && bit_set(t.w->kinds[t.kind].slots.fresh, slot);
}

/// The first of `slot` and its next siblings that wasn't created during the walk, or kNoNode.
std::uint16_t skip_fresh(const Tree& t, std::uint16_t slot) {
    while (slot != kNoNode && fresh(t, slot)) slot = NA::next(t.nodes[slot]);
    return slot;
}

} // namespace

DescendantWalk::DescendantWalk(Hierarchy& hierarchy, Entity root)
    : hot(tree_of(hierarchy).w), kind(NA::kind_of(hierarchy)), guard_(hot) {
    const Tree t = tree_of(hierarchy);
    if (!t.valid() || t.slot_of(root) == kNoNode) return;
    path[0] = root;
    level = 0;
    WorldModuleState& m = t.modules();
    outer = static_cast<DescendantWalk*>(m.walks);
    m.walks = this;
}

DescendantWalk::~DescendantWalk() {
    if (hot == nullptr || path[0] == Entity{}) return;
    WorldModuleState& m = hot->cold->modules;
    if (m.walks == this) m.walks = outer; // walks end in reverse order of their start (nested on the stack)
}

Entity DescendantWalk::next() {
    if (level < 0) return {};
    const Tree t = tree_from(hot, kind);
    // The valid prefix of the path: each node alive, with a node, under the previous one.
    std::uint16_t slots[kWalkLevels];
    slots[0] = t.slot_of(path[0]);
    if (slots[0] == kNoNode) {
        level = -1;
        return {};
    }
    int valid = 0;
    while (valid < level) {
        const std::uint16_t s = t.slot_of(path[valid + 1]);
        if (s == kNoNode || NA::parent(t.nodes[s]) != slots[valid]) break;
        slots[++valid] = s;
    }
    if (valid == level) { // the last visited node is in place: its children first
        const std::uint16_t c = skip_fresh(t, NA::first(t.nodes[slots[level]]));
        if (c != kNoNode && level + 1 < kWalkLevels) {
            ++level;
            path[level] = t.entity_of(c);
            pending[level] = NA::next(t.nodes[c]);
            return path[level];
        }
    } else {
        level = valid + 1; // destroyed or moved out: not descended into
    }
    for (; level > 0; --level) { // the remembered next sibling at this level, climbing when none is left
        const std::uint16_t c = skip_fresh(t, pending[level]);
        if (c == kNoNode) continue;
        path[level] = t.entity_of(c);
        pending[level] = NA::next(t.nodes[c]);
        return path[level];
    }
    level = -1;
    return {};
}

} // namespace nnh::detail
