#pragma once
/// @file nnh/platform/app.hpp
/// Platform contract — application lifecycle. Implemented by every platform library
/// (engine/platforms/<system>/), called only by nnh::core. Not for games.
/// See docs/architecture/platforms.md.

#include "nnh/app.hpp"

namespace nnh::platform {

/// Brings up the system (video, input). Called once by nnh::init(), before any other contract function.
/// @return false if the system can't run the game; nnh::init() then fails and nothing else is called.
/// May allocate. Must not be called again before shutdown().
bool init(const AppConfig& config);

/// Releases everything init() acquired. Called once by nnh::shutdown() after the last frame ended.
/// After it returns, init() may be called again.
void shutdown();

/// Processes pending system events. Called once per nnh::frame(), outside a frame (no frame begun).
/// @return false when the app must quit (HOME Menu close, window closed, test/automation limit).
bool poll_events();

} // namespace nnh::platform
