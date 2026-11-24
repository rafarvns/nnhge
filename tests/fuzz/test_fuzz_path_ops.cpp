// Mini-fuzz of pathfinding (spec 025 T6.3): 200 random byte strings of up to 4 KiB run as map settings, cells and an
// operation stream (fuzz_path_ops.cpp aborts on an invalid path, a sliced search unlike find(), a broken waypoint or
// field answer). The real fuzzing (libFuzzer + ASan, fuzz_path_ops) runs on Linux.

#include <cstdint>

#include "fuzz_path_ops.hpp"
#include "test.hpp"

NNH_TEST(path_ops_target_survives_random_inputs) {
    static std::uint8_t buffer[4096];
    std::uint32_t state = 2025;
    for (int run = 0; run < 200; ++run) {
        state = state * 1664525u + 1013904223u;
        const std::size_t size = (state >> 8) % sizeof buffer;
        for (std::size_t i = 0; i < size; ++i) {
            state = state * 1664525u + 1013904223u;
            buffer[i] = static_cast<std::uint8_t>(state >> 24);
        }
        CHECK(nnh::fuzz::path_ops_target(buffer, size) == 0);
    }
}
