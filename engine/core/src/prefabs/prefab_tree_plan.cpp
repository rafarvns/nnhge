// A prefab's children at registration (spec 022 FR-16; plan 022 "Prefab additions"): the entries up to the first null
// prefab, each made by nnh::child() and naming a prefab of this library (registered earlier: no cycle can exist); the
// base's children first for a variant; the node kind on the parent and on every child prefab; the tree's facts
// (entities, components, height) within the limits; the library's max_children. Pure: works on the plan. The node kind
// comes from the entries' tree operations, so this file needs no hierarchy code.

#include "prefabs/prefab_internal.hpp"

namespace nnh::detail {
namespace {

bool fail(Error& error, ErrorCode code, const char* what) {
    error = Error::make(code, "PrefabLibrary::add: %s", what);
    return false;
}

bool has_kind(const PrefabBlock& b, int prefab, TypeId kind) {
    const PrefabRecord& r = b.records[prefab];
    for (int i = 0; i < r.entries; ++i)
        if (b.entries[r.first + i].id == kind) return true;
    return false;
}

/// The parent's own (flattened) entries include `kind`.
bool plan_has_kind(const PrefabBlock& b, const PrefabPlan& plan, const ComponentValue* values, TypeId kind) {
    const PrefabRecord* base = plan.base >= 0 ? b.records + plan.base : nullptr;
    for (int i = 0; i < plan.entries; ++i) {
        const int j = plan.own[i];
        if ((j >= 0 ? values[j].id : b.entries[base->first + i].id) == kind) return true;
    }
    return false;
}

/// The base's children, then the options' own (up to the first null prefab).
bool gather(const PrefabBlock& b, const PrefabOptions& options, PrefabPlan& plan, Error& error) {
    if (plan.base >= 0 && b.facts != nullptr) {
        const PrefabRecord& base = b.records[plan.base];
        const PrefabTreeFacts& f = b.facts[plan.base];
        for (int i = 0; i < base.child_count; ++i) {
            const PrefabChildEntry& c = b.children[f.first_child + i];
            plan.child_prefab[i] = c.prefab;
            plan.child_has_local[i] = c.has_local != 0;
            plan.child_local[i] = c.local;
        }
        plan.child_total = base.child_count;
        plan.tree = b.tree;
    }
    for (const PrefabChild& c : options.children) {
        if (!c.prefab) break;
        if (c.tree == nullptr) return fail(error, ErrorCode::InvalidArgument, "a child not made by nnh::child()");
        if (record_of(&b, c.prefab) == nullptr)
            return fail(error, ErrorCode::InvalidArgument, "a child prefab not registered in this library");
        if (plan.child_total >= kMaxPrefabChildren) return fail(error, ErrorCode::LimitReached, "more than 8 children");
        plan.child_prefab[plan.child_total] = static_cast<std::uint16_t>(c.prefab.slot());
        plan.child_has_local[plan.child_total] = c.has_local;
        plan.child_local[plan.child_total] = c.local;
        ++plan.child_total;
        plan.tree = c.tree;
    }
    return true;
}

} // namespace

bool plan_children(const PrefabBlock& b, const PrefabOptions& options, const ComponentValue* values, PrefabPlan& plan,
                   Error& error) {
    if (!gather(b, options, plan, error)) return false;
    plan.tree_entities = 1;
    plan.tree_components = plan.entries;
    plan.height = 0;
    if (plan.child_total == 0) return true;
    if (b.max_children == 0 || b.children_used + plan.child_total > b.max_children)
        return fail(error, ErrorCode::LimitReached, "the library's max_children is full");
    const TypeId node = plan.tree->node_kind;
    if (!plan_has_kind(b, plan, values, node))
        return fail(error, ErrorCode::InvalidArgument, "a prefab with children needs the nnh::Node kind");
    for (int i = 0; i < plan.child_total; ++i) {
        const int c = plan.child_prefab[i];
        if (!has_kind(b, c, node)) return fail(error, ErrorCode::InvalidArgument, "a child prefab without nnh::Node");
        const PrefabTreeFacts& f = b.facts[c];
        plan.tree_entities += f.entities;
        plan.tree_components += f.components;
        plan.height = f.height + 1 > plan.height ? f.height + 1 : plan.height;
    }
    if (plan.tree_entities > kMaxPrefabTree) return fail(error, ErrorCode::LimitReached, "more than 32 entities in a tree");
    if (plan.tree_components > kMaxPrefabTreeComponents)
        return fail(error, ErrorCode::LimitReached, "more than 128 components in a tree");
    if (plan.height >= kMaxPrefabTreeLevels) return fail(error, ErrorCode::LimitReached, "a tree deeper than 16 levels");
    return true;
}

void commit_children(PrefabBlock& b, int index, const PrefabPlan& plan) {
    if (b.facts == nullptr) return;
    PrefabTreeFacts& f = b.facts[index];
    f.first_child = static_cast<std::uint16_t>(b.children_used);
    f.entities = static_cast<std::uint8_t>(plan.tree_entities);
    f.components = static_cast<std::uint8_t>(plan.tree_components);
    f.height = static_cast<std::uint8_t>(plan.height);
    for (int i = 0; i < plan.child_total; ++i) {
        PrefabChildEntry& c = b.children[b.children_used + i];
        c.prefab = plan.child_prefab[i];
        c.has_local = plan.child_has_local[i] ? 1 : 0;
        c.local = plan.child_local[i];
    }
    b.children_used += plan.child_total;
    b.records[index].child_count = static_cast<std::uint8_t>(plan.child_total);
    if (plan.child_total > 0) b.tree = plan.tree;
}

} // namespace nnh::detail
