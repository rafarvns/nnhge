#pragma once
// Showing a running game's log (spec 010 FR-5, used by nnh run and nnh serve): coloured in the terminal and
// saved as build/logs/<game>-<time>.txt.

#include <string>

#include "log_listener.hpp"

namespace nnh::tool {

/// Waits up to `wait_ms` for the game to connect to `logs`, then shows and saves its lines until it exits.
/// @return false if the game never connected.
bool show_game_log(LogListener& logs, const std::string& root, const std::string& game, int wait_ms);

} // namespace nnh::tool
