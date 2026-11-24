#pragma once
// Fuzz target for particles (spec 024 T5.5): bytes become a description (often invalid: create() refuses it) and a
// stream of updates, emissions, transforms, set_desc, restarts, stops, clears and expansions into a scratch with any
// camera and zoom. Aborts when alive exceeds the capacity, when a finite position falls outside the bounds or when an
// expansion writes more than its capacity. Every value given is finite (non-finite ones are misuse, tested apart).

#include <cstddef>
#include <cstdint>

namespace nnh::fuzz {

int particle_ops_target(const std::uint8_t* data, std::size_t size);

} // namespace nnh::fuzz
