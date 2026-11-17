#pragma once
/// @file nnh/platform/time.hpp
/// Platform contract — clock. Implemented by every platform library, called only by nnh::core.
/// Not for games. See docs/architecture/platforms.md.

#include <cstdint>

namespace nnh::platform {

/// Nanoseconds from a monotonic clock with an arbitrary origin. Never decreases, keeps counting while
/// the game runs, resolution at least 1 ms. Valid after init(). Must not allocate.
std::int64_t ticks_ns();

} // namespace nnh::platform
