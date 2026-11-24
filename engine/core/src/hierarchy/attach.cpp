// Attaching and detaching (spec 022 FR-6, FR-10; plan 022 "Attach"): the refusals in a fixed order (stale or dying,
// no node, cycle, too deep, not invertible), attaching to the current parent as a no-op, then unlink, append, depths
// and the transform — keep-world from the stored worlds (no sine, no chain walk), keep-local by composition.

#include "hierarchy/hierarchy_internal.hpp"

namespace nnh {
namespace detail {
namespace {

const char* reason(AttachCheck why) {
    switch (why) {
    case AttachCheck::Stale:
        return "Hierarchy::attach(): refused (a null, stale or dying entity)";
    case AttachCheck::NoNode:
        return "Hierarchy::attach(): refused (an entity without a node)";
    case AttachCheck::Cycle:
        return "Hierarchy::attach(): refused (the child is the parent or one of its ancestors)";
    case AttachCheck::TooDeep:
        return "Hierarchy::attach(): refused (deeper than kMaxHierarchyDepth)";
    default:
        return "Hierarchy::attach(): refused (keep-world under a parent that can't be inverted)";
    }
}

bool usable(const Tree& t, Entity e) {
    return live_record(*t.w, e) != nullptr && !bit_set(t.w->cold->dying, e.slot());
}

/// The checks in the plan's order; `same` is set when `child` is already `parent`'s child (nothing to do).
AttachCheck check(const Tree& t, Entity child, Entity parent, Reparent mode, std::uint16_t& cs, std::uint16_t& ps,
                  bool& same) {
    same = false;
    if (!usable(t, child) || !usable(t, parent)) return AttachCheck::Stale;
    cs = t.slot_of(child);
    ps = t.slot_of(parent);
    if (cs == kNoNode || ps == kNoNode) return AttachCheck::NoNode;
    if (NA::parent(t.nodes[cs]) == ps) {
        same = true;
        return AttachCheck::Ok;
    }
    for (std::uint16_t p = ps; p != kNoNode; p = NA::parent(t.nodes[p]))
        if (p == cs) return AttachCheck::Cycle;
    if (NA::depth(t.nodes[ps]) + 1 + subtree_height(t, cs) >= kMaxHierarchyDepth) return AttachCheck::TooDeep;
    if (mode == Reparent::KeepWorld) {
        Affine2D inv;
        if (!inverse(NA::world(t.nodes[ps]).map, inv)) return AttachCheck::NotInvertible;
    }
    return AttachCheck::Ok;
}

/// The local or the stored world after `node` moved under `parent` (or became a root: `parent` null).
void reparent_transform(Node& node, const Node* parent, Reparent mode) {
    WorldTransform2D& world = NA::world(node);
    if (mode == Reparent::KeepWorld) {
        if (parent != nullptr) {
            relative(parent->world(), world, node.local); // checked invertible
        } else {
            node.local = {world.map.origin, world.rotation, world.scale};
        }
        return;
    }
    const SinCos sc = cached_sin_cos(node);
    if (parent != nullptr) {
        child_world(parent->world(), node.local, sc, world);
    } else {
        root_world(node.local, sc, world);
    }
}

} // namespace
} // namespace detail

AttachCheck Hierarchy::can_attach(Entity child, Entity parent, Reparent mode) const {
    const detail::Tree t = detail::tree_of(*this);
    if (!t.valid()) return AttachCheck::NoNode;
    std::uint16_t cs = 0, ps = 0;
    bool same = false;
    return detail::check(t, child, parent, mode, cs, ps, same);
}

bool Hierarchy::attach(Entity child, Entity parent, Reparent mode) {
    const detail::Tree t = detail::tree_of(*this);
    if (!t.valid()) {
        detail::hierarchy_misuse("Hierarchy::attach() on an empty view");
        return false;
    }
    std::uint16_t cs = 0, ps = 0;
    bool same = false;
    const AttachCheck why = detail::check(t, child, parent, mode, cs, ps, same);
    if (why != AttachCheck::Ok) {
        ++t.modules().words[detail::kRefusals];
        detail::hierarchy_misuse(detail::reason(why));
        return false;
    }
    if (same) return true;
    Node& c = t.nodes[cs];
    if (mode == Reparent::KeepWorld) detail::reparent_transform(c, &t.nodes[ps], mode); // before the links change
    detail::unlink(t, cs);
    detail::link_last(t, ps, cs);
    detail::rewrite_depths(t, cs);
    if (mode == Reparent::KeepLocal) detail::reparent_transform(c, &t.nodes[ps], mode);
    return true;
}

bool Hierarchy::detach(Entity child, Reparent mode) {
    const detail::Tree t = detail::tree_of(*this);
    const std::uint16_t cs = t.valid() && detail::live_record(*t.w, child) != nullptr ? t.slot_of(child) : detail::kNoNode;
    if (cs == detail::kNoNode) {
        detail::hierarchy_misuse("Hierarchy::detach(): a null or stale entity, or one without a node");
        return false;
    }
    Node& c = t.nodes[cs];
    if (detail::NA::parent(c) == detail::kNoNode) return true;
    detail::unlink(t, cs);
    detail::rewrite_depths(t, cs);
    detail::reparent_transform(c, nullptr, mode);
    return true;
}

int Hierarchy::detach_children(Entity e, Reparent mode) {
    const detail::Tree t = detail::tree_of(*this);
    const std::uint16_t s = t.valid() && detail::live_record(*t.w, e) != nullptr ? t.slot_of(e) : detail::kNoNode;
    if (s == detail::kNoNode) {
        detail::hierarchy_misuse("Hierarchy::detach_children(): a null or stale entity, or one without a node");
        return 0;
    }
    int count = 0;
    for (std::uint16_t c = detail::NA::first(t.nodes[s]); c != detail::kNoNode; ++count) {
        const std::uint16_t next = detail::NA::next(t.nodes[c]);
        detail::unlink(t, c);
        detail::rewrite_depths(t, c);
        detail::reparent_transform(t.nodes[c], nullptr, mode);
        c = next;
    }
    return count;
}

} // namespace nnh
