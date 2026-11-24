// Edges found by the mutation pass (spec 027 T8.1): each test kills a mutant that survived the stage 1-7
// tests — a sequence one byte short, a struct list whose rows can't fit, a list count above its capacity in
// memory, the edges of signed ranges and of float exponents, list slots past the file's items, a bad bool
// byte inside an array, two keys of one hash, a row read into a number.

#include "nnh/fs.hpp"
#include "saves/save_validate.hpp"
#include "saves_fixture.hpp"

#include <cstring>

using saves_test::Bytes;
using saves_test::encoded;
using saves_test::header;
using saves_test::operator+;

namespace edges {

struct Wide16 {
    std::uint16_t v = 0;
    std::int16_t w = 0;
};
NNH_SAVE_TYPE(Wide16, "edge16", NNH_SAVE_FIELD(v, 1), NNH_SAVE_FIELD(w, 2));
struct Narrow16 {
    std::int16_t v = 7;
    std::int8_t w = 7;
};
NNH_SAVE_TYPE(Narrow16, "edge16", NNH_SAVE_FIELD(v, 1), NNH_SAVE_FIELD(w, 2));

struct Doubles {
    double big = 0;
    double exact = 0;
};
NNH_SAVE_TYPE(Doubles, "edgef", NNH_SAVE_FIELD(big, 1), NNH_SAVE_FIELD(exact, 2));
struct Floats {
    float big = 3;
    float exact = 3;
};
NNH_SAVE_TYPE(Floats, "edgef", NNH_SAVE_FIELD(big, 1), NNH_SAVE_FIELD(exact, 2));

nnh::data::List<std::uint16_t, 4> starter() {
    nnh::data::List<std::uint16_t, 4> l;
    l.push_back(50);
    l.push_back(60);
    l.push_back(70);
    return l;
}
struct Starter {
    nnh::data::List<std::uint16_t, 4> items = starter(); // a default list with items
    bool flags[3] = {true, true, true};
};
NNH_SAVE_TYPE(Starter, "starter", NNH_SAVE_FIELD(items, 1), NNH_SAVE_FIELD(flags, 2));

struct Collide {
    std::uint8_t a = 0;
};
NNH_SAVE_TYPE(Collide, "bgpx_", NNH_SAVE_FIELD(a, 1)); // FNV-1a 32 of "bgpx_" and "b13ck": 0xA6E8D423

} // namespace edges

namespace {

bool refused(const Bytes& b, const char* message) {
    const auto r = nnh::detail::save_validate_fields(b.data(), b.size());
    return !r.ok() && std::strcmp(r.error().message, message) == 0;
}

} // namespace

NNH_TEST(save_edge_validator_refuses_a_sequence_one_byte_short) {
    CHECK(refused({0x26, 7, 2, 1, 2, 3, 4, 5, 6, 7},
                  "field 7: list count 2 exceeds the file")); // 8 bytes needed, 7
}

NNH_TEST(save_edge_validator_refuses_rows_that_cannot_fit_before_reading_them) {
    // 5 rows of one u8 in 2 bytes: refused by the count, not by the third row
    CHECK(refused({0x53, 4, 5, 1, 0x02, 1, 2, 7, 8}, "field 4: list count 5 exceeds the file"));
}

NNH_TEST(save_edge_encode_clamps_a_list_count_above_its_capacity) {
    saves_test::Seqs s = saves_test::seqs_sample();
    const std::uint16_t too_many = 9;
    std::memcpy(&s.flags, &too_many, sizeof too_many); // the List's count is at offset 0
    const Bytes b = encoded(s);
    // field 2 (flags) follows field 1 (9 bytes after the 20-byte header): wire, id, count
    CHECK(b.size() > 32 && b[29] == 0x41 && b[30] == 2 && b[31] == 4);
}

NNH_TEST(save_edge_signed_ranges_end_at_their_limits) {
    edges::Wide16 in;
    in.v = 32768; // one past int16's max
    in.w = -128;  // int8's min
    Bytes b = encoded(in);
    edges::Narrow16 out;
    auto r = nnh::save::decode(b.data(), b.size(), out);
    CHECK(r.ok() && out.v == 7 && out.w == -128 && r.value().dropped == 1);
    in.v = 32767;
    in.w = 128; // one past int8's max
    b = encoded(in);
    r = nnh::save::decode(b.data(), b.size(), out);
    CHECK(r.ok() && out.v == 32767 && out.w == 7 && r.value().dropped == 1);
}

NNH_TEST(save_edge_double_to_float_stops_at_the_largest_exponent) {
    edges::Doubles in;
    const std::uint64_t two_128 = 0x47F0000000000000ull; // 2^128: above every float
    const std::uint64_t two_127 = 0x47E0000000000000ull; // 2^127: a float
    std::memcpy(&in.big, &two_128, 8);
    std::memcpy(&in.exact, &two_127, 8);
    const Bytes b = encoded(in);
    edges::Floats out;
    const auto r = nnh::save::decode(b.data(), b.size(), out);
    std::uint32_t bits = 0;
    std::memcpy(&bits, &out.exact, 4);
    CHECK(r.ok() && out.big == 3 && bits == 0x7F000000u && r.value().dropped == 1);
}

NNH_TEST(save_edge_list_slots_past_the_file_items_get_their_defaults) {
    // field 1: one u16 (5); the default list had 3 items: slots 1 and 2 must be cleared
    const Bytes b = header("starter", 1) + Bytes{0x44, 1, 1, 5, 0};
    edges::Starter out;
    CHECK(nnh::save::decode(b.data(), b.size(), out).ok());
    CHECK(out.items.size() == 1 && out.items[0] == 5 && out.items.begin()[1] == 0 &&
          out.items.begin()[2] == 0);
}

NNH_TEST(save_edge_a_bad_bool_byte_in_an_array_is_dropped) {
    const Bytes b = header("starter", 1) + Bytes{0x21, 2, 3, 0, 2, 1}; // flags: false, 2, true
    edges::Starter out;
    const auto r = nnh::save::decode(b.data(), b.size(), out);
    CHECK(r.ok() && !out.flags[0] && out.flags[1] && out.flags[2] &&
          r.value().dropped == 1); // [1] keeps true
}

NNH_TEST(save_edge_keys_of_the_same_hash_are_still_different_types) {
    CHECK(nnh::detail::save_key_hash("bgpx_") == nnh::detail::save_key_hash("b13ck"));
    const Bytes b = header("b13ck", 1) + Bytes{0x02, 1, 9};
    edges::Collide out;
    const auto r = nnh::save::decode(b.data(), b.size(), out);
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::InvalidArgument && out.a == 0);
}

NNH_TEST(save_edge_old_fields_refuse_a_row_into_a_number) {
    // field 9: a list of two rows {1 u16}
    const Bytes fields = {0x53, 9, 2, 1, 0x04, 1, 4, 1, 0, 2, 0};
    const nnh::save::OldFields old(fields.data(), static_cast<std::uint32_t>(fields.size()));
    std::uint32_t number = 77;
    CHECK(!old.get(nnh::save::path(9).item(1), number) && number == 77);
    CHECK(old.get(nnh::save::path(9).item(1).field(1), number) && number == 2);
}

NNH_TEST(save_edge_old_fields_skip_whole_strings_to_reach_an_item) {
    // field 3: a list of strings "ab", "xyz"
    const Bytes fields = {0x52, 3, 2, 7, 2, 'a', 'b', 3, 'x', 'y', 'z'};
    const nnh::save::OldFields old(fields.data(), static_cast<std::uint32_t>(fields.size()));
    nnh::data::String<4> text;
    CHECK(old.get(nnh::save::path(3).item(1), text) && text == "xyz");
}

NNH_TEST(save_edge_a_damaged_save_without_backup_is_corrupted_not_missing) {
    saves_test::fresh();
    nnh::save::Buffer<saves_test::Kinds> buffer;
    CHECK(nnh::save::store("only.sav", saves_test::kinds_sample(), buffer).ok()); // the first store: no .bak
    nnh::platform_fake::corrupt_file("only.sav", 30);
    saves_test::Kinds out;
    CHECK(nnh::save::load("only.sav", out, buffer).error().code == nnh::ErrorCode::Corrupted);
    nnh::shutdown();
}

NNH_TEST(save_edge_info_refuses_a_cut_file_whose_headers_are_whole) {
    saves_test::fresh();
    nnh::save::Buffer<saves_test::Kinds> buffer;
    CHECK(nnh::save::store("cut.sav", saves_test::kinds_sample(), buffer).ok());
    nnh::platform_fake::truncate_file("cut.sav", 60); // both headers still there, the payload cut
    CHECK(nnh::save::info("cut.sav").error().code == nnh::ErrorCode::Corrupted);
    nnh::shutdown();
}

NNH_TEST(save_edge_a_load_that_only_adjusted_logs_its_line) {
    saves_test::fresh();

    // a "text" save whose string is longer than the reading type's: the same key, a smaller capacity
    const Bytes payload =
        header("kinds", 1) + Bytes{0x12, 18, 9, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i'};
    CHECK(nnh::save::write("adjusted.sav", payload.data(), payload.size(), 1).ok());
    const int before = nnh::platform_fake::state().console_lines;
    saves_test::Kinds out;
    nnh::save::Buffer<saves_test::Kinds> buffer;
    const auto r = nnh::save::load("adjusted.sav", out, buffer);
    CHECK(r.ok() && r.value().adjusted == 1 && r.value().dropped == 0 && out.name == "abcdefgh");
    CHECK(nnh::platform_fake::state().console_lines == before + 1);
    nnh::shutdown();
}
