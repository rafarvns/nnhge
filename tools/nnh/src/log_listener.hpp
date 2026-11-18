#pragma once
// The game's log, received like 3dslink -s does (spec 010 FR-3, FR-5): nnh listens on TCP 17491; a
// Development build connects back (libctru link3dsStdio) and writes its stdout/stderr until it exits.
// Tested in tools/nnh/tests/test_console_link.cpp.

#include <cstdint>
#include <string>

#include "link_types.hpp"

namespace nnh::tool {

class LogListener {
public:
    /// Starts listening (before sending the game, so its first lines aren't lost).
    bool open(Ipv4 address, std::uint16_t port);
    std::uint16_t port() const { return listener_.local_port(); }

    /// Waits up to `timeout_ms` for the game to connect.
    bool wait_for_game(int timeout_ms);

    /// Calls `on_line(line, context)` for every line ('\n'-separated, '\r' removed, a last unterminated line
    /// included) until the game closes the connection.
    void read_lines(void (*on_line)(const std::string& line, void* context), void* context);

private:
    net::Socket listener_;
    net::Socket game_;
};

} // namespace nnh::tool
