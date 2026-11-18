#include "zlib_stored.hpp"

namespace nnh::tool {

std::uint32_t adler32(const std::uint8_t* data, std::size_t size, std::uint32_t adler) {
    constexpr std::uint32_t kMod = 65521;
    std::uint32_t a = adler & 0xFFFF;
    std::uint32_t b = adler >> 16;
    while (size > 0) {
        // 5552 bytes keep b below 2^32 before the reduction (zlib's NMAX).
        const std::size_t n = size < 5552 ? size : 5552;
        for (std::size_t i = 0; i < n; ++i) {
            a += data[i];
            b += a;
        }
        a %= kMod;
        b %= kMod;
        data += n;
        size -= n;
    }
    return b << 16 | a;
}

std::vector<std::uint8_t> zlib_stored(const std::uint8_t* data, std::size_t size) {
    constexpr std::size_t kBlock = 65535;
    std::vector<std::uint8_t> out;
    out.reserve(size + size / kBlock * 5 + 11);
    out.push_back(0x78); // deflate, 32 KB window
    out.push_back(0x01); // no dictionary, fastest; 0x7801 is a multiple of 31
    std::size_t at = 0;
    do {
        const std::size_t n = size - at < kBlock ? size - at : kBlock;
        const bool last = at + n == size;
        out.push_back(last ? 0x01 : 0x00); // BFINAL, BTYPE 00 (stored); the rest of the byte is padding
        out.push_back(static_cast<std::uint8_t>(n));
        out.push_back(static_cast<std::uint8_t>(n >> 8));
        out.push_back(static_cast<std::uint8_t>(~n));
        out.push_back(static_cast<std::uint8_t>(~n >> 8));
        out.insert(out.end(), data + at, data + at + n);
        at += n;
    } while (at < size);
    const std::uint32_t check = adler32(data, size);
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(check >> shift));
    return out;
}

} // namespace nnh::tool
