#pragma once
// Fixed table of event subscribers over caller-provided slots. Pure logic; unit tested in
// tests/events/test_subscriber_table.cpp.
//
// A subscription is identified by {slot, generation}: removing or invalidating a slot bumps its
// generation, so old handles can't touch a slot that was reused. A slot only receives events whose
// sequence is >= its `since_sequence` (subscribers added during dispatch skip already-queued events).

#include <cstdint>

#include "nnh/events.hpp"

namespace nnh::detail {

struct SubscriberSlot {
    TypeId type = nullptr;
    EventHandler handler;
    std::uint32_t generation = 1;
    std::uint32_t since_sequence = 0;
    bool active = false;
};

class SubscriberTable {
public:
    /// Uses `slots` (constructed here) for storage and removes every subscriber. Generations start at
    /// `first_generation`, so handles from a previous table (see next_generation()) stay stale.
    void reset(SubscriberSlot* slots, int capacity, std::uint32_t first_generation = 1);

    /// @return slot index, or -1 if the table is full. `generation_out` identifies this subscription.
    int add(TypeId type, EventHandler handler, std::uint32_t since_sequence, std::uint32_t& generation_out);

    /// @return false if the handle is stale (already removed, or the slot was reused).
    bool remove(int slot, std::uint32_t generation);

    /// Calls `fn(slot)` for every active slot of `type` that accepts `sequence`. A slot removed during
    /// the loop (by a handler) is skipped from then on.
    template <typename Fn>
    void for_each_matching(TypeId type, std::uint32_t sequence, Fn&& fn) {
        for (int i = 0; i < capacity_; ++i) {
            SubscriberSlot& slot = slots_[i];
            // Wrap-safe "sequence >= since_sequence" (sequences are 32-bit and may wrap around).
            if (slot.active && slot.type == type && static_cast<std::int32_t>(sequence - slot.since_sequence) >= 0) {
                fn(slot);
            }
        }
    }

    /// True if {slot, generation} is a live subscription.
    bool is_active(int slot, std::uint32_t generation) const;

    int active_count() const;

    /// Deactivates every slot and makes every outstanding handle stale (engine shutdown).
    void invalidate_all();

    int capacity() const { return capacity_; }

    /// A generation higher than any used so far: pass it to the next reset().
    std::uint32_t next_generation() const;

private:
    SubscriberSlot* slots_ = nullptr;
    int capacity_ = 0;
};

} // namespace nnh::detail
