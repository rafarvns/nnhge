#pragma once
// A small inflater of zlib streams made only of stored blocks, independent of zlib_stored.cpp: used by the
// tests to check what nnh sends (test_zlib_stored.cpp, the fake netloader in test_console_link.cpp).

#include <cstdint>
#include <vector>

#include "zlib_stored.hpp"

namespace nnh::tool::test {

enum class Inflate { Done, NeedMore, Bad };

/// Decodes `s` into `out`. NeedMore if the stream is valid so far but incomplete.
inline Inflate inflate_stored(const std::vector<std::uint8_t>& s, std::vector<std::uint8_t>& out) {
    out.clear();
    if (s.size() < 2) return Inflate::NeedMore;
    if (s[0] != 0x78 || ((s[0] << 8) | s[1]) % 31 != 0) return Inflate::Bad;
    std::size_t at = 2;
    for (;;) {
        if (at + 5 > s.size()) return Inflate::NeedMore;
        const std::uint8_t header = s[at];
        if ((header & 0x06) != 0) return Inflate::Bad; // only stored blocks
        const unsigned len = s[at + 1] | s[at + 2] << 8;
        const unsigned nlen = s[at + 3] | s[at + 4] << 8;
        if ((len ^ 0xFFFF) != nlen) return Inflate::Bad;
        if (at + 5 + len > s.size()) return Inflate::NeedMore;
        out.insert(out.end(), s.begin() + static_cast<long>(at + 5), s.begin() + static_cast<long>(at + 5 + len));
        at += 5 + len;
        if (header & 1) break;
    }
    if (at + 4 > s.size()) return Inflate::NeedMore;
    if (at + 4 != s.size()) return Inflate::Bad;
    const std::uint32_t stored = std::uint32_t(s[at]) << 24 | std::uint32_t(s[at + 1]) << 16 |
                                 std::uint32_t(s[at + 2]) << 8 | s[at + 3];
    return stored == adler32(out.data(), out.size()) ? Inflate::Done : Inflate::Bad;
}

} // namespace nnh::tool::test
