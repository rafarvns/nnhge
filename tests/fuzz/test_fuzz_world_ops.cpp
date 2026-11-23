// Mini-fuzz of worlds (spec 020 T4.4): 200 random byte strings of up to 4 KiB run as operations on a world and the
// reference model (fuzz_world_ops.cpp aborts on a difference). The real fuzzing (libFuzzer + ASan, fuzz_world_ops)
// runs on Linux.

#include <cstdint>

#include "fuzz_targets.hpp"
#include "test.hpp"

NNH_TEST(world_ops_target_survives_random_inputs) {
    static std::uint8_t buffer[4096];
    std::uint32_t state = 2020;
    for (int run = 0; run < 200; ++run) {
        state = state * 1664525u + 1013904223u;
        const std::size_t size = (state >> 8) % sizeof buffer;
        for (std::size_t i = 0; i < size; ++i) {
            state = state * 1664525u + 1013904223u;
            buffer[i] = static_cast<std::uint8_t>(state >> 24);
        }
        CHECK(nnh::fuzz::world_ops_target(buffer, size) == 0);
    }
}
