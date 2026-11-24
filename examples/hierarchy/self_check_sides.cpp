#include "self_check_sides.hpp"

#include <cstring>
#include <utility>

using nnh::Entity;
using nnh::Node;
using nnh::Reparent;
using nnh::World;

namespace hcheck {

template <bool kPrefab>
bool Side<kPrefab>::start() {
    Side* self = this;
    const nnh::ComponentDesc kinds[] = {
        nnh::node_component(kCapacity[kNode], [self](World&, Entity, Node&) { ++self->quarantined_[kNode]; }),
        nnh::component<A>(kCapacity[kA], [self](World&, Entity, A&) { ++self->quarantined_[kA]; }),
        nnh::component<B>(kCapacity[kB], [self](World&, Entity, B&) { ++self->quarantined_[kB]; }),
        nnh::component<C>(kCapacity[kC], [self](World&, Entity, C&) { ++self->quarantined_[kC]; }),
        nnh::component<D>(kCapacity[kD], [self](World& w, Entity, D& d) { self->d_hook(w, d); }),
        nnh::component<K>(kCapacity[kK], [self](World&, Entity, K&) {
            ++self->quarantined_[kK];
            ++self->quarantined_entities_;
        })};
    auto made = World::create({.max_entities = kEntities, .tags_per_entity = 4}, kinds);
    if (!made) return false;
    world_ = std::move(made.value());
    tree_ = nnh::Hierarchy(world_);
    target_ = {&Side::run_hook, this};
    if constexpr (kPrefab) {
        auto lib = nnh::PrefabLibrary::create({.max_prefabs = 16, .max_entries = 64, .value_bytes = 1024,
                                               .when_full = nnh::WhenFull::Miss, .max_children = 48});
        if (!lib) return false;
        lib_ = std::move(lib.value());
        if (!register_prefabs(lib_, prefabs_, &target_)) return false;
    }
    return true;
}

template <bool kPrefab>
void Side<kPrefab>::d_hook(World& w, D& d) {
    ++quarantined_[kD];
    if (w.clearing()) return;
    ++stats.d_hooks;
    const std::uint32_t call = d_calls_++;
    const Entity victim = handle(d.v * 7 + call);
    if (call % 3 == 0) {
        if (w.destroy(victim)) ++stats.cascades;
    } else if (call % 3 == 1) {
        const Entity parent = handle(d.v * 13 + call);
        if (tree_.can_attach(victim, parent, Reparent::KeepLocal) == nnh::AttachCheck::Ok)
            tree_.attach(victim, parent, Reparent::KeepLocal);
    }
}

template <bool kPrefab>
void Side<kPrefab>::run_hook(void* side, int hook, World& w, Entity e) {
    Side& s = *static_cast<Side*>(side);
    if (hook == kShipHook) {
        s.mix(static_cast<std::uint32_t>(s.tree_.child_count(e)));
        const nnh::WorldTransform2D now = s.tree_.world_now(e);
        std::uint32_t bits = 0;
        std::memcpy(&bits, &now.map.origin.x, 4);
        s.mix(bits);
    } else if (hook == kSpawnerHook) {
        ++s.stats.hook_spawns;
        s.remember(s.spawn(kMuzzle, nullptr, e));
    } else if (hook == kTowerHook) {
        s.mix(e.bits() ^ static_cast<std::uint32_t>(w.count()));
    }
}

template <bool kPrefab>
bool Side<kPrefab>::room(const HandDef& d, Entity parent) {
    if (world_.count() + quarantined_entities_ + d.count > kEntities) return false;
    const int live[kKinds] = {world_.storage<Node>().count(), world_.storage<A>().count(), world_.storage<B>().count(),
                              world_.storage<C>().count(),    world_.storage<D>().count(), world_.storage<K>().count()};
    for (int k = 0; k < kKinds; ++k)
        if (live[k] + quarantined_[k] + d.need[k] > kCapacity[k]) return false;
    return !parent || tree_.depth(parent) + 1 + d.height < nnh::kMaxHierarchyDepth;
}

template <bool kPrefab>
Entity Side<kPrefab>::by_hand(const HandDef& d, const nnh::Vec2* at, Entity parent) {
    Entity made[32];
    for (int i = 0; i < d.count; ++i) {
        const HandEntity& h = d.e[i];
        const Entity e = made[i] = world_.spawn();
        if (h.has[kNode]) {
            nnh::Transform2D local = h.local;
            const bool single = d.count == 1 && !parent; // spec 021's spawn path: the placement writes the local only
            if (i == 0 && at != nullptr && !single) local.position = *at;
            Node* node = world_.add<Node>(e, Node(local));
            if (i == 0 && at != nullptr && single) node->local.position = *at;
        }
        if (h.has[kA]) world_.add<A>(e, h.a);
        if (h.has[kB]) world_.add<B>(e, h.b);
        if (h.has[kC]) world_.add<C>(e, h.c);
        if (h.has[kD]) world_.add<D>(e, h.d);
        if (h.has[kK]) world_.add<K>(e, h.k);
        for (int t = 0; t < h.tag_count; ++t) world_.add_tag(e, h.tags[t]);
        const Entity up = i == 0 ? parent : made[h.parent];
        if (up) tree_.attach(e, up, Reparent::KeepLocal);
    }
    // Post-order with a stack (independent of the engine's walk): before pushing an entity, every stacked entity that
    // isn't its parent is finished (its subtree is complete), so it runs its hook; the rest at the end.
    int stack[32], top = 0;
    for (int i = 0; i <= d.count; ++i) {
        const int parent_index = i < d.count ? d.e[i].parent : -2;
        while (top > 0 && stack[top - 1] != parent_index) {
            const int j = stack[--top];
            if (d.e[j].hook != kNoHook) run_hook(this, d.e[j].hook, world_, made[j]);
        }
        if (i < d.count) stack[top++] = i;
    }
    return made[0];
}

template <bool kPrefab>
Entity Side<kPrefab>::spawn(int def, const nnh::Vec2* at, Entity parent) {
    if (world_.clearing()) return {};
    const HandDef& d = hand_def(def);
    Entity e{};
    if constexpr (kPrefab) {
        const nnh::Prefab p = prefabs_[def];
        if (parent) {
            e = at != nullptr ? lib_.spawn_under(world_, p, parent, *at) : lib_.spawn_under(world_, p, parent);
        } else {
            e = at != nullptr ? lib_.spawn(world_, p, *at) : lib_.spawn(world_, p);
        }
    } else {
        if (!room(d, parent)) {
            ++stats.misses;
            return {};
        }
        e = by_hand(d, at, parent);
    }
    if (e) ++stats.spawns;
    return e;
}

template <bool kPrefab>
int Side<kPrefab>::misses() const {
    if constexpr (kPrefab) {
        return lib_.stats().misses;
    } else {
        return stats.misses;
    }
}

template <bool kPrefab>
void Side<kPrefab>::flushed() {
    for (int& q : quarantined_) q = 0;
    quarantined_entities_ = 0;
}

template <bool kPrefab>
void Side<kPrefab>::remember(Entity e) {
    if (!e) return;
    remembered_[next_ & (kRemembered - 1)] = e;
    ++next_;
}

template <bool kPrefab>
void Side<kPrefab>::mix(std::uint32_t word) {
    for (int i = 0; i < 4; ++i) sum_ = (sum_ ^ ((word >> (8 * i)) & 0xFFu)) * 16777619u;
}

namespace {

std::uint32_t mix_bytes(std::uint32_t h, const void* p, std::size_t n) {
    const auto* b = static_cast<const unsigned char*>(p);
    for (std::size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 16777619u;
    return h;
}

} // namespace

template <bool kPrefab>
std::uint32_t Side<kPrefab>::state() {
    std::uint32_t h = 2166136261u;
    for (Entity e : world_.entities()) {
        h = mix_bytes(h, &e, sizeof e);
        if (const A* a = world_.get<A>(e)) h = mix_bytes(h, a, sizeof *a);
        if (const B* b = world_.get<B>(e)) h = mix_bytes(h, b, sizeof *b);
        if (const C* c = world_.get<C>(e)) h = mix_bytes(h, c, sizeof *c);
        if (const D* d = world_.get<D>(e)) h = mix_bytes(h, d, sizeof *d);
        if (const K* k = world_.get<K>(e)) h = mix_bytes(h, k, sizeof *k);
        std::uint32_t tags = 0;
        for (int t = 0; t < 4; ++t) tags |= world_.has_tag(e, kTags[t]) ? 1u << t : 0u;
        h = mix_bytes(h, &tags, sizeof tags);
        const Node* n = world_.get<Node>(e);
        if (n == nullptr) continue;
        const Entity parent = tree_.parent(e);
        const int depth = tree_.depth(e);
        h = mix_bytes(h, &parent, sizeof parent);
        h = mix_bytes(h, &depth, sizeof depth);
        for (Entity c : tree_.children(e)) h = mix_bytes(h, &c, sizeof c);
        h = mix_bytes(h, &n->world(), sizeof(nnh::WorldTransform2D));
        h = mix_bytes(h, &n->local, sizeof n->local);
    }
    return h;
}

template class Side<true>;
template class Side<false>;

} // namespace hcheck
