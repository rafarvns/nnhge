// Bulk byte helpers of structured saves (spec 027 T10.3, save_bulk.hpp): packed copies, single values and bool
// arrays moved a word at a time give the bytes a byte loop gives, for every length and alignment.

#include <cstdint>
#include <cstring>

#include "saves/save_bulk.hpp"
#include "test.hpp"

namespace {

void fill_pattern(std::uint8_t* b, int n, int seed) {
    for (int i = 0; i < n; ++i) b[i] = static_cast<std::uint8_t>(seed + i * 37);
}

} // namespace

NNH_TEST(save_bulk_copy_bytes_matches_memcpy_for_every_length_and_alignment) {
    std::uint8_t src[64];
    fill_pattern(src, 64, 1);
    bool same = true;
    for (int s = 0; s < 4; ++s) {
        for (int d = 0; d < 4; ++d) {
            for (int n = 0; n <= 40; ++n) {
                std::uint8_t got[64], want[64];
                std::memset(got, 0xEE, sizeof got);
                std::memset(want, 0xEE, sizeof want);
                std::memcpy(want + d, src + s, static_cast<std::size_t>(n));
                nnh::detail::copy_bytes(got + d, src + s, static_cast<std::size_t>(n));
                same = same && std::memcmp(got, want, sizeof got) == 0;
            }
        }
    }
    CHECK(same);
}

NNH_TEST(save_bulk_copy_value_moves_one_value_of_each_width) {
    const std::uint8_t src[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    for (int width : {1, 2, 4, 8}) {
        std::uint8_t got[11];
        std::memset(got, 0xEE, sizeof got);
        nnh::detail::copy_value(got + 1, src + 1, width);
        CHECK(std::memcmp(got + 1, src + 1, static_cast<std::size_t>(width)) == 0);
        CHECK(got[0] == 0xEE && got[1 + width] == 0xEE);
    }
}

NNH_TEST(save_bulk_values_load_and_store_little_endian_by_width) {
    const std::uint8_t src[9] = {0x88, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xF8};
    CHECK(nnh::detail::load_value(src, 1) == 0x88u);
    CHECK(nnh::detail::load_value(src, 2) == 0x0188u);
    CHECK(nnh::detail::load_value(src + 1, 4) == 0x04030201u);
    CHECK(nnh::detail::load_value(src + 1, 8) == 0xF807060504030201ull);
    for (int width : {1, 2, 4, 8}) {
        std::uint8_t got[10];
        std::memset(got, 0xEE, sizeof got);
        nnh::detail::store_value(got + 1, 0xF807060504030201ull, width);
        CHECK(std::memcmp(got + 1, src + 1, static_cast<std::size_t>(width)) == 0);
        CHECK(got[0] == 0xEE && got[1 + width] == 0xEE);
    }
}

NNH_TEST(save_bulk_put_bools_writes_any_nonzero_byte_as_one) {
    std::uint8_t src[259];
    for (int i = 0; i < 256; ++i) src[i] = static_cast<std::uint8_t>(i);
    src[256] = 0x80;
    src[257] = 0;
    src[258] = 0x01;
    bool right = true;
    for (int at = 0; at < 4; ++at) { // every byte value in every position of a word, and short tails
        const int n = 259 - at;
        std::uint8_t got[260];
        std::memset(got, 0xEE, sizeof got);
        nnh::detail::put_bools(got, src + at, static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) right = right && got[i] == (src[at + i] != 0 ? 1 : 0);
        right = right && got[n] == 0xEE;
    }
    CHECK(right);
}

NNH_TEST(save_bulk_copy_bools_copies_bytes_of_zero_and_one) {
    const std::uint8_t src[13] = {0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1};
    std::uint8_t got[14];
    std::memset(got, 0xEE, sizeof got);
    CHECK(nnh::detail::copy_bools(got, src, 13) == 13);
    CHECK(std::memcmp(got, src, 13) == 0 && got[13] == 0xEE);
}

NNH_TEST(save_bulk_copy_bools_stops_at_the_first_byte_above_one) {
    bool right = true;
    for (int pos = 0; pos < 13; ++pos) {
        for (std::uint8_t bad : {std::uint8_t{2}, std::uint8_t{0x10}, std::uint8_t{0x80}, std::uint8_t{0xFF}}) {
            std::uint8_t src[13] = {0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1};
            src[pos] = bad;
            std::uint8_t got[13];
            std::memset(got, 0xEE, sizeof got);
            right = right && nnh::detail::copy_bools(got, src, 13) == static_cast<std::size_t>(pos);
            for (int i = 0; i < 13; ++i) right = right && got[i] == (i < pos ? src[i] : 0xEE);
        }
    }
    CHECK(right);
}
