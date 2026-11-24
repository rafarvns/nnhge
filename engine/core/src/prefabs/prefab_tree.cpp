// Spawning a prefab hierarchy (spec 022 FR-17, FR-18; plan 022 "Tree spawn"): the tree flattened into a creation list
// (parent first, children depth-first in entry order), every kind resolved, the depth under a spawn's parent checked,
// one group reservation in the world (all or nothing), the copies as spec 021's spawn makes them, the root's
// placement, then the nodes: each linked last under its parent with its entry's local (as attach keep-local would) and
// its world computed; the spawn hooks run children before parents. The result is bit for bit the hand-written spawn,
// add, add_tag and attach calls in creation order followed by the hooks in that order. The only prefab source that
// includes the hierarchy: it is linked only through nnh::child()'s kPrefabTreeOps and spawn_under().

#include "entities/entities_internal.hpp"
#include "hierarchy/hierarchy_internal.hpp"
#include "nnh/assert.hpp"
#include "prefabs/prefab_internal.hpp"

#include <cstdio>

namespace nnh {
namespace detail {
namespace {

struct TreeEntity { // no default initializers: flatten_tree writes every entry it uses (no array clearing)
    int prefab;
    int parent; ///< index in the creation list, −1 for the root
    const PrefabChildEntry* entry; ///< null for the root
};

/// The creation list: pre-order, children in entry order (an explicit stack: ≤ 16 levels by the registration checks).
int flatten_tree(const PrefabBlock& b, int root, TreeEntity* out) {
    int n = 0;
    out[n++] = {root, -1, nullptr};
    int at[kMaxPrefabTreeLevels + 1], next[kMaxPrefabTreeLevels + 1];
    int top = 0;
    at[0] = 0;
    next[0] = 0;
    while (top >= 0) {
        const int prefab = out[at[top]].prefab;
        if (next[top] >= b.records[prefab].child_count) {
            --top;
            continue;
        }
        const PrefabChildEntry& c = b.children[b.facts[prefab].first_child + next[top]++];
        out[n] = {c.prefab, at[top], &c};
        ++top;
        at[top] = n++;
        next[top] = 0;
    }
    return n;
}

/// Entity `i`'s kinds copied from an earlier entity of the same prefab (a tree repeats its parts: one world lookup per
/// distinct prefab, plan 022 "Tree spawn" step 2). False when `i` is the first of its prefab.
bool copy_kinds(const TreeEntity* list, const ReserveEntity* reserve, int i, int entries, int* kinds, int used) {
    for (int j = 1; j < i; ++j) { // not the root: its kinds may be followed by the overrides' extras
        if (list[j].prefab != list[i].prefab) continue;
        const int* from = kinds + reserve[j].first_kind;
        for (int k = 0; k < entries; ++k) kinds[used + k] = from[k];
        return true;
    }
    return false;
}

void refuse_full(PrefabBlock& b, int index, const char* what) {
    ++b.counts.misses;
    if (b.when_full == WhenFull::Miss) return;
    char label[16];
    std::snprintf(label, sizeof label, "#%d", index);
    const char* name = b.names != nullptr ? b.names + index * kNameBytes : label;
    NNH_LOG_ERROR(prefabs, "spawn of prefab \"%s\": %s", name, what);
    NNH_ASSERT(false, "PrefabLibrary::spawn(\"%s\"): %s", name, what);
}

/// The root's overrides matched to its entries (`by_entry`, bit i of `matched`) or appended as extra kinds after them.
bool root_overrides(World& world, const PrefabEntry* entries, int n, const ComponentValue* overrides, int count,
                    std::uint32_t& matched, std::int8_t* by_entry, int* kinds, const ComponentValue** extra,
                    int& extras) {
    for (int j = 0; j < count; ++j) {
        const ComponentValue& v = overrides[j];
        int at = -1;
        for (int i = 0; i < n && at < 0; ++i)
            if (entries[i].id == v.id) at = i;
        bool twice = at >= 0 && ((matched >> at) & 1u) != 0;
        for (int k = 0; k < extras && !twice; ++k) twice = extra[k]->id == v.id;
        if (twice) {
            prefab_misuse("PrefabLibrary::spawn_with(): a kind overridden twice (the second is ignored)");
            continue;
        }
        if (at >= 0) {
            matched |= 1u << at;
            by_entry[at] = static_cast<std::int8_t>(j);
            continue;
        }
        const int kind = world_kind_of(world, v.id);
        if (kind < 0 || v.copy == nullptr || n + extras >= kMaxPrefabEntries) return false;
        kinds[n + extras] = kind;
        extra[extras++] = &v;
    }
    return true;
}

void place_root(const PrefabRecord& r, void* const* at, const SpawnSource& s) {
    unsigned char* member = static_cast<unsigned char*>(at[r.place_entry]) + r.place_offset;
    if ((r.place_flags & kPlaceRect) == 0) {
        *reinterpret_cast<Vec2*>(member) = s.position;
        return;
    }
    Rect* rect = reinterpret_cast<Rect*>(member);
    rect->x = s.position.x;
    rect->y = s.position.y;
    if (s.rect_size && (r.place_flags & kPlaceKeepSize) == 0) {
        rect->w = s.size.x;
        rect->h = s.size.y;
    }
}

void run_chain(PrefabBlock& b, int index, World& world, Entity e, const SpawnInfo& info) {
    std::uint16_t chain[kMaxPrefabHooks];
    int count = 0;
    const PrefabRecord& r = b.records[index];
    if (r.hook) chain[count++] = static_cast<std::uint16_t>(index);
    for (std::uint16_t p = r.hook_base; p != kNoHookBase && count < kMaxPrefabHooks; p = b.records[p].hook_base)
        chain[count++] = p;
    for (int i = count - 1; i >= 0 && !b.doomed; --i) b.records[chain[i]].hook(world, e, info);
}

/// Post-order (children before parents) of the creation list (pre-order): before an entity is pushed, every stacked
/// entity that isn't its parent has a complete subtree and is emitted; the rest at the end. No array to clear.
int post_order(const TreeEntity* list, int n, int* out) {
    int stack[kMaxPrefabTree];
    int top = 0, count = 0;
    for (int i = 0; i < n; ++i) {
        while (top > 0 && stack[top - 1] != list[i].parent) out[count++] = stack[--top];
        stack[top++] = i;
    }
    while (top > 0) out[count++] = stack[--top];
    return count;
}

} // namespace

/// The whole spawn. `parent`: spawn_under's (attached keep-local under it), or null.
Entity tree_spawn(PrefabBlock* b, World& world, Prefab prefab, const SpawnSource* source,
                  const ComponentValue* overrides, int count, Entity parent) {
    if (b == nullptr) {
        prefab_misuse("PrefabLibrary::spawn() on an empty or moved-from library");
        return {};
    }
    const PrefabRecord* r = record_of(b, prefab);
    if (r == nullptr || world.capacity() == 0) {
        prefab_misuse("PrefabLibrary::spawn(): a null or foreign prefab, or an empty world");
        return {};
    }
    const Tree t = tree_of(world);
    const std::uint16_t under = parent ? t.slot_of(parent) : kNoNode;
    if (!t.valid() || (parent && (under == kNoNode || world_dying(world, parent)))) {
        prefab_misuse(t.valid() ? "PrefabLibrary::spawn_under(): a stale parent, or one without a node"
                                : "PrefabLibrary::spawn(): a prefab hierarchy into a world without nnh::Node");
        return {};
    }
    bool rooted = false; // the root needs a node (children always have one: checked at registration)
    for (int j = 0; j < r->entries; ++j) rooted = rooted || b->entries[r->first + j].id == type_id<Node>();
    if (!rooted) {
        prefab_misuse("PrefabLibrary::spawn_under(): a prefab without the nnh::Node kind");
        return {};
    }
    TreeEntity list[kMaxPrefabTree];
    const int n = flatten_tree(*b, prefab.slot(), list);
    int kinds[kMaxPrefabTreeComponents + kMaxPrefabEntries];
    ReserveEntity reserve[kMaxPrefabTree];
    std::uint32_t matched = 0;
    std::int8_t by_entry[kMaxPrefabEntries];
    const ComponentValue* extra[kMaxPrefabEntries];
    int extras = 0, used = 0;
    int hooks = 0; // no hook anywhere in the tree: no post-order (plan 022 T10.3)
    for (int i = 0; i < n; ++i) {
        const PrefabRecord& ri = b->records[list[i].prefab];
        const PrefabEntry* entries = b->entries + ri.first;
        hooks |= ri.hooks;
        if (!(i > 0 && copy_kinds(list, reserve, i, ri.entries, kinds, used)) &&
            (world_kinds_of(world, &entries[0].id, sizeof(PrefabEntry), ri.entries, kinds + used) >= 0 ||
             (i == 0 && count > 0 &&
              !root_overrides(world, entries, ri.entries, overrides, count, matched, by_entry, kinds, extra, extras)))) {
            ++b->counts.misses;
            prefab_misuse("PrefabLibrary::spawn(): a component kind the world doesn't store");
            return {};
        }
        reserve[i] = {used, ri.entries + (i == 0 ? extras : 0), ri.tags, ri.tag_count};
        used += reserve[i].kind_count;
    }
    const int height = b->facts != nullptr ? b->facts[prefab.slot()].height : 0; // no facts: no children
    if (parent && NA::depth(t.nodes[under]) + 1 + height >= kMaxPrefabTreeLevels) {
        refuse_full(*b, prefab.slot(), "too deep under its parent (kMaxHierarchyDepth)");
        return {};
    }
    Entity made[kMaxPrefabTree];
    void* at[kMaxPrefabTreeComponents + kMaxPrefabEntries];
    ReserveRefusal why = ReserveRefusal::None;
    if (!world_reserve_many(world, reserve, n, kinds, made, at, why, nullptr)) {
        if (why != ReserveRefusal::Clearing)
            refuse_full(*b, prefab.slot(), why == ReserveRefusal::WorldFull
                                               ? "the world is full (raise WorldDesc::max_entities)"
                                               : "a component kind is full (raise its capacity)");
        return {};
    }
    if (why == ReserveRefusal::TagsDropped)
        prefab_misuse("PrefabLibrary::spawn(): more prefab tags than the world's tags_per_entity (the first kept)");
    // The copies, entity by entity, as spec 021's spawn makes them (the root's overrides and extras included).
    const TypeId node_kind = type_id<Node>();
    Node* nodes[kMaxPrefabTree];
    for (int i = 0; i < n; ++i) {
        const PrefabRecord& ri = b->records[list[i].prefab];
        const PrefabEntry* entries = b->entries + ri.first;
        void* const* slots = at + reserve[i].first_kind;
        for (int j = 0; j < ri.entries; ++j) {
            const bool own = i == 0 && ((matched >> j) & 1u) != 0;
            if (entries[j].destroy != nullptr) entries[j].destroy(slots[j]);
            entries[j].copy(slots[j], own ? overrides[by_entry[j]].value : b->values + (entries[j].value & kValueOffset));
            if (entries[j].id == node_kind) nodes[i] = static_cast<Node*>(slots[j]);
        }
        if (i == 0) {
            for (int k = 0; k < extras; ++k) {
                if (extra[k]->destroy != nullptr) extra[k]->destroy(slots[ri.entries + k]);
                extra[k]->copy(slots[ri.entries + k], extra[k]->value);
            }
            if (source != nullptr && source->has_position && ri.place_entry != kNoPlacement)
                place_root(ri, slots, *source);
        }
    }
    // The nodes: entry locals, links in creation order, depths, worlds (as attach keep-local).
    for (int i = 0; i < n; ++i) {
        Node& node = *nodes[i];
        // An entry's local: `node = Node(local)` byte for byte without the calls — the links stay, the world is
        // composed below, and the sine cache is refreshed when the rotation's bits changed (a copied cache always
        // holds its rotation's sine, and sin_cos(+0) is the default {0, 1}).
        if (list[i].entry != nullptr && list[i].entry->has_local != 0) node.local = list[i].entry->local;
        const SinCos sc = cached_sin_cos(node);
        const auto slot = static_cast<std::uint16_t>(&node - t.nodes);
        const std::uint16_t up = i == 0 ? under : static_cast<std::uint16_t>(nodes[list[i].parent] - t.nodes);
        if (up == kNoNode) {
            root_world(node.local, sc, NA::world(node));
            continue;
        }
        link_last(t, up, slot);
        NA::depth(node) = static_cast<std::uint8_t>(NA::depth(t.nodes[up]) + 1);
        child_world(t.nodes[up].world(), node.local, sc, NA::world(node));
    }
    ++b->counts.spawns;
    if (hooks == 0) return made[0];
    // The hooks, children before parents, each entity's chain base first; every SpawnInfo has the root's object.
    SpawnInfo root_info{}, child_info{};
    if (source != nullptr) {
        root_info = {source->has_position, source->position, source->object, source->object_index};
        child_info = {false, {}, source->object, source->object_index};
    }
    int order[kMaxPrefabTree];
    post_order(list, n, order);
    ++b->spawn_depth;
    for (int k = 0; k < n && !b->doomed; ++k) {
        const int i = order[k];
        if (b->records[list[i].prefab].hooks > 0)
            run_chain(*b, list[i].prefab, world, made[i], i == 0 ? root_info : child_info);
    }
    --b->spawn_depth;
    if (b->doomed && b->spawn_depth == 0) library_teardown(b);
    return made[0];
}

extern const PrefabTreeOps kPrefabTreeOps = {type_id<Node>(), &tree_spawn};

} // namespace detail

Entity PrefabLibrary::spawn_under(World& world, Prefab prefab, Entity parent) {
    if (!parent) {
        detail::prefab_misuse("PrefabLibrary::spawn_under(): a null parent");
        return {};
    }
    return detail::tree_spawn(block_, world, prefab, nullptr, nullptr, 0, parent);
}

Entity PrefabLibrary::spawn_under(World& world, Prefab prefab, Entity parent, Vec2 at) {
    if (!parent) {
        detail::prefab_misuse("PrefabLibrary::spawn_under(): a null parent");
        return {};
    }
    detail::SpawnSource source;
    source.has_position = true;
    source.position = at;
    return detail::tree_spawn(block_, world, prefab, &source, nullptr, 0, parent);
}

} // namespace nnh
