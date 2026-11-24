// Values that don't fit (spec 027 T4.3, FR-11, FR-15): out of range, inexact, another kind or shape, not finite, an
// enum outside its names, a bad bool byte — the field keeps its default and counts as dropped (per value); a string
// longer than its capacity is cut at a character boundary and a longer sequence keeps its first items — adjusted.
// Counts saturate at 65535; the first three paths are kept in Debug/Development.

#include <cstring>

#include "saves_fixture.hpp"

using saves_test::Bytes;
using saves_test::encoded;
using saves_test::header;
using saves_test::operator+;

namespace fit {

struct Wide {
    std::uint16_t small = 0;
    std::int32_t minus = 0;
    std::uint32_t not_float = 0;
    double not_narrow = 0;
    std::uint32_t was_struct = 0;
    saves_test::Pair was_number;
    std::uint8_t was_array[2] = {};
    std::uint8_t was_one = 0;
    nnh::data::String<6> text2;
    nnh::data::String<6> text3;
    nnh::data::List<std::uint8_t, 5> list;
    std::uint8_t array[4] = {};
    std::uint16_t values[4] = {};
    std::uint8_t few[2] = {};
    double exact = 0;
};
NNH_SAVE_TYPE(Wide, "fit", NNH_SAVE_FIELD(small, 1), NNH_SAVE_FIELD(minus, 2), NNH_SAVE_FIELD(not_float, 3),
              NNH_SAVE_FIELD(not_narrow, 4), NNH_SAVE_FIELD(was_struct, 5), NNH_SAVE_FIELD(was_number, 6),
              NNH_SAVE_FIELD(was_array, 7), NNH_SAVE_FIELD(was_one, 8), NNH_SAVE_FIELD(text2, 9),
              NNH_SAVE_FIELD(text3, 10), NNH_SAVE_FIELD(list, 11), NNH_SAVE_FIELD(array, 12), NNH_SAVE_FIELD(values, 13),
              NNH_SAVE_FIELD(few, 14), NNH_SAVE_FIELD(exact, 15));

struct Narrow {
    std::uint8_t small = 1;
    std::uint32_t minus = 2;
    float not_float = 3;
    float not_narrow = 4;
    saves_test::Pair was_struct;
    std::uint32_t was_number = 6;
    std::uint8_t was_array = 7;
    std::uint8_t was_one[2] = {8, 8};
    nnh::data::String<2> text2;
    nnh::data::String<4> text3;
    nnh::data::List<std::uint8_t, 3> list;
    std::uint8_t array[2] = {};
    std::uint8_t values[4] = {9, 9, 9, 9};
    std::uint8_t few[4] = {5, 5, 5, 5};
    float exact = 0;
};
NNH_SAVE_TYPE(Narrow, "fit", NNH_SAVE_FIELD(small, 1), NNH_SAVE_FIELD(minus, 2), NNH_SAVE_FIELD(not_float, 3),
              NNH_SAVE_FIELD(not_narrow, 4), NNH_SAVE_FIELD(was_struct, 5), NNH_SAVE_FIELD(was_number, 6),
              NNH_SAVE_FIELD(was_array, 7), NNH_SAVE_FIELD(was_one, 8), NNH_SAVE_FIELD(text2, 9),
              NNH_SAVE_FIELD(text3, 10), NNH_SAVE_FIELD(list, 11), NNH_SAVE_FIELD(array, 12), NNH_SAVE_FIELD(values, 13),
              NNH_SAVE_FIELD(few, 14), NNH_SAVE_FIELD(exact, 15));

Wide wide() {
    Wide w;
    w.small = 300;                 // > 255
    w.minus = -1;                  // a u32 can't hold it
    w.not_float = (1u << 24) + 1;  // 25 significant bits
    w.not_narrow = 0.1;            // no float is exactly 0.1
    w.was_struct = 5;
    w.was_number = {1, 2};
    w.was_array[0] = 3;
    w.was_one = 4;
    w.text2.assign("a\xC3\xA9");   // "aé": 3 bytes into 2 -> "a"
    w.text3.assign("\xE6\x97\xA5\xE6\x9C\xAC"); // "日本": 6 bytes into 4 -> "日"
    for (std::uint8_t i = 1; i <= 5; ++i) w.list.push_back(i);
    w.array[0] = 1;
    w.array[1] = 2;
    w.array[2] = 3;
    w.array[3] = 4;
    w.values[0] = 1;
    w.values[1] = 256;   // too big for u8
    w.values[2] = 3;
    w.values[3] = 1000;  // too big for u8
    w.few[0] = 6;
    w.few[1] = 7;
    w.exact = 1.5;
    return w;
}

struct OneFloat {
    float f = 0;
    double d = 0;
    nnh::Vec2 v{};
};
NNH_SAVE_TYPE(OneFloat, "floats", NNH_SAVE_FIELD(f, 1), NNH_SAVE_FIELD(d, 2), NNH_SAVE_FIELD(v, 3));

struct Flags {
    bool on = true;
    test_data::Element element = test_data::Element::Ice;
    test_data::Element elements[3] = {};
};
NNH_SAVE_TYPE(Flags, "flags", NNH_SAVE_FIELD(on, 1), NNH_SAVE_FIELD(element, 2), NNH_SAVE_FIELD(elements, 3));

struct AllBytes {
    std::uint8_t values[65535] = {};
    std::uint8_t more = 0;
};
NNH_SAVE_TYPE(AllBytes, "many", NNH_SAVE_FIELD(values, 1), NNH_SAVE_FIELD(more, 2));

} // namespace fit

using fit::Narrow;

namespace {

nnh::save::Report g_report;

Narrow narrow_of(const Bytes& b) {
    Narrow n;
    const auto r = nnh::save::decode(b.data(), b.size(), n);
    CHECK(r.ok());
    if (r.ok()) g_report = r.value();
    return n;
}

} // namespace

NNH_TEST(save_fit_drops_numbers_out_of_range_or_inexact) {
    const Narrow n = narrow_of(encoded(fit::wide()));
    CHECK(n.small == 1 && n.minus == 2 && n.not_float == 3.0f && n.not_narrow == 4.0f);
    CHECK(n.exact == 1.5f); // an exact double -> float converts
}

NNH_TEST(save_fit_drops_values_whose_kind_or_shape_changed) {
    const Narrow n = narrow_of(encoded(fit::wide()));
    CHECK(n.was_struct.a == 1 && n.was_struct.b == -2 && n.was_number == 6); // int <-> struct
    CHECK(n.was_array == 7 && n.was_one[0] == 8 && n.was_one[1] == 8);        // array <-> one value
}

NNH_TEST(save_fit_cuts_strings_at_a_character_boundary) {
    const Narrow n = narrow_of(encoded(fit::wide()));
    CHECK(n.text2 == "a" && n.text3 == "\xE6\x97\xA5");
}

NNH_TEST(save_fit_keeps_the_first_items_of_a_longer_sequence) {
    const Narrow n = narrow_of(encoded(fit::wide()));
    CHECK(n.list.size() == 3 && n.list[0] == 1 && n.list[2] == 3);
    CHECK(n.array[0] == 1 && n.array[1] == 2);
}

NNH_TEST(save_fit_drops_per_value_and_keeps_defaults_for_missing_items) {
    const Narrow n = narrow_of(encoded(fit::wide()));
    CHECK(n.values[0] == 1 && n.values[1] == 9 && n.values[2] == 3 && n.values[3] == 9); // two drops
    CHECK(n.few[0] == 6 && n.few[1] == 7 && n.few[2] == 5 && n.few[3] == 5);             // not counted
}

NNH_TEST(save_fit_counts_and_paths) {
    narrow_of(encoded(fit::wide()));
    // dropped: 1, 2, 3, 4, 5, 6, 7, 8, 13[1], 13[3]; adjusted: 9, 10, 11, 12
    CHECK(g_report.dropped == 10 && g_report.adjusted == 4 && g_report.unknown == 0);
    if (nnh::build::kAssertsEnabled) {
        CHECK(g_report.dropped_at[0] == nnh::save::path(1) && g_report.dropped_at[2] == nnh::save::path(3));
        CHECK(g_report.adjusted_at[0] == nnh::save::path(9) && g_report.adjusted_at[2] == nnh::save::path(11));
    }
}

NNH_TEST(save_fit_drops_floats_that_are_not_finite) {
    const std::uint32_t nan = 0x7FC00000u, inf = 0x7F800000u;
    const std::uint64_t dinf = 0xFFF0000000000000ull;
    for (int which = 0; which < 3; ++which) {
        fit::OneFloat in;
        if (which == 0) std::memcpy(&in.f, &nan, 4);
        if (which == 1) std::memcpy(&in.d, &dinf, 8);
        if (which == 2) std::memcpy(&in.v.y, &inf, 4);
        const Bytes b = encoded(in); // stored as they are
        fit::OneFloat out;
        out.f = 5;
        out.d = 5;
        out.v = {5, 5};
        const auto r = nnh::save::decode(b.data(), b.size(), out);
        CHECK(r.ok() && r.value().dropped == 1);
        CHECK(which != 0 || out.f == 0); // the default of the dropped field
        CHECK(which != 1 || out.d == 0);
        CHECK(which != 2 || (out.v.x == 0 && out.v.y == 0));
    }
}

NNH_TEST(save_fit_drops_bad_bool_bytes_and_enums_outside_their_names) {
    // bool byte 2, element 3 (Element has Normal 0, Fire 5, Ice 6), elements [5, 9, 6]
    for (std::uint8_t byte : {std::uint8_t{2}, std::uint8_t{0xFF}}) {
        const Bytes b = header("flags", 1) + Bytes{0x01, 1, byte, 0x02, 2, 3, 0x22, 3, 3, 5, 9, 6};
        fit::Flags f;
        const auto r = nnh::save::decode(b.data(), b.size(), f);
        CHECK(r.ok() && r.value().dropped == 3);
        CHECK(f.on && f.element == test_data::Element::Ice); // the defaults
        CHECK(f.elements[0] == test_data::Element::Fire && f.elements[1] == test_data::Element::Normal);
        CHECK(f.elements[2] == test_data::Element::Ice);
    }
}

NNH_TEST(save_fit_counts_saturate_at_65535) {
    // field 1: 65535 u16 values of 300 (none fits a u8), field 2: u16 999 -> 65536 drops, reported as 65535
    Bytes b = header("many", 1) + Bytes{0x24, 1, 0xFF, 0xFF, 0x03};
    for (int i = 0; i < 65535; ++i) {
        b.push_back(0x2C);
        b.push_back(0x01);
    }
    b = b + Bytes{0x04, 2, 0xE7, 0x03};
    static fit::AllBytes out;
    const auto r = nnh::save::decode(b.data(), b.size(), out);
    CHECK(r.ok() && r.value().dropped == 65535 && r.value().adjusted == 0);
    if (nnh::build::kAssertsEnabled) {
        CHECK(r.value().dropped_at[2] == nnh::save::path(1).item(2)); // the first three kept
    }
}
