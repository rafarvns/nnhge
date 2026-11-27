// Mini-fuzz of the reload receiver (spec 031 T1.6, FR-18, AC-6): random raw streams, random well-formed-but-wrong
// sequences, and a valid batch mutated 1-4 bytes at a time, through reload_receiver_target's own invariant checks.

#include <cstdint>
#include <cstring>

#include "core/crc32.hpp"
#include "fuzz_targets.hpp"
#include "reload/reload_wire.hpp"
#include "remote/remote_wire.hpp"
#include "test.hpp"

using namespace nnh::detail::remote;

namespace {

std::uint32_t next(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}

/// A raw-mode input: Hello, then a batch of one put "a/b.txt" = "hello" and one remove, then its Apply.
std::size_t valid_batch(std::uint8_t* out) {
    std::uint8_t stream[256];
    std::size_t n = 0;
    auto add = [&](const Message& m) {
        encode_message(m, stream + n);
        n += kMessageSize;
    };
    auto raw = [&](const void* data, std::size_t size) {
        std::memcpy(stream + n, data, size);
        n += size;
    };
    Message hello;
    hello.type = MessageType::Hello;
    hello.version = kVersion;
    add(hello);
    add(reload_begin_message(1, 2, 7 + 5 + 5));
    add(reload_file_message(0, ReloadOp::Put, 7, 5));
    raw("a/b.txt", 7);
    const std::uint32_t crc = nnh::detail::crc32("hello", 5);
    raw(&crc, 4); // little-endian hosts
    add(reload_chunk_message(0, 5, 0));
    raw("hello", 5);
    add(reload_file_message(1, ReloadOp::Remove, 5, 0));
    raw("c.png", 5);
    add(reload_apply_message(1, 2, 0));
    std::size_t size = 0;
    out[size++] = 0; // raw mode
    for (std::size_t at = 0; at < n;) { // each piece led by its control byte (bytes - 1)
        const std::size_t take = n - at < 64 ? n - at : 64;
        out[size++] = static_cast<std::uint8_t>(take - 1);
        std::memcpy(out + size, stream + at, take);
        size += take;
        at += take;
    }
    return size;
}

} // namespace

NNH_TEST(reload_receiver_target_survives_random_streams) {
    std::uint32_t state = 3131;
    std::uint8_t input[600];
    for (int i = 0; i < 20000; ++i) {
        const std::size_t n = 1 + next(state) % sizeof input;
        for (std::size_t b = 0; b < n; ++b) input[b] = static_cast<std::uint8_t>(next(state) >> 24);
        nnh::fuzz::reload_receiver_target(input, n);
    }
    CHECK(true);
}

NNH_TEST(reload_receiver_target_survives_mutated_valid_batches) {
    std::uint8_t seed[400];
    const std::size_t size = valid_batch(seed);
    CHECK(nnh::fuzz::reload_receiver_target(seed, size) == 0);
    std::uint32_t state = 777;
    std::uint8_t mutant[sizeof seed];
    for (int i = 0; i < 20000; ++i) {
        std::memcpy(mutant, seed, size);
        const int changes = 1 + static_cast<int>(next(state) % 4);
        for (int c = 0; c < changes; ++c) mutant[1 + next(state) % (size - 1)] = static_cast<std::uint8_t>(next(state) >> 24);
        const std::size_t n = next(state) % 8 == 0 ? 1 + next(state) % (size - 1) : size;
        nnh::fuzz::reload_receiver_target(mutant, n);
    }
    CHECK(true);
}
