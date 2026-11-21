#pragma once
// Following a running game (spec 010 FR-5; spec 013): its log coloured in the terminal and saved as
// build/logs/<game>-<time>.txt, screenshots in build/logs/<game>-<time>/, and — for nnh run — a test script
// or the commands typed in the terminal (remote/game_session.hpp). Used by nnh run and nnh serve.

#include <string>

#include "link_types.hpp"
#include "log_listener.hpp"

namespace nnh::tool {

class StdinLines;
namespace remote {
class ScriptRunner;
}

/// What nnh run adds to showing the log.
struct GameRun {
    remote::ScriptRunner* runner = nullptr; ///< a script, or interactive mode; null = the log only
    StdinLines* typed = nullptr;            ///< interactive mode
    int linger_ms = 1000;                   ///< after a script's verdict, how long to wait for the game to close
};

/// Shows and saves the log of the connected `game` until it closes (or a script's verdict).
/// @return the session's exit code (0 without a script).
int follow_game(net::Socket& game, const std::string& root, const std::string& name, const GameRun& run);

/// Waits up to `wait_ms` for the game to connect to `logs`, then follows it with nothing else (nnh serve).
/// @return false if the game never connected.
bool show_game_log(LogListener& logs, const std::string& root, const std::string& game, int wait_ms);

} // namespace nnh::tool
