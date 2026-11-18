#pragma once
// FBI's "Receive URLs over the network" message (spec 010 FR-6): TCP port 5000, a uint32 big-endian
// length, then the URLs separated by '\n' (the protocol of FBI's servefiles.py). Pure; unit tested in
// tools/nnh/tests/test_fbi.cpp.

#include <cstdint>
#include <string>
#include <vector>

namespace nnh::tool {

inline constexpr std::uint16_t kFbiPort = 5000;

std::vector<std::uint8_t> fbi_url_message(const std::vector<std::string>& urls);

/// "http://<host>:<port>/<name>" with `name` percent-encoded except letters, digits and "-._~".
std::string http_url(const std::string& host, std::uint16_t port, const std::string& name);

} // namespace nnh::tool
