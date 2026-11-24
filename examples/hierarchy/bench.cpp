#include "bench.hpp"

#include <utility>

using nnh::Entity;
using nnh::Node;
using nnh::Reparent;
using nnh::World;

namespace {

constexpr nnh::Tag kBenchTag = nnh::tag("bench");

/// A seeded permutation of 0..n−1 (xorshift; no rand).
void shuffle(int* out, int n, std::uint32_t seed) {
    for (int i = 0; i < n; ++i) out[i] = i;
    for (int i = n - 1; i > 0; --i) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        const int j = static_cast<int>(seed % static_cast<std::uint32_t>(i + 1));
        std::swap(out[i], out[j]);
    }
}

} // namespace

nnh::Result<World> HierarchyBench::make_world(int entities) {
    const nnh::ComponentDesc kinds[] = {nnh::node_component(entities), nnh::component<Payload>(entities),
                                        nnh::component<Extra>(entities)};
    return World::create({.max_entities = entities, .tags_per_entity = 1}, kinds);
}

bool HierarchyBench::start() {
    auto a = make_world(1024), b = make_world(1024), c = make_world(256), d = make_world(256), e = make_world(128);
    auto lib = nnh::PrefabLibrary::create({.max_prefabs = 4, .max_entries = 16, .value_bytes = 512,
                                           .when_full = nnh::WhenFull::Miss, .max_children = 8});
    if (!a || !b || !c || !d || !e || !lib) return false;
    pass_world_ = std::move(a.value());
    now_world_ = std::move(b.value());
    spawn_world_ = std::move(c.value());
    hand_world_ = std::move(d.value());
    reparent_world_ = std::move(e.value());
    lib_ = std::move(lib.value());
    pass_tree_ = nnh::Hierarchy(pass_world_);
    now_tree_ = nnh::Hierarchy(now_world_);
    spawn_tree_ = nnh::Hierarchy(spawn_world_);
    hand_tree_ = nnh::Hierarchy(hand_world_);
    reparent_tree_ = nnh::Hierarchy(reparent_world_);
    // pass: 256 × (root, child with a grandchild, child) by prefab, consecutive slots in visiting order.
    auto leaf = lib_.add("leaf", {.tags = {kBenchTag}}, Node({.position = {3, 1}}), Payload{1, 2}, Extra{3});
    if (!leaf) return false;
    auto arm = lib_.add("arm", {.tags = {kBenchTag}, .children = {nnh::child(leaf.value())}}, Node({.position = {6, 0}}),
                        Payload{4, 5}, Extra{6});
    if (!arm) return false;
    auto tree = lib_.add("tree", {.tags = {kBenchTag}, .place = nnh::place(&Node::local),
                                  .children = {nnh::child(arm.value()), nnh::child(leaf.value(), {.position = {0, 8}})}},
                         Node(), Payload{7, 8}, Extra{9});
    if (!tree) return false;
    tree_prefab_ = tree.value();
    for (int i = 0; i < kHierarchies; ++i) {
        roots_[i] = lib_.spawn(pass_world_, tree_prefab_, {static_cast<float>(i % 32) * 12, static_cast<float>(i / 32) * 30});
        turned_[i] = pass_tree_.first_child(roots_[i]);
    }
    // world_now: 256 chains of 4 by hand, created in a shuffled order of chains and levels (records scatter).
    int order[kHierarchies * 4];
    shuffle(order, kHierarchies * 4, 0x9E3779B9u);
    Entity made[kHierarchies][4] = {};
    for (int k = 0; k < kHierarchies * 4; ++k) {
        const int chain = order[k] / 4, level = order[k] % 4;
        made[chain][level] = now_world_.spawn();
        now_world_.add<Node>(made[chain][level], Node({.position = {static_cast<float>(level) * 5, 2}, .rotation = 0.1f * level}));
    }
    for (int c2 = 0; c2 < kHierarchies; ++c2) {
        for (int l = 1; l < 4; ++l) now_tree_.attach(made[c2][l], made[c2][l - 1], Reparent::KeepLocal);
        deep_[c2] = made[c2][3];
    }
    shuffle(order, kHierarchies, 0x85EBCA6Bu);
    for (int k = 0; k < kHierarchies; ++k) std::swap(deep_[k], deep_[order[k]]);
    // reparent: 16 roots with a child each (32 parents), 64 items.
    for (int i = 0; i < 16; ++i) {
        parents_[2 * i] = reparent_world_.spawn();
        reparent_world_.add<Node>(parents_[2 * i], Node({.position = {static_cast<float>(i) * 20, 40}, .rotation = 0.3f * i,
                                                        .scale = {1.5f, 1.5f}}));
        parents_[2 * i + 1] = reparent_world_.spawn();
        reparent_world_.add<Node>(parents_[2 * i + 1], Node({.position = {8, 0}, .rotation = -0.2f, .scale = {0.5f, 0.5f}}));
        reparent_tree_.attach(parents_[2 * i + 1], parents_[2 * i], Reparent::KeepLocal);
    }
    for (int i = 0; i < 64; ++i) {
        items_[i] = reparent_world_.spawn();
        reparent_world_.add<Node>(items_[i], Node({.position = {static_cast<float>(i) * 4, 100}, .rotation = 0.05f * i}));
    }
    reparent_tree_.propagate();
    pass_tree_.propagate();
    now_tree_.propagate();
    return true;
}

void HierarchyBench::prepare_turn() {
    ++frame_;
    const float step = static_cast<float>(frame_ & 63) * 0.25f;
    for (int i = 0; i < kHierarchies; ++i) {
        nnh::Transform2D* root = pass_tree_.local(roots_[i]);
        root->position.x = static_cast<float>(i % 32) * 12 + step;
        root->rotation = 0.01f * static_cast<float>((frame_ + static_cast<std::uint32_t>(i)) & 255);
        pass_tree_.local(turned_[i])->rotation = 0.02f * static_cast<float>(frame_ & 127);
    }
}

void HierarchyBench::prepare_move() {
    ++frame_;
    const float step = static_cast<float>(frame_ & 63) * 0.25f;
    for (int i = 0; i < kHierarchies; ++i) pass_tree_.local(roots_[i])->position.y = static_cast<float>(i / 32) * 30 + step;
}

int HierarchyBench::run_pass() { return pass_tree_.propagate(); }

int HierarchyBench::run_world_now() {
    int sum = 0;
    for (const Entity e : deep_) sum += static_cast<int>(now_tree_.world_now(e).map.origin.x);
    return sum;
}

int HierarchyBench::run_spawn() {
    Entity* roots = spawned_;
    for (int i = 0; i < kSpawned; ++i) roots[i] = lib_.spawn(spawn_world_, tree_prefab_, {static_cast<float>(i), 0});
    for (int i = 0; i < kSpawned; ++i) spawn_world_.destroy(roots[i]);
    spawn_world_.flush();
    return spawn_world_.count();
}

int HierarchyBench::run_hand() {
    // The same 64 hierarchies by hand: spawn, add, add_tag, attach keep-local in the prefab's creation order.
    const auto node = [this](const nnh::Transform2D& local, Payload p, Extra x, Entity parent) {
        const Entity e = hand_world_.spawn();
        hand_world_.add<Node>(e, Node(local));
        hand_world_.add<Payload>(e, p);
        hand_world_.add<Extra>(e, x);
        hand_world_.add_tag(e, kBenchTag);
        if (parent) hand_tree_.attach(e, parent, Reparent::KeepLocal);
        return e;
    };
    Entity* roots = spawned_;
    for (int i = 0; i < kSpawned; ++i) {
        roots[i] = node({.position = {static_cast<float>(i), 0}}, {7, 8}, {9}, {});
        const Entity a = node({.position = {6, 0}}, {4, 5}, {6}, roots[i]);
        node({.position = {3, 1}}, {1, 2}, {3}, a);
        node({.position = {0, 8}}, {1, 2}, {3}, roots[i]);
    }
    for (int i = 0; i < kSpawned; ++i) hand_world_.destroy(roots[i]);
    hand_world_.flush();
    return hand_world_.count();
}

int HierarchyBench::run_reparent() {
    int ok = 0;
    for (int i = 0; i < kReparents; ++i)
        ok += reparent_tree_.attach(items_[i & 63], parents_[(i * 7) & 31], Reparent::KeepWorld) ? 1 : 0;
    for (int i = 0; i < kReparents; ++i)
        ok += reparent_tree_.attach(items_[i & 63], parents_[(i * 5 + 3) & 31], Reparent::KeepLocal) ? 1 : 0;
    return ok;
}
