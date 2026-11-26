#pragma once
// Hooks the frame loop uses to drive the clock. Internal.

#include <cstdint>

namespace nnh::detail {

/// Called by nnh::init() with the platform clock: now() starts at 0 here.
void time_reset(std::int64_t platform_ns);

/// Called by nnh::frame() when a frame starts; updates delta().
void time_frame_started(std::int64_t platform_ns);

/// Debug/Development (spec 030 D52, app/dev_pause.cpp): set before a second time_frame_started() after a pause, which
/// then restarts the frame clock at `platform_ns` and keeps the delta measured before the pause.
extern bool g_time_keep_delta;

} // namespace nnh::detail
