#pragma once
// Console remote schedule (spec 013 FR-1, FR-8): the accepted messages waiting for their frame, in arrival order.
// A fixed 64-entry FIFO: the head is due once its frame has started; a message whose frame already passed is
// still applied (at once) and reported Late. Pure, no heap. Tests: tests/remote/test_remote_schedule.cpp.

#include <cstddef>
#include <cstdint>

#include "remote/remote_wire.hpp"

namespace nnh::detail::remote {

inline constexpr std::size_t kScheduleCapacity = 64;
static_assert(sizeof(Message) <= 32, "the schedule stays at 2 KB (plan 013, Data and memory)");

/// True when a message applied at frame `applied` came after the frame it wanted (answer: Late{seq, wanted,
/// applied}).
constexpr bool is_late(const Message& message, std::uint32_t applied) { return message.frame < applied; }

class RemoteSchedule {
public:
    /// Queues `message` at the start of frame `now` (frame 0 → `now`). @return false when full (answer: Dropped).
    bool push(const Message& message, std::uint32_t now);

    /// Takes the oldest message if it is due at frame `now` (its frame ≤ `now`). Messages come out in arrival
    /// order: a later-queued message never overtakes an earlier one. @return false when none is due.
    bool pop_due(std::uint32_t now, Message& out);

    std::size_t size() const { return count_; }
    std::size_t room() const { return kScheduleCapacity - count_; }
    void clear() { head_ = count_ = 0; }

private:
    Message entries_[kScheduleCapacity];
    std::size_t head_ = 0;
    std::size_t count_ = 0;
};

} // namespace nnh::detail::remote
