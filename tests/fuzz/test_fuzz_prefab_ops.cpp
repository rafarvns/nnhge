// Mini-fuzz of prefabs (spec 021 T5.5): 100 random byte strings of up to 8 KiB run as the self-check's operations on
// the prefab side and the hand-written side (fuzz_prefab_ops.cpp aborts on a difference). The real fuzzing (libFuzzer
// + ASan, fuzz_prefab_ops) runs on Linux.

#include <cstdint>

#include "fuzz_targets.hpp"
#include "test.hpp"

NNH_TEST(prefab_ops_target_survives_random_inputs) {
    static std::uint8_t buffer[8192];
    std::uint32_t state = 2021;
    for (int run = 0; run < 100; ++run) {
        state = state * 1664525u + 1013904223u;
        const std::size_t size = (state >> 8) % sizeof buffer;
        for (std::size_t i = 0; i < size; ++i) {
            state = state * 1664525u + 1013904223u;
            buffer[i] = static_cast<std::uint8_t>(state >> 24);
        }
        CHECK(nnh::fuzz::prefab_ops_target(buffer, size) == 0);
    }
}
