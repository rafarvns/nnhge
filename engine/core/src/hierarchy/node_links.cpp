// Links between nodes (spec 022 FR-6, FR-7; plan 022 D37): node slots, never handles. Children keep attach order:
// parent, first child, next and previous sibling, the first child's previous being the last child, so appending and
// unlinking are O(1). Subtree walks are stackless (parent links). Every unlink and append tells the active descendant
// walks of the world (their remembered next siblings), so a walk never steps onto a node that left its part.

#include "hierarchy/hierarchy_internal.hpp"

namespace nnh::detail {
namespace {

DescendantWalk* first_walk(const Tree& t) { return static_cast<DescendantWalk*>(t.modules().walks); }

/// `child` is leaving its parent: a walk remembering it as the next sibling to visit moves on to its next one.
void tell_unlinked(const Tree& t, std::uint16_t child, std::uint16_t next) {
    for (DescendantWalk* walk = first_walk(t); walk != nullptr; walk = walk->outer)
        for (int l = 1; l <= walk->level; ++l)
            if (walk->pending[l] == child) walk->pending[l] = next;
}

/// `child` was appended to `parent`: a walk that has no next sibling left under `parent` gets it (the part not yet
/// walked; nodes created during the walk are skipped when reached).
void tell_appended(const Tree& t, std::uint16_t parent, std::uint16_t child) {
    DescendantWalk* walk = first_walk(t);
    if (walk == nullptr) return;
    const Entity p = t.entity_of(parent);
    for (; walk != nullptr; walk = walk->outer)
        for (int l = 1; l <= walk->level; ++l)
            if (walk->pending[l] == kNoNode && walk->path[l - 1] == p) walk->pending[l] = child;
}

} // namespace

void link_last(const Tree& t, std::uint16_t parent, std::uint16_t child) {
    Node& p = t.nodes[parent];
    Node& c = t.nodes[child];
    NA::parent(c) = parent;
    NA::next(c) = kNoNode;
    const std::uint16_t first = NA::first(p);
    if (first == kNoNode) {
        NA::first(p) = child;
        NA::prev(c) = child; // the only child is its own last
    } else {
        const std::uint16_t last = NA::prev(t.nodes[first]);
        NA::next(t.nodes[last]) = child;
        NA::prev(c) = last;
        NA::prev(t.nodes[first]) = child;
    }
    ++NA::children(p);
    tell_appended(t, parent, child);
}

void unlink(const Tree& t, std::uint16_t child) {
    Node& c = t.nodes[child];
    const std::uint16_t parent = NA::parent(c);
    if (parent == kNoNode) return;
    Node& p = t.nodes[parent];
    const std::uint16_t next = NA::next(c), prev = NA::prev(c);
    if (NA::first(p) == child) {
        NA::first(p) = next;
        if (next != kNoNode) NA::prev(t.nodes[next]) = prev; // the last child, still
    } else {
        NA::next(t.nodes[prev]) = next;
        NA::prev(t.nodes[next != kNoNode ? next : NA::first(p)]) = prev; // was the last: the first's prev
    }
    --NA::children(p);
    NA::parent(c) = kNoNode;
    NA::next(c) = kNoNode;
    NA::prev(c) = kNoNode;
    tell_unlinked(t, child, next);
}

int subtree_height(const Tree& t, std::uint16_t root) {
    const int base = NA::depth(t.nodes[root]);
    int deepest = base;
    for (std::uint16_t n = next_in_subtree(t, root, root); n != kNoNode; n = next_in_subtree(t, root, n)) {
        const int d = NA::depth(t.nodes[n]);
        deepest = d > deepest ? d : deepest;
    }
    return deepest - base;
}

void rewrite_depths(const Tree& t, std::uint16_t root) {
    const std::uint16_t parent = NA::parent(t.nodes[root]);
    NA::depth(t.nodes[root]) = static_cast<std::uint8_t>(parent == kNoNode ? 0 : NA::depth(t.nodes[parent]) + 1);
    for (std::uint16_t n = next_in_subtree(t, root, root); n != kNoNode; n = next_in_subtree(t, root, n))
        NA::depth(t.nodes[n]) = static_cast<std::uint8_t>(NA::depth(t.nodes[NA::parent(t.nodes[n])]) + 1);
}

} // namespace nnh::detail
