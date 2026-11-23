// Adding and removing components (spec 020 FR-12, FR-13): the slot bookkeeping of add<T>() (the typed part is
// inline), remove<T>() with its hook, and the detach shared with destroy.

#include "entities/world_internal.hpp"

namespace nnh::detail {

int world_add(WorldHot& w, int kind, Entity e) {
    std::uint32_t* r = live_record(w, e);
    if (r == nullptr) return -1; // stale: normal play
    const int entity = e.slot();
    if (bit_set(w.cold->dying, entity)) {
        world_misuse("World::add<T>() to an entity being destroyed (ignored)");
        return -1;
    }
    std::uint16_t* slots = record_slots(w, r);
    if (slots[kind] != kNoSlot) {
        world_misuse("World::add<T>(): the entity already has this kind (the existing one is kept)");
        return kAddedExisting | slots[kind];
    }
    KindHot& k = w.kinds[kind];
    const int slot = k.slots.find_free();
    if (slot < 0) {
        ++k.slots.misses;
        world_misuse("World::add<T>(): the kind is full (raise its capacity in nnh::component<T>())");
        return -1;
    }
    attach_component(w, kind, entity, r, slot);
    return slot;
}

/// Runs `kind`'s hook for the component in `slot` of entity `e` (the component still attached). Its `removing` bit
/// is set meanwhile, so remove<T>() of it from its own hook is refused.
void run_hook(World& world, WorldHot& w, int kind, int slot, Entity e) {
    const RemoveHook& hook = w.cold->kinds[kind].hook;
    if (!hook) return;
    std::uint32_t* removing = w.kinds[kind].removing;
    removing[slot >> 5] |= 1u << (slot & 31);
    ++w.cold->hook_depth;
    hook(world, e, component_at(w, kind, slot));
    --w.cold->hook_depth;
    removing[slot >> 5] &= ~(1u << (slot & 31));
}

void drain_queue(World& world, WorldHot& w);

bool world_remove(World& world, int kind, Entity e) {
    WorldHot& w = *WorldAccess::hot(world);
    std::uint32_t* r = live_record(w, e);
    if (r == nullptr || bit_set(w.cold->dying, e.slot())) return false;
    const std::uint16_t slot = record_slots(w, r)[kind];
    if (slot == kNoSlot || bit_set(w.kinds[kind].removing, slot)) return false;
    run_hook(world, w, kind, slot, e);
    if ((w.pending & kPendingDoomed) == 0) {
        record_slots(w, r)[kind] = kNoSlot;
        r[1] &= ~(1u << kind);
        detach_component(w, kind, slot);
        if (w.cold->hook_depth == 0) drain_queue(world, w);
    }
    world_finish_if_doomed(w);
    return true;
}

} // namespace nnh::detail
