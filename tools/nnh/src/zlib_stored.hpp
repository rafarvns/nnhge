#pragma once
// A zlib stream (RFC 1950) holding data in stored (uncompressed) deflate blocks (RFC 1951 §3.2.4): what
// the Homebrew Launcher's netloader inflates, without a compression library (spec 010, decision D23).
// Pure; unit tested in tools/nnh/tests/test_zlib_stored.cpp.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nnh::tool {

/// Adler-32 of `size` bytes, continuing from `adler` (1 for a new checksum).
std::uint32_t adler32(const std::uint8_t* data, std::size_t size, std::uint32_t adler = 1);

/// The whole input as a zlib stream: header 78 01, stored blocks of at most 65 535 bytes (the last one
/// marked final; one empty final block for empty input), then the big-endian Adler-32.
std::vector<std::uint8_t> zlib_stored(const std::uint8_t* data, std::size_t size);

} // namespace nnh::tool
