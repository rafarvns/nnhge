#pragma once
// The game's log connection, as 3dslink -s receives it (spec 010 FR-3, FR-5): nnh listens on TCP 17491; a
// Development build connects back (libctru link3dsStdio) and writes its stdout/stderr until it exits. The
// same connection carries the console remote (spec 013, D28); the session (remote/game_session.hpp) reads
// it. Tested in tools/nnh/tests/test_console_link.cpp.

#include <cstdint>
#include <string>

#include "link_types.hpp"

namespace nnh::tool {

class LogListener {
public:
    /// Starts listening (before sending the game, so its first lines aren't lost).
    bool open(Ipv4 address, std::uint16_t port);
    std::uint16_t port() const { return listener_.local_port(); }

    /// Waits up to `timeout_ms` for the game to connect, then stops listening (one game per run).
    bool wait_for_game(int timeout_ms);

    /// The game's connection, once (invalid afterwards, or if no game connected).
    net::Socket take_game();

private:
    net::Socket listener_;
    net::Socket game_;
};

} // namespace nnh::tool
