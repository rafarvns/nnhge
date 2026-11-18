#pragma once
// Finding `nnh serve` on the local network (spec 011): the console broadcasts "NNHLINK?1" to UDP 17492;
// each server answers to the sender "NNHSERVE1 <server id, 32 hex digits> <name>". Pure; unit tested and
// fuzzed (link/tests/test_messages.cpp).

#include <cstddef>
#include <string>

#include "nnh_link/messages.hpp"

namespace nnh::link {

inline constexpr char kDiscoveryQuestion[] = "NNHLINK?1";
inline constexpr std::size_t kMaxServerName = 32;

bool is_discovery_question(const char* data, std::size_t size);

/// The answer; the name is cut to 32 bytes and anything but printable ASCII becomes '?'.
std::string discovery_answer(const Id& server_id, const std::string& name);

/// Parses an answer. @return false unless it is exactly "NNHSERVE1 <32 hex> <1-32 printable ASCII>".
bool parse_discovery_answer(const char* data, std::size_t size, Id& server_id, std::string& name);

} // namespace nnh::link
