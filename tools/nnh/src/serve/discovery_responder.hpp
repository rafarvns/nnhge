#pragma once
// nnh serve answering NNH Link's discovery (spec 011): "NNHLINK?1" on UDP 17492 gets
// "NNHSERVE1 <server id> <name>" back to the sender, if it is on the local network. Tested on the loopback
// interface in tools/nnh/tests/test_serve.cpp.

#include <string>

#include "link_types.hpp"
#include "nnh_link/messages.hpp"

namespace nnh::tool {

/// Answers one question if one arrives within `timeout_ms`. @return true if it answered.
bool answer_discovery(const net::Socket& udp, const Id& server_id, const std::string& name, int timeout_ms);

} // namespace nnh::tool
