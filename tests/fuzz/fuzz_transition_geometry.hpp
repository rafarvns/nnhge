#pragma once
// Fuzz target of the transition effect geometry (spec 023, fuzz_transition_geometry.cpp): every 26 bytes are one
// effect (fields from raw bytes), a coverage (raw float bits) and a placement; both screens are drawn and checked
// (inside the screen, at most max_rects()); a broken promise aborts.

#include <cstddef>
#include <cstdint>

namespace nnh::fuzz {

int transition_geometry_target(const std::uint8_t* data, std::size_t size);

} // namespace nnh::fuzz
