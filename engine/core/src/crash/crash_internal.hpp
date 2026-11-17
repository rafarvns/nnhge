#pragma once
// Crash handling setup. Internal.

namespace nnh::detail {

/// Installs the platform fault hooks pointing at core's crash path (called by nnh::init()).
void crash_install();

} // namespace nnh::detail
