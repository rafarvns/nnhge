// The world's all-or-nothing reservation for prefabs (spec 021 FR-5, FR-8; plan 021): every free slot found first,
// then taken with the bookkeeping spawn() and world_add() share (world_internal.hpp), so a reservation leaves exactly
// what spawn() + add<T>() per kind + add_tag() per tag would, or nothing but a miss counter.

#include "entities/entities_internal.hpp"
#include "entities/world_internal.hpp"

namespace nnh::detail {

int world_kind_of(const World& world, TypeId id) {
    const WorldHot* w = WorldAccess::hot(world);
    if (w == nullptr || id == nullptr) return -1;
    if (w->ids[0] == id) return 0; // the ids past kind_count are null: no count check
    if (w->ids[1] == id) return 1;
    if (w->ids[2] == id) return 2;
    if (w->ids[3] == id) return 3;
    for (int k = 4; k < w->kind_count; ++k)
        if (w->ids[k] == id) return k;
    return -1;
}

Entity world_reserve(World& world, const int* kinds, int count, std::uint16_t* slots, const Tag* tags, int tag_count,
                     ReserveRefusal& why, int* full_kind) {
    WorldHot* w = WorldAccess::hot(world);
    if (w == nullptr) {
        why = ReserveRefusal::Empty;
        return {};
    }
    if (w->cold->clearing) { // nothing may survive a clear (as spawn())
        why = ReserveRefusal::Clearing;
        return {};
    }
    const int entity = w->entities.find_free(); // find_free only moves a hint past full words: no state change
    if (entity < 0) {
        ++w->entities.misses;
        why = ReserveRefusal::WorldFull;
        return {};
    }
    for (int i = 0; i < count; ++i) {
        SlotTable& table = w->kinds[kinds[i]].slots;
        const int slot = table.find_free();
        if (slot < 0) {
            ++table.misses;
            why = ReserveRefusal::KindFull;
            if (full_kind != nullptr) *full_kind = kinds[i];
            return {};
        }
        slots[i] = static_cast<std::uint16_t>(slot);
    }
    const Entity e = take_entity(*w, entity);
    std::uint32_t* r = record_at(*w, static_cast<std::uint32_t>(entity));
    for (int i = 0; i < count; ++i) attach_component(*w, kinds[i], entity, r, slots[i]);
    const int n = tag_count < w->tags ? tag_count : w->tags;
    for (int i = 0; i < n; ++i) r[2 + i] = static_cast<std::uint32_t>(tags[i]); // a new record has no tag
    why = ReserveRefusal::None;
    return e;
}

void* world_component(World& world, int kind, int slot) {
    return component_at(*WorldAccess::hot(world), kind, slot);
}

} // namespace nnh::detail
