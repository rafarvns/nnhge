// CRC-32 (IEEE 802.3): known vectors. Pure code shared by saves, maps and nnh-assetc (spec 012).

#include <cstdint>

#include "core/crc32.hpp"
#include "test.hpp"

using nnh::detail::crc32;
using nnh::detail::crc32_update;

NNH_TEST(crc32_update_continues_a_checksum_in_pieces) {
    // Like zlib's crc32(crc, buf, len): start from 0, feed pieces, get the one-shot value.
    std::uint32_t crc = crc32_update(0, "1234", 4);
    crc = crc32_update(crc, "", 0);
    crc = crc32_update(crc, "56789", 5);
    CHECK(crc == 0xCBF43926u);
    CHECK(crc32_update(0, "a", 1) == crc32("a", 1));
    CHECK(crc32_update(0, nullptr, 0) == 0u);
}

NNH_TEST(crc32_matches_the_standard_check_value) {
    CHECK(crc32("123456789", 9) == 0xCBF43926u);
}

NNH_TEST(crc32_of_nothing_is_zero) {
    CHECK(crc32("", 0) == 0u);
    CHECK(crc32(nullptr, 0) == 0u);
}

NNH_TEST(crc32_matches_known_vectors) {
    CHECK(crc32("a", 1) == 0xE8B7BE43u);
    CHECK(crc32("The quick brown fox jumps over the lazy dog", 43) == 0x414FA339u);
    const std::uint8_t zeros[32] = {};
    CHECK(crc32(zeros, sizeof zeros) == 0x190A55ADu);
    std::uint8_t ones[32];
    for (auto& b : ones) b = 0xFF;
    CHECK(crc32(ones, sizeof ones) == 0xFF6CAB0Bu);
}
