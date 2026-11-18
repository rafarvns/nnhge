#pragma once
// Helpers for the audio tests that run the engine on the fake platform (spec 008).

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "nnh/app.hpp"
#include "nnh/platform_fake.hpp"

namespace audio_test {

/// Writes a PCM WAV file into `out` (capacity checked by the caller): `frames` frames whose values are
/// `value(i)` for value index i (interleaved). @return its size.
template <typename Value>
std::size_t make_wav(std::uint8_t* out, int channels, int bits, std::uint32_t rate, std::uint32_t frames, Value value) {
    const std::uint32_t bytes_per_value = static_cast<std::uint32_t>(bits / 8);
    const std::uint32_t data = frames * static_cast<std::uint32_t>(channels) * bytes_per_value;
    auto put32 = [&](std::size_t at, std::uint32_t v) {
        for (int i = 0; i < 4; ++i) out[at + i] = static_cast<std::uint8_t>(v >> (8 * i));
    };
    auto put16 = [&](std::size_t at, std::uint32_t v) {
        out[at] = static_cast<std::uint8_t>(v);
        out[at + 1] = static_cast<std::uint8_t>(v >> 8);
    };
    std::memcpy(out, "RIFF", 4);
    put32(4, 36 + data);
    std::memcpy(out + 8, "WAVEfmt ", 8);
    put32(16, 16);
    put16(20, 1);
    put16(22, static_cast<std::uint32_t>(channels));
    put32(24, rate);
    put32(28, rate * static_cast<std::uint32_t>(channels) * bytes_per_value);
    put16(32, static_cast<std::uint32_t>(channels) * bytes_per_value);
    put16(34, static_cast<std::uint32_t>(bits));
    std::memcpy(out + 36, "data", 4);
    put32(40, data);
    for (std::uint32_t i = 0; i < frames * static_cast<std::uint32_t>(channels); ++i) {
        const int v = value(i);
        if (bits == 8) {
            out[44 + i] = static_cast<std::uint8_t>(v);
        } else {
            put16(44 + 2 * i, static_cast<std::uint32_t>(static_cast<std::uint16_t>(static_cast<std::int16_t>(v))));
        }
    }
    return 44 + data;
}

/// Reads tests/data/sounds/<name> into `out`. @return its size (0 on failure).
inline std::size_t load_fixture(const char* name, std::uint8_t* out, std::size_t capacity) {
    char path[512];
    std::snprintf(path, sizeof path, "%s/sounds/%s", NNH_TEST_DATA_DIR, name);
    std::FILE* f = std::fopen(path, "rb");
    if (f == nullptr) return 0;
    const std::size_t size = std::fread(out, 1, capacity, f);
    std::fclose(f);
    return size < capacity ? size : 0;
}

} // namespace audio_test
