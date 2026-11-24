#pragma once
// The engine's integer finalizer (spec 018's camera shake, moved here by spec 024 T1.1 so the particle generator shares
// it): a lowbias32-style multiply/xor-shift, every input bit changes about half the output bits. Integers only: the same
// bits on every system.

#include <cstdint>

namespace nnh::detail {

inline std::uint32_t mix32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

} // namespace nnh::detail
