// Mini-fuzz of the transition effect geometry (spec 023 T3.5): random bytes and a few hand-made extremes (NaN and
// infinite coverage and centres, enum bytes out of range, huge gaps). The real fuzzing (libFuzzer + ASan,
// fuzz_transition_geometry) runs on Linux.

#include <cstdint>
#include <cstring>
#include <limits>

#include "fuzz_transition_geometry.hpp"
#include "test.hpp"

NNH_TEST(transition_geometry_target_survives_random_inputs) {
    static std::uint8_t buffer[26 * 64];
    std::uint32_t state = 2023;
    for (int run = 0; run < 300; ++run) {
        state = state * 1664525u + 1013904223u;
        const std::size_t size = (state >> 8) % sizeof buffer;
        for (std::size_t i = 0; i < size; ++i) {
            state = state * 1664525u + 1013904223u;
            buffer[i] = static_cast<std::uint8_t>(state >> 24);
        }
        CHECK(nnh::fuzz::transition_geometry_target(buffer, size) == 0);
    }
}

NNH_TEST(transition_geometry_target_survives_extremes) {
    const float specials[] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity(), 1e30f, -1e30f, 0.5f, 4096.5f, -4097.0f};
    std::uint8_t record[26] = {};
    for (std::uint8_t kind = 0; kind < 10; ++kind) {
        for (float a : specials) {
            for (float c : specials) {
                record[0] = kind;
                record[5] = static_cast<std::uint8_t>(kind * 37);
                record[6] = static_cast<std::uint8_t>(kind * 11);
                record[7] = static_cast<std::uint8_t>(kind * 29);
                record[8] = kind;
                record[9] = static_cast<std::uint8_t>(kind * 53);
                std::memcpy(record + 10, &a, 4);
                std::memcpy(record + 14, &c, 4);
                record[18] = static_cast<std::uint8_t>(kind & 3);
                std::memcpy(record + 19, &c, 4);
                record[23] = static_cast<std::uint8_t>(kind & 3);
                record[24] = 0xFF;
                record[25] = static_cast<std::uint8_t>(kind * 13);
                CHECK(nnh::fuzz::transition_geometry_target(record, sizeof record) == 0);
            }
        }
    }
}
