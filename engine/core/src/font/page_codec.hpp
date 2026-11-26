#pragma once
// Glyph page codec (spec 029 D49; plan 029 decision 16): 4-bit coverage two pixels a byte (low nibble first), and
// PackBits — a header byte n: 0..127 copies the next n + 1 bytes, 129..255 repeats the next byte 257 − n times
// (2..128); 128 is never written and refused. The encoder is deterministic: a repeat packet for 3 or more equal bytes,
// literals otherwise. Pure, no engine includes (nnh-assetc encodes, the console decodes). Bounded; never allocates.
// Tests: tests/font/test_page_codec.cpp.

#include <cstddef>
#include <cstdint>

namespace nnh::detail::font {

/// Packs `pixels` levels (0..15, `pixels` even) into pixels / 2 bytes.
void a4_pack(const std::uint8_t* levels, std::size_t pixels, std::uint8_t* out);

/// Unpacks pixels / 2 bytes into one coverage byte per pixel: level × 17 (0, 17, …, 255).
void a4_unpack(const std::uint8_t* packed, std::size_t pixels, std::uint8_t* coverage);

/// Encodes `size` bytes. @return the encoded size; writes only when `out` isn't null and `capacity` suffices.
std::size_t packbits_encode(const std::uint8_t* in, std::size_t size, std::uint8_t* out, std::size_t capacity);

/// Decodes exactly `expected` bytes from exactly `size` encoded bytes. @return false for a malformed stream, one that
/// would write past `expected` or read past `size`, or one that ends with fewer or more bytes.
bool packbits_decode(const std::uint8_t* in, std::size_t size, std::uint8_t* out, std::size_t expected);

} // namespace nnh::detail::font
