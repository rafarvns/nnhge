#pragma once
// Event bus hooks used by the frame loop (app/app.cpp) and by Subscription (events/subscription.cpp).

#include <cstdint>

#include "nnh/app.hpp"

namespace nnh::detail {

/// Allocates the queue and subscriber table (memory tag `events`). @return false if out of memory.
bool events_init(const EventsConfig& config);

/// Drops pending events, ends every subscription and frees the memory. Safe to call more than once.
void events_shutdown();

/// Delivers every queued event, including the ones handlers publish meanwhile. Called by nnh::frame().
void events_dispatch();

void events_unsubscribe(int slot, std::uint32_t generation);
bool events_is_subscribed(int slot, std::uint32_t generation);

} // namespace nnh::detail
