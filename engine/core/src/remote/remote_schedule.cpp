// Console remote schedule (see remote_schedule.hpp).

#include "remote/remote_schedule.hpp"

namespace nnh::detail::remote {

bool RemoteSchedule::push(const Message& message, std::uint32_t now) {
    if (count_ == kScheduleCapacity) return false;
    Message& slot = entries_[(head_ + count_) % kScheduleCapacity];
    slot = message;
    slot.frame = resolve_frame(message.frame, now);
    ++count_;
    return true;
}

bool RemoteSchedule::pop_due(std::uint32_t now, Message& out) {
    if (count_ == 0 || entries_[head_].frame > now) return false;
    out = entries_[head_];
    head_ = (head_ + 1) % kScheduleCapacity;
    --count_;
    return true;
}

} // namespace nnh::detail::remote
