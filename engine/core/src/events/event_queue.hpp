#pragma once
// Ring buffer of event records over caller-provided memory. Pure logic; unit tested in
// tests/events/test_event_queue.cpp.
//
// The front record stays valid until pop(): handlers may publish (push) while the front record is
// being delivered without overwriting it.

#include <cstddef>
#include <cstdint>

#include "nnh/detail/type_id.hpp"

namespace nnh::detail {

struct EventRecord {
    TypeId type;
    std::uint32_t sequence; ///< increases by one per push
    std::uint16_t size;     ///< payload bytes
    std::uint16_t depth;    ///< 0 = published outside dispatch; parent depth + 1 inside a handler
    const void* payload;    ///< 8-byte aligned, valid until pop()
};

class EventQueue {
public:
    /// Uses `buffer` (8-byte aligned, `capacity` bytes) for storage and empties the queue.
    void reset(void* buffer, std::size_t capacity);

    /// Appends a copy of `size` bytes. @return false if there is no room (nothing is written).
    bool push(TypeId type, const void* data, std::size_t size, std::uint16_t depth);

    /// The oldest record, without removing it. @return false if empty.
    bool front(EventRecord& out);

    /// Removes the oldest record (no-op if empty).
    void pop();

    bool empty() const { return count_ == 0; }
    std::uint32_t next_sequence() const { return next_sequence_; }
    std::size_t capacity() const { return capacity_; }

private:
    unsigned char* buffer_ = nullptr;
    std::size_t capacity_ = 0;
    std::size_t head_ = 0; ///< offset of the oldest record
    std::size_t tail_ = 0; ///< offset where the next record goes
    std::size_t count_ = 0;
    std::uint32_t next_sequence_ = 0;

    void skip_wrap_at_head();
};

} // namespace nnh::detail
