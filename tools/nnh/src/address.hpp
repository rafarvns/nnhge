#pragma once
// IPv4 addresses for nnh (spec 010 FR-4, FR-7): parsing, and the rule that the tool only talks to the
// local network. Pure; unit tested in tools/nnh/tests/test_address.cpp.

#include <cstdint>
#include <optional>
#include <string>

namespace nnh::tool {

struct Ipv4 {
    std::uint8_t b[4] = {};
    bool operator==(const Ipv4&) const = default;
};

/// "192.168.10.52" -> the address; nullopt for anything else (no host names, no leading zeros).
std::optional<Ipv4> parse_ipv4(const std::string& text);
std::string to_string(Ipv4 address);

/// Private (10/8, 172.16/12, 192.168/16), link-local (169.254/16) or loopback (127/8): the local network.
bool is_local_network(Ipv4 address);

/// The broadcast address of a network: `address` with every bit after the first `prefix_length` set.
Ipv4 broadcast_of(Ipv4 address, int prefix_length);

} // namespace nnh::tool
