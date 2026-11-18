// Deterministic mini-fuzz of every fuzz target, run on every compiler as part of nnh_tests:
// seed inputs plus pseudo-random mutations of them. The real fuzzing (libFuzzer + ASan) runs on Linux.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "audio/wav_reader.hpp"
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

NNH_TEST(asset_index_target_survives_mutated_inputs) {
    const char* seeds[] = {"art/hero.png\nlevels/level1.txt\n", "a\r\nb\r\n", "../x\n", "a//b\nc", ""};
    for (const char* s : seeds) {
        mutate_and_run(nnh::fuzz::asset_index_target, reinterpret_cast<const std::uint8_t*>(s), std::strlen(s));
    }
    CHECK(true);
}

NNH_TEST(wav_target_survives_mutated_inputs) {
    // 16-bit stereo, 32 kHz, 4 frames.
    const std::uint8_t wav[] = {'R', 'I', 'F', 'F', 52, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0,
                                1, 0, 2, 0, 0, 0x7D, 0, 0, 0, 0xF4, 1, 0, 4, 0, 16, 0, 'd', 'a', 't', 'a', 16, 0, 0, 0,
                                1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    CHECK(nnh::detail::parse_wav(wav, sizeof wav, sizeof wav).ok()); // the seed itself is valid
    mutate_and_run(nnh::fuzz::wav_target, wav, sizeof wav);
    mutate_and_run(nnh::fuzz::wav_target, nullptr, 0);
}

NNH_TEST(ogg_target_survives_mutated_inputs) {
    // The real fixture, with a few bytes flipped and a random cut; fewer runs (each one decodes).
    static std::uint8_t file[16 * 1024];
    static std::uint8_t buffer[sizeof file];
    char path[512];
    std::snprintf(path, sizeof path, "%s/sounds/sweep_stereo_32k_loop.ogg", NNH_TEST_DATA_DIR);
    std::FILE* f = std::fopen(path, "rb");
    CHECK(f != nullptr);
    if (f == nullptr) return;
    const std::size_t size = std::fread(file, 1, sizeof file, f);
    std::fclose(f);
    CHECK(size > 0 && size < sizeof file);
    nnh::fuzz::ogg_target(file, size);
    std::uint32_t state = 777;
    for (int i = 0; i < 300; ++i) {
        std::memcpy(buffer, file, size);
        const std::size_t n = i % 3 == 0 ? next(state) % size : size;
        const int flips = 1 + static_cast<int>(next(state) % 4);
        for (int k = 0; k < flips; ++k) buffer[next(state) % size] ^= static_cast<std::uint8_t>(1 + next(state) % 255);
        nnh::fuzz::ogg_target(buffer, n);
    }
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
