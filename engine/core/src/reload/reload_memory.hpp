#pragma once
// The memory tag of hot reload (spec 031): the receive buffer, the batch block, the overlay table and the side tables
// of path hashes. Debug/Development only.

#include "nnh/memory.hpp"

namespace nnh::detail::reload {

MemTag& reload_memory_tag();

} // namespace nnh::detail::reload
