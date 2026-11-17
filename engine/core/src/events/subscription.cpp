// RAII subscription handle (nnh/events.hpp).

#include "events/events_internal.hpp"
#include "nnh/events.hpp"

namespace nnh {

Subscription& Subscription::operator=(Subscription&& other) noexcept {
    if (this != &other) {
        reset();
        slot_ = other.slot_;
        generation_ = other.generation_;
        other.slot_ = -1;
    }
    return *this;
}

void Subscription::reset() {
    if (slot_ < 0) return;
    detail::events_unsubscribe(slot_, generation_);
    slot_ = -1;
}

bool Subscription::active() const { return slot_ >= 0 && detail::events_is_subscribed(slot_, generation_); }

} // namespace nnh
