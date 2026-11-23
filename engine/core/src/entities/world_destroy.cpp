// Destroying entities (spec 020 FR-13): step 1 runs the remove hooks of the entity's components in registration
// order with every component attached; step 2 destroys them (slots hold T{}, quarantined), zeroes the tags and makes
// the handle stale. A destroy from inside a hook only marks the entity dying and queues it (FIFO); the outermost
// operation processes the queue before it returns, so cascades never recurse.

#include "entities/world_internal.hpp"

namespace nnh::detail {

void run_hook(World& world, WorldHot& w, int kind, int slot, Entity e);

namespace {

void enqueue(WorldCold& c, std::uint32_t slot) {
    c.queue_next[slot] = kNoSlot;
    if (c.queue_tail < 0) {
        c.queue_head = static_cast<int>(slot);
    } else {
        c.queue_next[c.queue_tail] = static_cast<std::uint16_t>(slot);
    }
    c.queue_tail = static_cast<int>(slot);
}

int dequeue(WorldCold& c) {
    const int slot = c.queue_head;
    if (slot < 0) return -1;
    const std::uint16_t next = c.queue_next[slot];
    c.queue_head = next == kNoSlot ? -1 : next;
    if (c.queue_head < 0) c.queue_tail = -1;
    return slot;
}

void process(World& world, WorldHot& w, std::uint32_t slot) {
    std::uint32_t* r = record_at(w, slot);
    const Entity e = make_handle<World>(slot, r[0] & 0xFFFFu);
    for (int k = 0; k < w.kind_count; ++k) { // step 1: hooks, data intact
        if ((w.pending & kPendingDoomed) != 0) return;
        if ((r[1] >> k & 1u) == 0) continue;
        run_hook(world, w, k, record_slots(w, r)[k], e);
    }
    if ((w.pending & kPendingDoomed) != 0) return;
    std::uint16_t* slots = record_slots(w, r); // step 2: gone
    for (int k = 0; k < w.kind_count; ++k) {
        if ((r[1] >> k & 1u) == 0) continue;
        const int s = slots[k];
        slots[k] = kNoSlot;
        detach_component(w, k, s);
    }
    r[1] = 0;
    for (int i = 0; i < w.tags; ++i) r[2 + i] = 0;
    r[0] = next_generation(r[0] & 0xFFFFu); // stale from now on
    w.entities.retire(static_cast<int>(slot));
    w.cold->dying[slot >> 5] &= ~(1u << (slot & 31));
}

} // namespace

void drain_queue(World& world, WorldHot& w) {
    for (int slot = dequeue(*w.cold); slot >= 0; slot = dequeue(*w.cold)) {
        if ((w.pending & kPendingDoomed) != 0) return;
        ++w.cold->hook_depth; // destroys from these hooks queue behind
        process(world, w, static_cast<std::uint32_t>(slot));
        --w.cold->hook_depth;
    }
}

void world_destroy(World& world, WorldHot& w, std::uint32_t slot) {
    w.cold->dying[slot >> 5] |= 1u << (slot & 31);
    if (w.cold->hook_depth > 0) {
        enqueue(*w.cold, slot);
        return;
    }
    ++w.cold->hook_depth;
    process(world, w, slot);
    --w.cold->hook_depth;
    drain_queue(world, w);
}

} // namespace nnh::detail

namespace nnh {

bool World::destroy(Entity e) {
    detail::WorldHot* w = hot_;
    if (w == nullptr) return false;
    if (detail::live_record(*w, e) == nullptr || detail::bit_set(w->cold->dying, e.slot())) return false;
    detail::world_destroy(*this, *w, e.bits() & 0xFFFFu);
    detail::world_finish(*w);
    return true;
}

} // namespace nnh
