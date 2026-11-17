#pragma once
// log.txt in the data folder: rotated to log.prev.txt at start, one line per log message. Internal.

namespace nnh::detail {

/// Rotates log.txt and starts appending every log line to it (called by nnh::init()).
void log_file_start();

/// Stops writing log.txt (called by nnh::shutdown()).
void log_file_stop();

} // namespace nnh::detail
