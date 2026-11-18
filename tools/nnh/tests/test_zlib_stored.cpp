// zlib streams of stored blocks (zlib_stored.hpp): checked byte by byte and decoded by the tests' own
// inflater of stored blocks (stored_inflate.hpp), independent of the encoder.

#include <cstring>
#include <vector>

#include "stored_inflate.hpp"
#include "test.hpp"
#include "zlib_stored.hpp"

using nnh::tool::adler32;
using nnh::tool::zlib_stored;

namespace {

bool inflate_stored(const std::vector<std::uint8_t>& s, std::vector<std::uint8_t>& out) {
    return nnh::tool::test::inflate_stored(s, out) == nnh::tool::test::Inflate::Done;
}

} // namespace

NNH_TEST(adler32_matches_known_values) {
    const char* wiki = "Wikipedia";
    CHECK(adler32(reinterpret_cast<const std::uint8_t*>(wiki), 9) == 0x11E60398u);
    CHECK(adler32(nullptr, 0) == 1u);
    // Continuing a checksum gives the same as computing it at once.
    const char* abc = "abcdef";
    const auto* p = reinterpret_cast<const std::uint8_t*>(abc);
    CHECK(adler32(p + 3, 3, adler32(p, 3)) == adler32(p, 6));
}

NNH_TEST(adler32_reduces_modulo_65521_on_long_input) {
    std::vector<std::uint8_t> ff(100000, 0xFF);
    // Computed independently: sum of 0xFF * n and its running sum, both mod 65521.
    std::uint32_t a = 1, b = 0;
    for (std::uint8_t v : ff) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    CHECK(adler32(ff.data(), ff.size()) == (b << 16 | a));
}

NNH_TEST(zlib_stored_of_a_short_input_is_one_final_block) {
    const std::uint8_t data[] = {1, 2, 3};
    const auto s = zlib_stored(data, 3);
    const std::uint8_t expected[] = {0x78, 0x01, 0x01, 0x03, 0x00, 0xFC, 0xFF, 1, 2, 3, 0x00, 0x0D, 0x00, 0x07};
    CHECK(s.size() == sizeof expected && std::memcmp(s.data(), expected, sizeof expected) == 0);
}

NNH_TEST(zlib_stored_of_empty_input_is_valid) {
    const auto s = zlib_stored(nullptr, 0);
    std::vector<std::uint8_t> out;
    CHECK(inflate_stored(s, out) && out.empty());
}

NNH_TEST(zlib_stored_splits_into_64k_blocks_and_round_trips) {
    for (std::size_t size : {std::size_t{65535}, std::size_t{65536}, std::size_t{200000}}) {
        std::vector<std::uint8_t> data(size);
        for (std::size_t i = 0; i < size; ++i) data[i] = static_cast<std::uint8_t>(i * 7 + (i >> 8));
        const auto s = zlib_stored(data.data(), data.size());
        const std::size_t blocks = (size + 65534) / 65535;
        CHECK(s.size() == 2 + blocks * 5 + size + 4);
        std::vector<std::uint8_t> out;
        CHECK(inflate_stored(s, out) && out == data);
    }
}
