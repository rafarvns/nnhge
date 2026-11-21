#pragma once
// The simulator's end of the console remote (spec 013 FR-4, D28): a loopback TCP connection to the
// `nnh run --pc` that started it, named by NNH_REMOTE. Internal to engine/platforms/pc/. No SDL here, so the
// socket headers stay in remote_pc.cpp. Debug/Development only: Release never calls these.

namespace nnh::platform::pc {

/// At init: connects to NNH_REMOTE (loopback only, 1 s timeout). Without it, or on failure, there is
/// simply no connection (the game runs as before).
void remote_open();
/// At shutdown (and when init fails): closes the connection, if any.
void remote_close();
/// While connected, sends one log line (a '\n' is added) instead of printing it. @return false when not
/// connected (the caller prints the line), true when the line went to nnh.
bool remote_log_line(const char* line);

} // namespace nnh::platform::pc
