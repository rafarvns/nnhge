// Glyph page codec (spec 029 D49, plan 029 decision 16): 4-bit coverage two pixels a byte, PackBits with exact sizes,
// every malformed stream refused.

#include <cstdint>
#include <cstring>

#include "font/page_codec.hpp"
#include "test.hpp"

using namespace nnh::detail::font;

namespace {

bool round_trips(const std::uint8_t* in, std::size_t n, std::size_t want_encoded) {
    std::uint8_t enc[1024];
    std::uint8_t dec[1024];
    const std::size_t size = packbits_encode(in, n, enc, sizeof enc);
    if (size != want_encoded || size > sizeof enc) return false;
    return packbits_decode(enc, size, dec, n) && std::memcmp(dec, in, n) == 0;
}

} // namespace

NNH_TEST(a4_packs_low_nibble_first_and_unpacks_to_multiples_of_17) {
    const std::uint8_t levels[4] = {1, 15, 0, 7};
    std::uint8_t packed[2] = {};
    a4_pack(levels, 4, packed);
    CHECK(packed[0] == 0xF1 && packed[1] == 0x70);
    std::uint8_t coverage[4] = {};
    a4_unpack(packed, 4, coverage);
    CHECK(coverage[0] == 17 && coverage[1] == 255 && coverage[2] == 0 && coverage[3] == 119);
}

NNH_TEST(a4_every_level_round_trips) {
    std::uint8_t levels[16], packed[8], coverage[16];
    for (int i = 0; i < 16; ++i) levels[i] = static_cast<std::uint8_t>(i);
    a4_pack(levels, 16, packed);
    a4_unpack(packed, 16, coverage);
    bool ok = true;
    for (int i = 0; i < 16; ++i) ok = ok && coverage[i] == i * 17;
    CHECK(ok);
}

NNH_TEST(packbits_runs_of_1_to_128_have_exact_sizes) {
    std::uint8_t in[128];
    std::memset(in, 0x5A, sizeof in);
    bool ok = true;
    for (std::size_t n = 1; n <= 128; ++n) ok = ok && round_trips(in, n, n >= 3 ? 2 : 1 + n);
    CHECK(ok);
    std::uint8_t long_run[300];
    std::memset(long_run, 0, sizeof long_run);
    CHECK(round_trips(long_run, 300, 6)); // 128 + 128 + 44
    CHECK(round_trips(long_run, 129, 4)); // a run of 128 (2 bytes), then a literal of 1 (2 bytes)
}

NNH_TEST(packbits_literals_of_1_to_128_have_exact_sizes) {
    std::uint8_t in[260];
    for (int i = 0; i < 260; ++i) in[i] = static_cast<std::uint8_t>(i * 7 + 1); // no two neighbours equal
    bool ok = true;
    for (std::size_t n = 1; n <= 128; ++n) ok = ok && round_trips(in, n, n + 1);
    CHECK(ok);
    CHECK(round_trips(in, 129, 131)); // two literal packets
    CHECK(round_trips(in, 260, 263));
}

NNH_TEST(packbits_pairs_stay_in_literals_and_triples_become_runs) {
    const std::uint8_t pair[] = {1, 2, 2, 3};
    CHECK(round_trips(pair, 4, 5));
    const std::uint8_t triple[] = {1, 2, 2, 2, 3};
    CHECK(round_trips(triple, 5, 6)); // literal 1, run 3, literal 1
    std::uint8_t enc[8];
    CHECK(packbits_encode(triple, 5, enc, sizeof enc) == 6);
    CHECK(enc[0] == 0 && enc[1] == 1 && enc[2] == 254 && enc[3] == 2 && enc[4] == 0 && enc[5] == 3);
}

NNH_TEST(packbits_refuses_malformed_streams) {
    std::uint8_t out[8];
    const std::uint8_t noop[] = {128, 1};
    CHECK(!packbits_decode(noop, 2, out, 1));
    const std::uint8_t short_literal[] = {3, 1, 2};
    CHECK(!packbits_decode(short_literal, 3, out, 4));
    const std::uint8_t no_byte[] = {255};
    CHECK(!packbits_decode(no_byte, 1, out, 2));
    const std::uint8_t too_long[] = {251, 9}; // 6 bytes into 4
    CHECK(!packbits_decode(too_long, 2, out, 4));
    const std::uint8_t too_short[] = {253, 9}; // 4 bytes, 5 expected
    CHECK(!packbits_decode(too_short, 2, out, 5));
    const std::uint8_t exact[] = {253, 9};
    CHECK(packbits_decode(exact, 2, out, 4) && out[0] == 9 && out[3] == 9);
    CHECK(packbits_decode(nullptr, 0, out, 0));
}

NNH_TEST(packbits_encode_only_measures_without_room) {
    std::uint8_t in[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    CHECK(packbits_encode(in, 10, nullptr, 0) == 11);
    std::uint8_t small[4] = {0xEE, 0xEE, 0xEE, 0xEE};
    CHECK(packbits_encode(in, 10, small, 4) == 11);
    CHECK(small[0] == 0xEE && small[3] == 0xEE); // nothing written when it doesn't fit
}
