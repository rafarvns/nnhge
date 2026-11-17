// Deterministic mini-fuzz of every fuzz target, run on every compiler as part of nnh_tests:
// seed inputs plus pseudo-random mutations of them. The real fuzzing (libFuzzer + ASan) runs on Linux.

#include <cstdint>
#include <cstring>

#include "fs/save_format.hpp"
#include "fuzz_targets.hpp"
#include "test.hpp"

namespace {

constexpr int kIterations = 20000;

std::uint32_t next(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u; // LCG: reproducible
    return state;
}

void mutate_and_run(int (*target)(const std::uint8_t*, std::size_t), const std::uint8_t* seed, std::size_t size) {
    std::uint8_t buffer[256];
    std::uint32_t state = 12345;
    for (int i = 0; i < kIterations; ++i) {
        const std::size_t n = size == 0 ? next(state) % 64 : (size + next(state) % 8) % sizeof buffer;
        for (std::size_t b = 0; b < n; ++b) {
            buffer[b] = b < size ? seed[b] : static_cast<std::uint8_t>(next(state));
        }
        const int flips = static_cast<int>(next(state) % 4);
        for (int f = 0; f < flips && n > 0; ++f) buffer[next(state) % n] ^= static_cast<std::uint8_t>(next(state));
        target(buffer, n);
    }
}

} // namespace

NNH_TEST(path_target_survives_mutated_inputs) {
    const char* seeds[] = {"saves/slot1.bin", "../x", "a//b", "sdmc:/x", ""};
    for (const char* s : seeds) {
        mutate_and_run(nnh::fuzz::path_target, reinterpret_cast<const std::uint8_t*>(s), std::strlen(s));
    }
    CHECK(true);
}

NNH_TEST(save_target_survives_mutated_inputs) {
    std::uint8_t file[64] = {};
    const char payload[] = "seed payload";
    const std::size_t header = nnh::detail::build_save_header(file, sizeof payload, 1,
                                                              nnh::detail::crc32(payload, sizeof payload));
    std::memcpy(file + header, payload, sizeof payload);
    mutate_and_run(nnh::fuzz::save_target, file, header + sizeof payload);
    mutate_and_run(nnh::fuzz::save_target, nullptr, 0);
    CHECK(true);
}
