#pragma once
/// @file nnh/platform/log.hpp
/// Platform contract — console output for logs. Implemented by every platform library, called only by
/// nnh::core. Not for games. See docs/architecture/logging.md.
///
/// Rule for all platform libraries: they never call nnh::core (no logging from platform code). They
/// report problems through return values and last_error() (nnh/platform/app.hpp); core logs them.

#include "nnh/log.hpp"

namespace nnh::platform {

/// Writes one already-formatted log line (no trailing newline) to the system's developer output:
/// PC: the terminal, colored by level; 3DS: stdout (reaches the PC when launched with `3dslink -s`) and,
/// in Debug builds, the debugger. May be called before init() and after shutdown(). Must not allocate.
void log_console(LogLevel level, const char* line);

} // namespace nnh::platform
