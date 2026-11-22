#pragma once
// Little-endian reads from a byte range that never go past its end (spec 014: the data validator and apply).
// A read past the end returns 0 and sets `failed`; later reads keep failing. Pure, header-only.

#include <cstddef>
#include <cstdint>

namespace nnh::detail {

struct ByteReader {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    std::size_t at = 0;
    bool failed = false;

    bool has(std::size_t n) const { return !failed && size - at >= n; }

    /// The next `n` bytes (or null, setting `failed`), and moves past them.
    const std::uint8_t* take(std::size_t n) {
        if (!has(n)) {
            failed = true;
            return nullptr;
        }
        const std::uint8_t* p = data + at;
        at += n;
        return p;
    }

    std::uint32_t le(std::size_t n) {
        const std::uint8_t* p = take(n);
        std::uint32_t v = 0;
        for (std::size_t i = 0; p != nullptr && i < n; ++i) v |= static_cast<std::uint32_t>(p[i]) << (8 * i);
        return v;
    }
    std::uint32_t u8() { return le(1); }
    std::uint32_t u16() { return le(2); }
    std::uint32_t u32() { return le(4); }
};

} // namespace nnh::detail
