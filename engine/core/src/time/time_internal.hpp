#pragma once
// Hooks the frame loop uses to drive the clock. Internal.

#include <cstdint>

namespace nnh::detail {

/// Called by nnh::init() with the platform clock: now() starts at 0 here.
void time_reset(std::int64_t platform_ns);

/// Called by nnh::frame() when a frame starts; updates delta().
void time_frame_started(std::int64_t platform_ns);

} // namespace nnh::detail
