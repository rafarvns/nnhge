#pragma once
/// @file nnh/hierarchy.hpp
/// Hierarchies of entities (spec 022, decision D37): an entity takes part through the engine's one component kind,
/// nnh::Node — a local transform the game writes, the world transform last computed, and links to its parent and
/// children in the same world. One pass per frame computes every world transform (the same bits on PC and 3DS);
/// world_now() gives a fresh one on demand; attach and detach keep the world transform or the local one; destroying a
/// parent destroys its children through spec 020's queue. See docs/api/hierarchy.md.
///
/// Optional (constitution XI): a game that doesn't include this header links none of it. Light: <nnh/entities.hpp> and
/// <nnh/transform.hpp>; no graphics, collision, camera, tween, animation, tilemap or prefab header.
///
/// @code
/// const nnh::ComponentDesc kinds[] = {nnh::node_component(64), nnh::component<Sprite>(64)};
/// nnh::World world = std::move(nnh::World::create({.max_entities = 64}, kinds).value()); // check it in real code
/// nnh::Hierarchy tree(world);
/// const nnh::Entity ship = world.spawn(), gun = world.spawn();
/// world.add<nnh::Node>(ship, nnh::Node({.position = {100, 60}}));
/// world.add<nnh::Node>(gun, nnh::Node({.position = {12, -4}}));
/// tree.attach(gun, ship, nnh::Reparent::KeepLocal);       // the gun rides on the ship
/// tree.local(ship)->rotation += 0.1f;                     // turn the ship...
/// tree.propagate();                                       // ...once per frame, before the camera and drawing
/// const nnh::Vec2 at = tree.world_transform(gun)->position(); // the gun, turned with it
/// @endcode

#include <cstdint>
#include <type_traits>
#include <utility>

#include "nnh/detail/hierarchy_walk.hpp"
#include "nnh/entities.hpp"
#include "nnh/transform.hpp"

namespace nnh {

enum class Screen : std::uint8_t; // <nnh/gfx.hpp>: only debug_draw takes one

/// Levels in a hierarchy: a root is depth 0, the deepest node depth 15.
inline constexpr int kMaxHierarchyDepth = 16;

/// The engine's node component (FR-5): register it with node_component(); reach it with world.get<nnh::Node>(e) or a
/// Hierarchy. 80 bytes on every system. Copying a Node copies its local and world transforms, never its links (a copy
/// added to another entity is a root); assigning keeps the destination's links.
class Node {
    // Declaration order is the layout (plan 022 "Data and memory"): links, depth, the local transform, the sine cache,
    // the world transform — what the pass reads first comes first.
    friend struct detail::NodeAccess;
    std::uint16_t parent_ = detail::kNoNode;
    std::uint16_t first_ = detail::kNoNode;
    std::uint16_t next_ = detail::kNoNode;
    std::uint16_t prev_ = detail::kNoNode; ///< the first child's prev is the last child
    std::uint8_t depth_ = 0;
    [[maybe_unused]] std::uint8_t flags_ = 0; ///< spare (keeps the 80-byte layout); unused today
    std::uint16_t children_ = 0;

public:
    /// Yours to write: a tween may target it by pointer (`&node->local.position`), a prefab places `&Node::local`.
    /// Shows in world() at the next pass, and in world_now() at once.
    Transform2D local{};

    constexpr Node() = default;
    /// A node at `local`; its world is computed now (a root's: equal to `local` bit for bit).
    explicit Node(const Transform2D& local);
    Node(const Node& other);
    Node& operator=(const Node& other);
    ~Node() = default;

    /// The world transform last computed: by the pass, an attach or detach that keeps the local, the node's creation.
    const WorldTransform2D& world() const { return world_; }
    int depth() const { return depth_; }

private:
    std::uint32_t cached_rotation_ = 0; ///< the bits of the rotation whose sine and cosine follow
    float cached_sin_ = 0;
    float cached_cos_ = 1;
    WorldTransform2D world_{};
};

/// The node kind's registration (D35): `const nnh::ComponentDesc kinds[] = {nnh::node_component(256), ...}`. Its remove
/// hook is the engine's (unlinking, cascades).
ComponentDesc node_component(int capacity);
/// With the game's own remove hook (captures ≤ 16 bytes, trivially copyable, as spec 020's hooks), run before the
/// engine's: the node is still linked and its world intact.
template <typename F>
ComponentDesc node_component(int capacity, F&& on_remove) {
    using Fn = std::decay_t<F>;
    static_assert(std::is_invocable_v<const Fn&, World&, Entity, Node&>,
                  "nnh::node_component(capacity, hook): the hook takes (nnh::World&, nnh::Entity, nnh::Node&)");
    return component<Node>(capacity, [fn = Fn(static_cast<F&&>(on_remove))](World& w, Entity e, Node& n) {
        fn(w, e, n);
        detail::node_removed(w, e, n);
    });
}

/// What attach and detach keep: the world transform (the local is recomputed: a held item doesn't jump) or the local
/// transform (the node moves to its place under the new parent).
enum class Reparent : std::uint8_t { KeepWorld, KeepLocal };

/// Why an attach would be refused, or Ok (can_attach: no assert).
enum class AttachCheck : std::uint8_t {
    Ok,
    Stale,        ///< a null, stale or dying entity (child or parent)
    NoNode,       ///< either has no node
    Cycle,        ///< the child is the parent or one of its ancestors
    TooDeep,      ///< the child's subtree would reach depth 16
    NotInvertible ///< keep-world under a parent whose map can't be inverted (a zero scale)
};

/// A hierarchy's counts (FR-22): nodes live, peak and capacity (the kind's); roots, deepest depth and nodes computed as
/// of the last pass; attaches refused and non-finite locals met by passes since the world's creation.
struct HierarchyStats {
    int nodes = 0, peak = 0, capacity = 0;
    int roots = 0, deepest = 0, computed = 0;
    int refusals = 0, bad_locals = 0;
};

class Hierarchy;

namespace detail {

/// The children of a node in attach order: `for (nnh::Entity c : tree.children(ship))`. Each child's next sibling is
/// read before the child is yielded, so destroying or detaching the current child inside is safe.
class ChildRange {
public:
    class Iterator {
    public:
        Entity operator*() const { return entity_at(*w_, w_->kinds[kind_].owner[slot_]); }
        Iterator& operator++() {
            slot_ = next_;
            next_ = slot_ == kNoNode ? kNoNode : ChildRange::next_of(nodes_, slot_);
            return *this;
        }
        bool operator!=(const Iterator& o) const { return slot_ != o.slot_; }

    private:
        friend class ChildRange;
        const WorldHot* w_ = nullptr;
        const Node* nodes_ = nullptr;
        int kind_ = 0;
        std::uint16_t slot_ = kNoNode;
        std::uint16_t next_ = kNoNode;
    };
    ChildRange() = default;
    ChildRange(const WorldHot* w, int kind, const Node* nodes, std::uint16_t first) : w_(w), nodes_(nodes), kind_(kind), first_(first) {}
    Iterator begin() const {
        Iterator i;
        i.w_ = w_;
        i.nodes_ = nodes_;
        i.kind_ = kind_;
        i.slot_ = first_;
        i.next_ = first_ == kNoNode ? kNoNode : next_of(nodes_, first_);
        return i;
    }
    Iterator end() const { return {}; }

private:
    static std::uint16_t next_of(const Node* nodes, std::uint16_t slot);
    const WorldHot* w_ = nullptr;
    const Node* nodes_ = nullptr;
    int kind_ = 0;
    std::uint16_t first_ = kNoNode;
};

} // namespace detail

/// A view of a world's nodes, the kind lookup done once: cheap to copy, valid for the world's life. Empty (default, or
/// made from a world without the node kind: asserts) it answers nothing and its commands do nothing. Every call is
/// allocation-free and calls no virtual function; per-frame safe.
class Hierarchy {
public:
    Hierarchy() = default;
    explicit Hierarchy(World& world);

    // ---- Structure (FR-6, FR-10, FR-11) ----

    /// Appends `child` after `parent`'s children. KeepWorld (default): the stored world is unchanged and the local
    /// becomes relative() to the parent's stored world. KeepLocal: the local is kept and the world is computed from the
    /// parent's stored world. Attaching to the current parent changes nothing (true). Refused — false, an assert in
    /// Debug/Development, a stats refusal, nothing changed — for each AttachCheck reason. Linear in the moved subtree.
    bool attach(Entity child, Entity parent, Reparent mode = Reparent::KeepWorld);
    /// What attach would answer, without asserting or counting.
    AttachCheck can_attach(Entity child, Entity parent, Reparent mode = Reparent::KeepWorld) const;
    /// Makes `child` a root (true; already a root: true, nothing changed). KeepWorld: its local becomes its stored
    /// world's position, drawing rotation and scale (a sheared world loses its shear at the next pass). KeepLocal: its
    /// world becomes to_world(local). False (asserts) for a stale entity or one without a node.
    bool detach(Entity child, Reparent mode = Reparent::KeepWorld);
    /// Detaches every child of `e` (in order) and returns how many.
    int detach_children(Entity e, Reparent mode = Reparent::KeepWorld);

    // ---- Queries (FR-7) ----

    Entity parent(Entity e) const;   ///< constant time; null for a root, a stale handle or no node
    int depth(Entity e) const;       ///< constant time; −1 without a node
    Entity root(Entity e) const;     ///< `e` itself for a root; null without a node
    int child_count(Entity e) const; ///< constant time
    Entity first_child(Entity e) const;
    Entity next_sibling(Entity e) const;
    detail::ChildRange children(Entity e) const; ///< attach order
    bool is_ancestor(Entity ancestor, Entity e) const; ///< strict: a node is not its own ancestor
    /// fn(Entity) for every descendant of `e` (not `e`), pre-order, children in attach order. Destroying, detaching,
    /// attaching and spawning inside are safe: what is destroyed or moved out is not visited, what is moved into the
    /// part not yet walked is, what is created is not.
    template <typename F>
    void each_descendant(Entity e, F&& fn) {
        detail::DescendantWalk walk(*this, e);
        for (Entity c = walk.next(); c; c = walk.next()) fn(c);
    }

    // ---- World transforms (FR-8, FR-9) ----

    /// The pass: every node's world, roots in ascending node slots, each subtree depth-first in child order, each node
    /// once from its parent's fresh world and its own current local. Once per frame, after moving things and before the
    /// camera and drawing. Inside an iteration of this world or a hook: asserts, does nothing. A non-finite local
    /// asserts (Release: treated as the identity for this pass, counted). Returns the nodes computed. Calls no game code.
    int propagate();
    /// A fresh world transform from the current locals of `e` and its ancestors: bit for bit what propagate() would
    /// store now. Stores nothing. The identity without a node. Linear in the depth.
    WorldTransform2D world_now(Entity e) const;
    /// The stored world transform (a load); null without a node.
    const WorldTransform2D* world_transform(Entity e) const;
    Transform2D* local(Entity e); ///< null without a node
    Node* node(Entity e);         ///< null without a node

    HierarchyStats stats() const;
    /// Debug/Development: for each node, a line to its parent and its local axes (8 px), on `screen`. Release: nothing.
    void debug_draw(Screen screen) const;

    World* world() const { return world_; }

private:
    friend struct detail::NodeAccess;
    friend class detail::DescendantWalk;
    World* world_ = nullptr;
    Storage<Node> nodes_{};
    int kind_ = -1;
};

} // namespace nnh
