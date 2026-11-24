// Mini-fuzz of hierarchies and transforms (spec 022 T5.6): random byte strings run as the hierarchy self-check's
// operations on both sides (fuzz_hierarchy_ops.cpp aborts on a difference) and as float bits into every transform
// function (fuzz_transform.cpp aborts on a broken promise). The real fuzzing (libFuzzer + ASan) runs on Linux.

#include <cstdint>

#include "fuzz_targets.hpp"
#include "test.hpp"

namespace {

std::uint32_t g_state = 2022;
std::uint8_t next_byte() {
    g_state = g_state * 1664525u + 1013904223u;
    return static_cast<std::uint8_t>(g_state >> 24);
}

} // namespace

NNH_TEST(hierarchy_ops_target_survives_random_inputs) {
    static std::uint8_t buffer[8192];
    for (int run = 0; run < 40; ++run) {
        const std::size_t size = (static_cast<std::size_t>(next_byte()) << 5) % sizeof buffer;
        for (std::size_t i = 0; i < size; ++i) buffer[i] = next_byte();
        CHECK(nnh::fuzz::hierarchy_ops_target(buffer, size) == 0);
    }
}

NNH_TEST(transform_target_survives_random_float_bits) {
    std::uint8_t buffer[64];
    const std::uint8_t specials[][4] = {{0, 0, 0x80, 0x7F}, {0, 0, 0xC0, 0x7F}, {0, 0, 0x80, 0xFF}, {1, 0, 0, 0},
                                        {0, 0, 0, 0x80}, {0xFF, 0xFF, 0x7F, 0x7F}};
    for (int run = 0; run < 20000; ++run) {
        for (std::uint8_t& b : buffer) b = next_byte();
        if (run % 4 == 0) { // plant special patterns
            const int at = (next_byte() % 16) * 4, s = next_byte() % 6;
            for (int i = 0; i < 4; ++i) buffer[at + i] = specials[s][i];
        }
        if (run % 3 == 0) { // keep exponents small so the composed values stay finite
            for (int i = 3; i < 64; i += 4) buffer[i] = static_cast<std::uint8_t>(0x3C + (buffer[i] & 3));
        }
        CHECK(nnh::fuzz::transform_target(buffer, sizeof buffer) == 0);
    }
}
