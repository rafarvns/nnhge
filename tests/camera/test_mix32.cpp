// The engine's integer finalizer (spec 024 T1.1), moved from the camera shake to core/mix32.hpp with its bits
// unchanged: known values (computed by hand from the lowbias32 steps).

#include "core/mix32.hpp"
#include "test.hpp"

namespace {

std::uint32_t reference(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

} // namespace

NNH_TEST(mix32_keeps_the_camera_shake_finalizer_bits) {
    CHECK(nnh::detail::mix32(0) == 0);
    CHECK(nnh::detail::mix32(1) == 0x688990C0u);
    CHECK(nnh::detail::mix32(0x12345678u) == 0xF5E71C96u);
    CHECK(nnh::detail::mix32(0xFFFFFFFFu) == 0x6768824Au);
    for (std::uint32_t x = 0; x < 100000; x += 7) CHECK(nnh::detail::mix32(x * 2654435761u) == reference(x * 2654435761u));
}

NNH_TEST(mix32_changes_about_half_the_bits_per_input_bit) {
    int total = 0;
    for (std::uint32_t bit = 0; bit < 32; ++bit) {
        const std::uint32_t d = nnh::detail::mix32(0x12345678u) ^ nnh::detail::mix32(0x12345678u ^ (1u << bit));
        total += __builtin_popcount(d);
    }
    CHECK(total > 32 * 12 && total < 32 * 20);
}
