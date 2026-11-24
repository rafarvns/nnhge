#pragma once
// Hierarchies (spec 022, D37): the node's private fields, the view every source works through (a world's node array
// and its record layout), the link helpers (node_links.cpp) and the module words (stats, active walks) kept in the
// world's cold part.

#include <cstdint>

#include "entities/entities_internal.hpp"
#include "entities/world_internal.hpp"
#include "nnh/hierarchy.hpp"
#include "nnh/log.hpp"
#include "transform/compose.hpp"

NNH_LOG_CHANNEL_EXTERN(hierarchy);

namespace nnh::detail {

struct NodeAccess {
    static std::uint16_t& parent(Node& n) { return n.parent_; }
    static std::uint16_t& first(Node& n) { return n.first_; }
    static std::uint16_t& next(Node& n) { return n.next_; }
    static std::uint16_t& prev(Node& n) { return n.prev_; }
    static std::uint8_t& depth(Node& n) { return n.depth_; }
    static std::uint16_t& children(Node& n) { return n.children_; }
    static std::uint32_t& cached_rotation(Node& n) { return n.cached_rotation_; }
    static float& cached_sin(Node& n) { return n.cached_sin_; }
    static float& cached_cos(Node& n) { return n.cached_cos_; }
    static WorldTransform2D& world(Node& n) { return n.world_; }
    static std::uint16_t parent(const Node& n) { return n.parent_; }
    static std::uint16_t first(const Node& n) { return n.first_; }
    static std::uint16_t next(const Node& n) { return n.next_; }
    static std::uint16_t prev(const Node& n) { return n.prev_; }
    static int depth(const Node& n) { return n.depth_; }
    static int children(const Node& n) { return n.children_; }
    static World* world_of(const Hierarchy& h) { return h.world_; }
    static int kind_of(const Hierarchy& h) { return h.kind_; }
};
using NA = NodeAccess;

/// Module words (WorldModuleState::words).
enum ModuleWord : int { kRoots = 0, kDeepest = 1, kComputed = 2, kRefusals = 3, kBadLocals = 4 };

/// What every hierarchy source works with: the world, its block, the node kind and the node array. Empty (`nodes`
/// null) for an empty view or a world without the node kind.
struct Tree {
    World* world = nullptr;
    WorldHot* w = nullptr;
    int kind = -1;
    Node* nodes = nullptr;

    bool valid() const { return nodes != nullptr; }
    /// `e`'s node slot, or kNoNode (stale, no node).
    std::uint16_t slot_of(Entity e) const {
        const auto field = static_cast<std::uint32_t>(2 * w->slots_word + kind);
        return static_cast<std::uint16_t>(slot_by_handle(w->records, static_cast<std::uint32_t>(w->record_words),
                                                         static_cast<std::uint32_t>(w->entities.capacity), field, e));
    }
    /// The entity owning live node slot `slot`.
    Entity entity_of(std::uint16_t slot) const { return entity_at(*w, w->kinds[kind].owner[slot]); }
    Node& at(std::uint16_t slot) const { return nodes[slot]; }
    WorldModuleState& modules() const { return w->cold->modules; }
};

Tree tree_of(const Hierarchy& h);
Tree tree_of(World& world); ///< the kind looked up (the remove hook)

/// Misuse: logs an error and asserts (Debug/Development); Release goes on harmlessly.
void hierarchy_misuse(const char* what);

// ---- Links (node_links.cpp): slots, never handles; no recursion, no stack ----

/// Appends `child` (a root) after `parent`'s children and tells the active walks.
void link_last(const Tree& t, std::uint16_t parent, std::uint16_t child);
/// Takes `child` out of its parent's children (a root: nothing) and tells the active walks; its links are cleared.
void unlink(const Tree& t, std::uint16_t child);
/// The deepest depth in `root`'s subtree minus `root`'s depth (0 for a leaf).
int subtree_height(const Tree& t, std::uint16_t root);
/// Writes the depths of `root`'s subtree from `root`'s parent (or 0 for a root).
void rewrite_depths(const Tree& t, std::uint16_t root);
/// The next node of `root`'s subtree in pre-order after `n`, or kNoNode (stackless: parent links).
inline std::uint16_t next_in_subtree(const Tree& t, std::uint16_t root, std::uint16_t n) {
    if (NA::first(t.nodes[n]) != kNoNode) return NA::first(t.nodes[n]);
    while (n != root && NA::next(t.nodes[n]) == kNoNode) n = NA::parent(t.nodes[n]);
    return n == root ? kNoNode : NA::next(t.nodes[n]);
}

/// The sine and cosine of the node's local rotation through its cache (recomputed only when the rotation's bits
/// changed: the same bits as computing them).
NNH_DET_INLINE SinCos cached_sin_cos(Node& node) {
    const std::uint32_t bits = float_bits(node.local.rotation);
    if (bits != NA::cached_rotation(node)) {
        const SinCos sc = sin_cos_inline(node.local.rotation);
        NA::cached_rotation(node) = bits;
        NA::cached_sin(node) = sc.sin;
        NA::cached_cos(node) = sc.cos;
    }
    return {NA::cached_sin(node), NA::cached_cos(node)};
}

} // namespace nnh::detail
