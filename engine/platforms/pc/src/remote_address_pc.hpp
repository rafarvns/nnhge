#pragma once
// NNH_REMOTE=<ip>:<port> for the simulator (spec 013 FR-4, FR-7): set by `nnh run --pc` so the game connects
// back to that nnh. Only a loopback address (127.0.0.0/8) is accepted, so a simulator never connects to
// another computer. Pure (no SDL, no sockets); tested in tests/simulator/test_remote_address.cpp.

#include <cstdint>

namespace nnh::platform::pc {

struct RemoteAddress {
    std::uint8_t ip[4] = {};
    std::uint16_t port = 0;
};

/// "a.b.c.d:port", exactly: four decimal parts 0–255 with a = 127, and a port 1–65535. Anything else
/// (another network, a name, spaces, extra text, null) → false and `out` unchanged.
bool parse_remote_address(const char* text, RemoteAddress& out);

} // namespace nnh::platform::pc
