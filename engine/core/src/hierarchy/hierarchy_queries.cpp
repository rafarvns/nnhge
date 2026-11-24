// Hierarchy queries (spec 022 FR-7, FR-22): constant-time parent, depth and child count; root by a climb; children by
// links; the stored world transform as a load; stats from the node kind and the module words. An empty view, a stale
// handle or an entity without a node answers nothing, without asserting.

#include "hierarchy/hierarchy_internal.hpp"

namespace nnh {
namespace {

/// `e`'s node slot (kNoNode: empty view, stale, no node) and the view's tree.
std::uint16_t slot(const Hierarchy& h, Entity e, detail::Tree& t) {
    t = detail::tree_of(h);
    return t.valid() ? t.slot_of(e) : detail::kNoNode;
}

Entity entity_or_null(const detail::Tree& t, std::uint16_t s) { return s == detail::kNoNode ? Entity{} : t.entity_of(s); }

} // namespace

using detail::NA;

Entity Hierarchy::parent(Entity e) const {
    detail::Tree t;
    const std::uint16_t s = slot(*this, e, t);
    return s == detail::kNoNode ? Entity{} : entity_or_null(t, NA::parent(t.nodes[s]));
}

int Hierarchy::depth(Entity e) const {
    detail::Tree t;
    const std::uint16_t s = slot(*this, e, t);
    return s == detail::kNoNode ? -1 : NA::depth(t.nodes[s]);
}

Entity Hierarchy::root(Entity e) const {
    detail::Tree t;
    std::uint16_t s = slot(*this, e, t);
    if (s == detail::kNoNode) return {};
    while (NA::parent(t.nodes[s]) != detail::kNoNode) s = NA::parent(t.nodes[s]);
    return t.entity_of(s);
}

int Hierarchy::child_count(Entity e) const {
    detail::Tree t;
    const std::uint16_t s = slot(*this, e, t);
    return s == detail::kNoNode ? 0 : NA::children(t.nodes[s]);
}

Entity Hierarchy::first_child(Entity e) const {
    detail::Tree t;
    const std::uint16_t s = slot(*this, e, t);
    return s == detail::kNoNode ? Entity{} : entity_or_null(t, NA::first(t.nodes[s]));
}

Entity Hierarchy::next_sibling(Entity e) const {
    detail::Tree t;
    const std::uint16_t s = slot(*this, e, t);
    return s == detail::kNoNode ? Entity{} : entity_or_null(t, NA::next(t.nodes[s]));
}

detail::ChildRange Hierarchy::children(Entity e) const {
    detail::Tree t;
    const std::uint16_t s = slot(*this, e, t);
    if (s == detail::kNoNode) return {};
    return detail::ChildRange(t.w, t.kind, t.nodes, NA::first(t.nodes[s]));
}

bool Hierarchy::is_ancestor(Entity ancestor, Entity e) const {
    detail::Tree t;
    const std::uint16_t s = slot(*this, e, t);
    const std::uint16_t a = t.valid() ? t.slot_of(ancestor) : detail::kNoNode;
    if (s == detail::kNoNode || a == detail::kNoNode) return false;
    for (std::uint16_t p = NA::parent(t.nodes[s]); p != detail::kNoNode; p = NA::parent(t.nodes[p]))
        if (p == a) return true;
    return false;
}

const WorldTransform2D* Hierarchy::world_transform(Entity e) const {
    detail::Tree t;
    const std::uint16_t s = slot(*this, e, t);
    return s == detail::kNoNode ? nullptr : &t.nodes[s].world();
}

Transform2D* Hierarchy::local(Entity e) {
    detail::Tree t;
    const std::uint16_t s = slot(*this, e, t);
    return s == detail::kNoNode ? nullptr : &t.nodes[s].local;
}

Node* Hierarchy::node(Entity e) {
    detail::Tree t;
    const std::uint16_t s = slot(*this, e, t);
    return s == detail::kNoNode ? nullptr : &t.nodes[s];
}

HierarchyStats Hierarchy::stats() const {
    const detail::Tree t = detail::tree_of(*this);
    if (!t.valid()) return {};
    const KindStats k = nodes_.stats();
    const std::uint32_t* words = t.modules().words;
    HierarchyStats s;
    s.nodes = k.live;
    s.peak = k.peak;
    s.capacity = k.capacity;
    s.roots = static_cast<int>(words[detail::kRoots]);
    s.deepest = static_cast<int>(words[detail::kDeepest]);
    s.computed = static_cast<int>(words[detail::kComputed]);
    s.refusals = static_cast<int>(words[detail::kRefusals]);
    s.bad_locals = static_cast<int>(words[detail::kBadLocals]);
    return s;
}

} // namespace nnh
