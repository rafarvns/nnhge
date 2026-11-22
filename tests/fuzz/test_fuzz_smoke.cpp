// Deterministic mini-fuzz of every fuzz target, run on every compiler as part of nnh_tests:
// seed inputs plus pseudo-random mutations of them. The real fuzzing (libFuzzer + ASan) runs on Linux.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../tilemap/map_file.hpp"
#include "audio/wav_reader.hpp"
#include "fs/save_format.hpp"
#include "fuzz_targets.hpp"
#include "nnh/memory.hpp"
#include "remote/remote_wire.hpp"
#include "test.hpp"
#include "tilemap/map_validate.hpp"
#include "tilemap/tilemap_internal.hpp"

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

NNH_TEST(remote_target_survives_mutated_inputs) {
    // A valid session (control byte: 16 bytes arrive, then bits 6-7 frames pass) to mutate from.
    using namespace nnh::detail::remote;
    Message messages[7];
    messages[0].type = MessageType::Hello;
    messages[0].version = kVersion;
    messages[1].type = MessageType::Buttons;
    messages[1].frame = 2;
    messages[1].press = 0x0101;
    messages[2].type = MessageType::Stick;
    messages[2].frame = 3;
    messages[2].x = -700;
    messages[2].y = 1000;
    messages[3].type = MessageType::Touch;
    messages[3].frame = 3;
    messages[3].down = true;
    messages[3].x = 319;
    messages[3].y = 239;
    messages[4].type = MessageType::Screenshot;
    messages[4].frame = 0;
    messages[4].screens = 3;
    messages[5].type = MessageType::Buttons;
    messages[5].frame = 9;
    messages[5].release = 0x0001;
    messages[6].type = MessageType::Bye;
    messages[6].frame = 12;
    std::uint8_t seed[7 * (1 + kMessageSize)];
    std::size_t size = 0;
    for (const Message& m : messages) {
        seed[size++] = static_cast<std::uint8_t>(kMessageSize | (1u << 6));
        encode_message(m, seed + size);
        size += kMessageSize;
    }
    mutate_and_run(nnh::fuzz::remote_target, seed, size);
    mutate_and_run(nnh::fuzz::remote_target, nullptr, 0);
    CHECK(true);
}

NNH_TEST(tilemap_target_survives_mutated_inputs) {
    // The sample map (every section used), the same with animated tiles, and the minimal one, with 1-4 bytes
    // changed and sometimes cut. The target fixes the size and CRC itself, so mutants reach the content checks;
    // some are even accepted and then copied, queried, drawn and edited (counted here to prove the deep path runs,
    // animated tiles included).
    const map_file::Bytes seeds[] = {map_file::sample(), map_file::animated(), map_file::minimal()};
    std::uint32_t state = 4242;
    int accepted = 0;
    int animated = 0;
    for (const map_file::Bytes& seed : seeds) {
        CHECK(nnh::detail::validate_map(seed.data(), seed.size()).ok());
        nnh::fuzz::tilemap_target(seed.data(), seed.size());
        for (int i = 0; i < 4000; ++i) {
            map_file::Bytes buffer = seed;
            if (i % 8 == 0) buffer.resize(next(state) % (seed.size() + 1));
            const int flips = 1 + static_cast<int>(next(state) % 4);
            for (int k = 0; k < flips && !buffer.empty(); ++k) {
                buffer[next(state) % buffer.size()] ^= static_cast<std::uint8_t>(1 + next(state) % 255);
            }
            nnh::fuzz::tilemap_target(buffer.data(), buffer.size());
            if (buffer.size() < sizeof(nnh::detail::mapfmt::Header)) continue;
            map_file::seal(buffer);
            if (!nnh::detail::validate_map(buffer.data(), buffer.size()).ok()) continue;
            ++accepted;
            animated += map_file::view(buffer).header().animations > 0 ? 1 : 0;
        }
    }
    CHECK(accepted > 100);
    CHECK(animated > 30);
    CHECK(nnh::mem::tag_stats(nnh::detail::tilemap_memory_tag()).count == 0); // every map was freed
}

NNH_TEST(data_target_survives_mutated_inputs) {
    // A file with every field kind set, with 1-4 bytes changed and sometimes cut. The target fixes the header itself,
    // so mutants reach the body; many are accepted and then applied and checked (counted here to prove the deep path
    // runs). Random short inputs too.
    std::uint8_t seed[512];
    const std::size_t size = nnh::fuzz::data_seed(seed, sizeof seed);
    CHECK(size > 100);
    CHECK(nnh::fuzz::data_fixed_accepted(seed, size));
    nnh::fuzz::data_target(seed, size);
    std::uint32_t state = 777;
    int accepted = 0;
    std::uint8_t buffer[512];
    for (int i = 0; i < 6000; ++i) {
        std::size_t n = size;
        std::memcpy(buffer, seed, size);
        if (i % 8 == 0) n = next(state) % (size + 1);
        const int flips = 1 + static_cast<int>(next(state) % 4);
        for (int k = 0; k < flips && n > 0; ++k) {
            buffer[next(state) % n] ^= static_cast<std::uint8_t>(1 + next(state) % 255);
        }
        nnh::fuzz::data_target(buffer, n);
        accepted += nnh::fuzz::data_fixed_accepted(buffer, n) ? 1 : 0;
    }
    CHECK(accepted > 100);
    mutate_and_run(nnh::fuzz::data_target, nullptr, 0);
}

NNH_TEST(data_index_target_survives_mutated_inputs) {
    // An index with two types and every group, with 1-4 bytes changed and sometimes cut or grown. The target fixes
    // the size and CRC itself, so mutants reach the sections; the accepted ones run every lookup (counted here).
    std::uint8_t seed[512];
    const std::size_t size = nnh::fuzz::data_index_seed(seed, sizeof seed);
    CHECK(size > 100);
    CHECK(nnh::fuzz::data_index_fixed_accepted(seed, size));
    std::uint32_t state = 4321;
    int accepted = 0;
    std::uint8_t buffer[sizeof seed];
    for (int i = 0; i < 6000; ++i) {
        std::size_t n = size;
        std::memcpy(buffer, seed, size);
        if (i % 8 == 0) n = next(state) % (size + 1);
        if (i % 8 == 1) n = size + next(state) % 8;
        const int flips = 1 + static_cast<int>(next(state) % 4);
        for (int k = 0; k < flips && n > 0; ++k) {
            buffer[next(state) % n] ^= static_cast<std::uint8_t>(1 + next(state) % 255);
        }
        nnh::fuzz::data_index_target(buffer, n);
        accepted += nnh::fuzz::data_index_fixed_accepted(buffer, n) ? 1 : 0;
    }
    CHECK(accepted > 20);
    mutate_and_run(nnh::fuzz::data_index_target, nullptr, 0);
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
