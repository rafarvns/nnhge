// Glyph page codec (page_codec.hpp).

#include "font/page_codec.hpp"

namespace nnh::detail::font {
namespace {

/// Equal bytes from `at`, at most 128.
std::size_t run_at(const std::uint8_t* in, std::size_t size, std::size_t at) {
    std::size_t n = 1;
    while (at + n < size && n < 128 && in[at + n] == in[at]) ++n;
    return n;
}

} // namespace

void a4_pack(const std::uint8_t* levels, std::size_t pixels, std::uint8_t* out) {
    for (std::size_t i = 0; i + 1 < pixels; i += 2) {
        out[i / 2] = static_cast<std::uint8_t>((levels[i] & 15u) | ((levels[i + 1] & 15u) << 4));
    }
}

void a4_unpack(const std::uint8_t* packed, std::size_t pixels, std::uint8_t* coverage) {
    for (std::size_t i = 0; i + 1 < pixels; i += 2) {
        const std::uint8_t b = packed[i / 2];
        coverage[i] = static_cast<std::uint8_t>((b & 15u) * 17u);
        coverage[i + 1] = static_cast<std::uint8_t>((b >> 4) * 17u);
    }
}

std::size_t packbits_encode(const std::uint8_t* in, std::size_t size, std::uint8_t* out, std::size_t capacity) {
    if (out != nullptr && packbits_encode(in, size, nullptr, 0) > capacity) out = nullptr; // measure only
    std::size_t at = 0, written = 0;
    auto put = [&](std::uint8_t b) {
        if (out != nullptr) out[written] = b;
        ++written;
    };
    while (at < size) {
        const std::size_t run = run_at(in, size, at);
        if (run >= 3) {
            put(static_cast<std::uint8_t>(257 - run));
            put(in[at]);
            at += run;
            continue;
        }
        std::size_t n = 0; // a literal: up to 128 bytes, stopping before a run of 3
        while (at + n < size && n < 128 && run_at(in, size, at + n) < 3) ++n;
        put(static_cast<std::uint8_t>(n - 1));
        for (std::size_t i = 0; i < n; ++i) put(in[at + i]);
        at += n;
    }
    return written;
}

bool packbits_decode(const std::uint8_t* in, std::size_t size, std::uint8_t* out, std::size_t expected) {
    std::size_t at = 0, written = 0;
    while (at < size) {
        const std::uint8_t h = in[at++];
        if (h < 128) {
            const std::size_t n = std::size_t{h} + 1;
            if (n > size - at || n > expected - written) return false;
            for (std::size_t i = 0; i < n; ++i) out[written++] = in[at++];
        } else if (h > 128) {
            const std::size_t n = 257 - std::size_t{h};
            if (at >= size || n > expected - written) return false;
            const std::uint8_t b = in[at++];
            for (std::size_t i = 0; i < n; ++i) out[written++] = b;
        } else {
            return false;
        }
    }
    return written == expected;
}

} // namespace nnh::detail::font
