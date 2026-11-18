#pragma once
// The link library's names (addresses, sockets; link/, spec 011) as seen from the nnh tool's namespace.

#include "nnh_link/address.hpp"
#include "nnh_link/net.hpp"

namespace nnh::tool {

using namespace nnh::link; // Ipv4, parse_ipv4, ..., and net::

} // namespace nnh::tool
