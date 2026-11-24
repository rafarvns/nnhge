// Nodes and the hierarchy view (spec 022 FR-5): a Node made at a local computes its root world; copies carry the
// transforms, never the links; the node kind's registration with the engine's remove hook; a view of a world's nodes.

#include "hierarchy/hierarchy_internal.hpp"
#include "nnh/assert.hpp"

NNH_LOG_CHANNEL(hierarchy);

namespace nnh {

static_assert(sizeof(Node) == 80, "nnh::Node is 80 bytes on every system (plan 022 \"Data and memory\")");
static_assert(alignof(Node) == 4);
static_assert(sizeof(Transform2D) == 20 && sizeof(Affine2D) == 24 && sizeof(WorldTransform2D) == 36);

Node::Node(const Transform2D& l) : local(l) {
    detail::root_world(local, detail::cached_sin_cos(*this), world_);
}

Node::Node(const Node& other)
    : local(other.local), cached_rotation_(other.cached_rotation_), cached_sin_(other.cached_sin_),
      cached_cos_(other.cached_cos_), world_(other.world_) {}

Node& Node::operator=(const Node& other) {
    local = other.local;
    cached_rotation_ = other.cached_rotation_;
    cached_sin_ = other.cached_sin_;
    cached_cos_ = other.cached_cos_;
    world_ = other.world_;
    return *this;
}

ComponentDesc node_component(int capacity) {
    return component<Node>(capacity, [](World& w, Entity e, Node& n) { detail::node_removed(w, e, n); });
}

Hierarchy::Hierarchy(World& world) : world_(&world) {
    kind_ = detail::world_kind_of(world, detail::type_id<Node>());
    if (kind_ < 0) {
        detail::hierarchy_misuse("nnh::Hierarchy: the world has no nnh::Node kind (register nnh::node_component())");
        world_ = nullptr;
        return;
    }
    nodes_ = world.storage<Node>();
}

namespace detail {

void hierarchy_misuse(const char* what) {
    NNH_LOG_ERROR(hierarchy, "%s", what);
    NNH_ASSERT(false, "%s", what);
}

Tree tree_of(const Hierarchy& h) {
    Tree t;
    World* world = NA::world_of(h);
    if (world == nullptr) return t;
    t.world = world;
    t.w = WorldAccess::hot(*world);
    if (t.w == nullptr) return {};
    t.kind = NA::kind_of(h);
    t.nodes = static_cast<Node*>(t.w->kinds[t.kind].data);
    return t;
}

Tree tree_of(World& world) {
    Tree t;
    t.w = WorldAccess::hot(world);
    if (t.w == nullptr) return {};
    t.kind = world_kind_of(world, type_id<Node>());
    if (t.kind < 0) return {};
    t.world = &world;
    t.nodes = static_cast<Node*>(t.w->kinds[t.kind].data);
    return t;
}

std::uint16_t ChildRange::next_of(const Node* nodes, std::uint16_t slot) { return NA::next(nodes[slot]); }

} // namespace detail
} // namespace nnh
