#pragma once
// The session with a connected game (spec 013 T4.3, plan "Where each piece lives"): the socket loop around the
// pure parts. Bytes from the game go through the stream splitter; log lines are shown (coloured) and saved,
// records go to the runner; what the runner hands out is sent (messages), written (screenshots as PNG) and
// printed ("nnh: " notes, saved too). Typed lines (interactive mode) come from StdinLines. Used by nnh run
// (console and --pc) and, without a runner, by nnh serve. Tested over loopback in
// tools/nnh/tests/test_game_session.cpp.

#include <cstdio>
#include <string>

#include "link_types.hpp"

namespace nnh::tool {

class StdinLines;
namespace remote {
class ScriptRunner;
}

struct SessionConfig {
    std::FILE* out = stdout;     ///< the terminal: the game's lines and "nnh:" notes
    bool colour = false;         ///< colour the game's lines in `out`
    std::FILE* log = nullptr;    ///< the log file (lines and notes, never coloured); may be null
    std::string shots_dir;       ///< screenshots go here, created at the first one
    remote::ScriptRunner* runner = nullptr; ///< a script or interactive mode; null = only the log
    StdinLines* typed = nullptr; ///< interactive mode: the lines typed in the terminal
    int hello_timeout_ms = 3000; ///< how long the game has to answer a Hello (plan: 3 s)
    int linger_ms = 1000;        ///< after a script's verdict: how long the log is still shown, waiting for
                                 ///< the game to close (the simulator closes on Bye{quit})
};

/// Runs until the game closes the connection or, with a script, until its verdict (plus `linger_ms`).
/// @return the script's exit code (0 pass, 1 fail, 2 could not run), or 0 without a script.
int run_game_session(net::Socket& game, const SessionConfig& config);

} // namespace nnh::tool
