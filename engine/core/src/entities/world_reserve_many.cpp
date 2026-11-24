// A group reservation for prefab hierarchies (spec 022 FR-17, plan 022 "Entities-internal additions"): room for every
// entity and every component is counted first, without touching any state; then the slots are taken one entity at a
// time with exactly the calls world_reserve() makes (find_free, take_entity, attach_component), so the world ends as
// `count` world_reserve() calls in a row would leave it. A refusal is counted where the first failing world_reserve()
// would count it, and nothing else changes.

#include "entities/entities_internal.hpp"
#include "entities/world_internal.hpp"

namespace nnh::detail {
namespace {

/// Free slots (neither live nor quarantined) of `table`, counted from its hint and capped at `wanted`: no state
/// change (the words below the hint have none). Padding bits read as taken.
int free_slots(const SlotTable& table, int wanted) {
    int found = 0;
    for (int w = table.hint; w < table.words && found < wanted; ++w)
        for (std::uint32_t free = ~(table.live[w] | table.quarantine[w]); free != 0 && found < wanted; free &= free - 1)
            ++found;
    return found;
}

/// Whether the group fits, deciding as `count` world_reserve() calls would: entity i needs the (i+1)-th free entity
/// slot, then each of its kinds, in order, one more free slot of that kind. Counts the first refusal. Only the kinds
/// the group names are touched (no array clearing: no memset call on the spawn path).
bool fits(WorldHot& w, const ReserveEntity* list, int count, const int* kinds, ReserveRefusal& why, int* full_kind) {
    int demand[kMaxKinds], room[kMaxKinds], used[kMaxKinds];
    const int total = list[count - 1].first_kind + list[count - 1].kind_count; // entries are consecutive
    for (int i = 0; i < total; ++i) {
        demand[kinds[i]] = 0;
        used[kinds[i]] = 0;
        room[kinds[i]] = -1;
    }
    for (int i = 0; i < total; ++i) ++demand[kinds[i]];
    const int entities = free_slots(w.entities, count);
    for (int i = 0; i < count; ++i) {
        if (i >= entities) {
            ++w.entities.misses;
            why = ReserveRefusal::WorldFull;
            return false;
        }
        for (int j = 0; j < list[i].kind_count; ++j) {
            const int k = kinds[list[i].first_kind + j];
            if (room[k] < 0) room[k] = free_slots(w.kinds[k].slots, demand[k]);
            if (++used[k] <= room[k]) continue;
            ++w.kinds[k].slots.misses;
            why = ReserveRefusal::KindFull;
            if (full_kind != nullptr) *full_kind = k;
            return false;
        }
    }
    return true;
}

} // namespace

Entity world_reserve_many(World& world, const ReserveEntity* list, int count, const int* kinds, Entity* out,
                          void** components, ReserveRefusal& why, int* full_kind) {
    WorldHot* w = WorldAccess::hot(world);
    if (w == nullptr) {
        why = ReserveRefusal::Empty;
        return {};
    }
    if (w->cold->clearing) { // nothing may survive a clear (as spawn())
        why = ReserveRefusal::Clearing;
        return {};
    }
    if (count < 1 || !fits(*w, list, count, kinds, why, full_kind)) {
        if (count < 1) why = ReserveRefusal::Empty;
        return {};
    }
    why = ReserveRefusal::None;
    int at = 0;
    for (int i = 0; i < count; ++i) {
        const ReserveEntity& d = list[i];
        const int entity = w->entities.find_free();
        out[i] = take_entity(*w, entity);
        std::uint32_t* r = record_at(*w, static_cast<std::uint32_t>(entity));
        for (int j = 0; j < d.kind_count; ++j) {
            const int k = kinds[d.first_kind + j];
            const int slot = w->kinds[k].slots.find_free();
            attach_component(*w, k, entity, r, slot);
            components[at++] = component_at(*w, k, slot);
        }
        const int n = d.tag_count < w->tags ? d.tag_count : w->tags;
        for (int t = 0; t < n; ++t) r[2 + t] = static_cast<std::uint32_t>(d.tags[t]); // a new record has no tag
        if (n < d.tag_count) why = ReserveRefusal::TagsDropped;
    }
    return out[0];
}

} // namespace nnh::detail
