// Subscriber table (see subscriber_table.hpp).

#include "events/subscriber_table.hpp"

#include <new>

namespace nnh::detail {

void SubscriberTable::reset(SubscriberSlot* slots, int capacity, std::uint32_t first_generation) {
    slots_ = slots;
    capacity_ = capacity;
    for (int i = 0; i < capacity_; ++i) {
        new (&slots_[i]) SubscriberSlot{};
        slots_[i].generation = first_generation;
    }
}

int SubscriberTable::add(TypeId type, EventHandler handler, std::uint32_t since_sequence,
                         std::uint32_t& generation_out) {
    for (int i = 0; i < capacity_; ++i) {
        SubscriberSlot& slot = slots_[i];
        if (slot.active) continue;
        slot.type = type;
        slot.handler = handler;
        slot.since_sequence = since_sequence;
        slot.active = true;
        generation_out = slot.generation;
        return i;
    }
    return -1;
}

bool SubscriberTable::remove(int slot, std::uint32_t generation) {
    if (slot < 0 || slot >= capacity_) return false;
    SubscriberSlot& s = slots_[slot];
    if (!s.active || s.generation != generation) return false;
    s.active = false;
    s.handler = EventHandler{};
    ++s.generation;
    return true;
}

bool SubscriberTable::is_active(int slot, std::uint32_t generation) const {
    if (slot < 0 || slot >= capacity_) return false;
    return slots_[slot].active && slots_[slot].generation == generation;
}

int SubscriberTable::active_count() const {
    int n = 0;
    for (int i = 0; i < capacity_; ++i) n += slots_[i].active ? 1 : 0;
    return n;
}

std::uint32_t SubscriberTable::next_generation() const {
    std::uint32_t highest = 0;
    for (int i = 0; i < capacity_; ++i) highest = slots_[i].generation > highest ? slots_[i].generation : highest;
    return highest + 1;
}

void SubscriberTable::invalidate_all() {
    for (int i = 0; i < capacity_; ++i) {
        SubscriberSlot& s = slots_[i];
        s.active = false;
        s.handler = EventHandler{};
        ++s.generation;
    }
}

} // namespace nnh::detail
