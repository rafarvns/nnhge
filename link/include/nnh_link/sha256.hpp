#pragma once
// SHA-256 (FIPS 180-4) and HMAC-SHA-256 (RFC 2104) for NNH Link's authentication and file checks
// (spec 011, decision D24). Own implementation, checked against the published test vectors in
// link/tests/test_sha256.cpp.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace nnh::link {

using Digest = std::array<std::uint8_t, 32>;

class Sha256 {
public:
    Sha256();
    void update(const void* data, std::size_t size);
    /// The digest of everything given; the object must not be used afterwards.
    Digest finish();

private:
    void block(const std::uint8_t* p);
    std::uint32_t state_[8];
    std::uint64_t length_ = 0; ///< bytes so far
    std::uint8_t buffer_[64];
    std::size_t used_ = 0;
};

Digest sha256(const void* data, std::size_t size);
Digest hmac_sha256(const void* key, std::size_t key_size, const void* data, std::size_t size);

/// Compares without an early exit, so the time doesn't reveal where two MACs differ.
bool equal_constant_time(const std::uint8_t* a, const std::uint8_t* b, std::size_t size);

/// Lower-case hex; and back (false unless `text` is exactly 2*size hex digits).
std::string to_hex(const std::uint8_t* data, std::size_t size);
bool from_hex(const std::string& text, std::uint8_t* out, std::size_t size);

} // namespace nnh::link
