// Encoding one-value kinds and nested structs (spec 027 T3.1, FR-3, FR-4, FR-5, FR-8, FR-21): every kind's bytes
// written by hand (little-endian, no padding, ascending ids whatever the declaration order), bools as 0/1, strings at
// their length, the overhead per field, a short buffer, a string that isn't UTF-8.

#include <cstring>

#include "../actions/actions_fixture.hpp"
#include "saves_fixture.hpp"

using saves_test::Bytes;
using saves_test::encoded;
using saves_test::header;
using saves_test::operator+;
using saves_test::Kinds;

namespace {

struct Ids {
    std::uint8_t a = 1;
    std::uint8_t b = 2;
    std::uint8_t c = 3;
};
NNH_SAVE_TYPE(Ids, "ids", NNH_SAVE_FIELD(c, 16384), NNH_SAVE_FIELD(b, 128), NNH_SAVE_FIELD(a, 127));

struct Deep {
    saves_test::Pair pair;
    std::uint32_t tail = 0;
};
NNH_SAVE_STRUCT(Deep, NNH_SAVE_FIELD(tail, 2), NNH_SAVE_FIELD(pair, 1));
struct Deeper {
    Deep deep;
};
NNH_SAVE_TYPE(Deeper, "deeper", NNH_SAVE_FIELD(deep, 3));

struct Text {
    nnh::data::String<8> name;
};
NNH_SAVE_TYPE(Text, "text", NNH_SAVE_FIELD(name, 1));


Kinds* g_bad_text = nullptr;
void encode_bad_text() {
    std::uint8_t out[256];
    (void)nnh::save::encode(*g_bad_text, out, sizeof out);
}

} // namespace

NNH_TEST(save_encode_writes_every_one_value_kind_by_hand) {
    const Bytes expected = header("kinds", 1) + Bytes{
        0x01, 1,  0x01,                                           // 1 flag: bool true
        0x02, 2,  0xAB,                                           // 2 u8
        0x03, 3,  0xFE,                                           // 3 i8 -2
        0x04, 4,  0x34, 0x12,                                     // 4 u16 0x1234
        0x05, 5,  0xFD, 0xFF,                                     // 5 i16 -3
        0x06, 6,  0xEF, 0xCD, 0xAB, 0x89,                         // 6 u32
        0x07, 7,  0xFC, 0xFF, 0xFF, 0xFF,                         // 7 i32 -4
        0x08, 8,  0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, // 8 u64
        0x09, 9,  0xFB, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 9 i64 -5
        0x0A, 10, 0x00, 0x00, 0xC0, 0x3F,                         // 10 f32 1.5
        0x0B, 11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xC0, // 11 f64 -2.25
        0x0C, 12, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0xBF, // 12 vec2 (1, -1)
        0x0D, 13, 0x01, 0x02, 0x03, 0x04,                         // 13 color
        0x0E, 14, 0x44, 0x33, 0x22, 0x11,                         // 14 data ref
        0x0F, 15, 0x05, 0x00, 0x00, 0x00,                         // 15 texture ref
        0x10, 16, 0x06, 0x00, 0x00, 0x00,                         // 16 sound ref
        0x11, 17, 0x07, 0x00, 0x00, 0x00,                         // 17 music ref
        0x12, 18, 0x03, 'A',  'n',  'a',                          // 18 string "Ana": its length, no NUL
        0x13, 19, 0x07, 0x02, 0x01, 0x09, 0x05, 0x02, 0xFF, 0xFF, // 19 struct {1 u8 9, 2 i16 -1}
        0x02, 20, 0x01,                                           // 20 enum Mood (u8) Angry
        0x02, 21, 0x05,                                           // 21 enum Element (u8) Fire
        0x09, 22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, // 22 enum Big (i64) 1 << 40
    };
    CHECK(encoded(saves_test::kinds_sample()) == expected);
}

NNH_TEST(save_encode_writes_any_nonzero_bool_as_one) {
    Kinds k;
    const std::uint8_t two = 2;
    std::memcpy(&k.flag, &two, 1);
    const Bytes b = encoded(k);
    CHECK(b.size() > 27 && b[21] == 0x01 && b[22] == 1 && b[23] == 0x01); // after the 21-byte header
}

NNH_TEST(save_encode_writes_defaults_too) {
    const Bytes b = encoded(Kinds{});
    CHECK(b.size() == 21 + 3 + 3 + 3 + 4 + 4 + 6 + 6 + 10 + 10 + 6 + 10 + 10 + 6 + 6 + 6 + 6 + 6 + 3 + 10 + 3 + 3 + 10);
}

NNH_TEST(save_encode_overhead_is_two_bytes_per_field_and_more_for_large_ids) {
    const Bytes expected = header("ids", 1) + Bytes{
        0x02, 0x7F, 1,                   // 127: one-byte id
        0x02, 0x80, 0x01, 2,             // 128: two bytes
        0x02, 0x80, 0x80, 0x01, 3,       // 16384: three bytes
    };
    CHECK(encoded(Ids{}) == expected);
}

NNH_TEST(save_encode_nests_structs_by_their_own_ids) {
    Deeper d;
    d.deep.pair = {4, 5};
    d.deep.tail = 6;
    const Bytes expected = header("deeper", 1) + Bytes{
        0x13, 3, 16,                                  // 3 deep: 16 bytes
        0x13, 1, 7, 0x02, 1, 4, 0x05, 2, 5, 0x00,     //   1 pair {1 u8 4, 2 i16 5}
        0x06, 2, 6, 0, 0, 0,                          //   2 tail u32 6
    };
    CHECK(encoded(d) == expected);
}

NNH_TEST(save_encode_refuses_a_short_buffer_and_writes_nothing_past_it) {
    const Kinds k = saves_test::kinds_sample();
    const std::size_t full = encoded(k).size();
    for (std::size_t capacity : {std::size_t{0}, std::size_t{10}, std::size_t{21}, full / 2, full - 1}) {
        std::uint8_t out[256];
        std::memset(out, 0xEE, sizeof out);
        const auto r = nnh::save::encode(k, out, capacity);
        CHECK(!r.ok() && r.error().code == nnh::ErrorCode::LimitReached);
        bool untouched = true;
        for (std::size_t i = capacity; i < sizeof out; ++i) untouched = untouched && out[i] == 0xEE;
        CHECK(untouched);
    }
    std::uint8_t out[256];
    CHECK(nnh::save::encode(k, out, full).value_or(0) == full);
}

NNH_TEST(save_encode_writes_a_string_of_every_length_up_to_its_capacity) {
    Text t;
    t.name.assign("12345678");
    CHECK((encoded(t) == header("text", 1) + Bytes{0x12, 1, 8, '1', '2', '3', '4', '5', '6', '7', '8'}));
    t.name.assign("");
    CHECK((encoded(t) == header("text", 1) + Bytes{0x12, 1, 0}));
    t.name.assign("\xC3\xA9");
    CHECK((encoded(t) == header("text", 1) + Bytes{0x12, 1, 2, 0xC3, 0xA9}));
}

NNH_TEST(save_encode_refuses_a_string_that_is_not_utf8) {
    static Kinds k = saves_test::kinds_sample();
    const char bad[] = {'\xFF', 'a', '\0'};
    k.name.assign(bad);
    g_bad_text = &k;
    if (nnh::build::kAssertsEnabled) {
        CHECK(actions_test::crashes(encode_bad_text)); // an assert in Debug and Development
    } else {
        std::uint8_t out[256];
        const auto r = nnh::save::encode(k, out, sizeof out);
        CHECK(!r.ok() && r.error().code == nnh::ErrorCode::InvalidArgument);
        CHECK(std::strstr(r.error().message, "field 18") != nullptr);
    }
}
