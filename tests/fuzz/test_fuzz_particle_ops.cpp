// Mini-fuzz of particles (spec 024 T5.5): 200 random byte strings of up to 4 KiB run as a description and an operation
// stream (fuzz_particle_ops.cpp aborts on alive > capacity, a position outside the bounds or an over-full expansion).
// The real fuzzing (libFuzzer + ASan/UBSan, fuzz_particle_ops) runs on Linux.

#include <cstdint>

#include "fuzz_particle_ops.hpp"
#include "test.hpp"

NNH_TEST(particle_ops_target_survives_random_inputs) {
    static std::uint8_t buffer[4096];
    std::uint32_t state = 2024;
    for (int run = 0; run < 200; ++run) {
        state = state * 1664525u + 1013904223u;
        const std::size_t size = (state >> 8) % sizeof buffer;
        for (std::size_t i = 0; i < size; ++i) {
            state = state * 1664525u + 1013904223u;
            buffer[i] = static_cast<std::uint8_t>(state >> 24);
        }
        CHECK(nnh::fuzz::particle_ops_target(buffer, size) == 0);
    }
}

// libFuzzer finding (stage 9, crash-8f2b7aa6…): lifetime 1 tick + world-space rate births pre-aged past it, then
// particle() read the colour curve out of bounds (heap-buffer-overflow under ASan).
NNH_TEST(particle_ops_target_survives_pre_age_past_lifetime_crash) {
    static const std::uint8_t crash[] = {
        0x01, 0x0a, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x3a, 0x47, 0x29, 0x91, 0x91, 0x91, 0x91,
        0x91, 0xff, 0xff, 0xff, 0xe3, 0x91, 0x91, 0x91, 0x00, 0x00, 0x00, 0x47, 0x47, 0x47, 0x84, 0x00,
        0x00, 0x00, 0x00, 0x10, 0x40, 0x47, 0x47, 0x91, 0x91, 0x91, 0x91, 0x47, 0x47, 0x47, 0x0a, 0xd7};
    CHECK(nnh::fuzz::particle_ops_target(crash, sizeof crash) == 0);
}
