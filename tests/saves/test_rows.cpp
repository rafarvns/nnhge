// Struct rows (spec 027 T10.3): what the per-column row paths of the validator, the decoder and the encoder must
// keep exactly — each column's copy, check or conversion, the counts and paths of the report (unknown, retired,
// dropped, adjusted, per row), the values visited, templates past the planned columns, the validator's messages and
// steps inside rows, a bad string inside a row, bools of any byte, and canonical lengths around 128 and 16384 bytes.

#include <cstring>

#include "../actions/actions_fixture.hpp"
#include "saves/save_apply.hpp"
#include "saves/save_validate.hpp"
#include "saves_fixture.hpp"

using saves_test::Bytes;
using saves_test::encoded;
using saves_test::header;
using saves_test::operator+;

namespace rows {

struct Cell {
    std::uint16_t a = 11;
    float f = 12;
    bool b = true;
    nnh::data::String<3> s;
    std::uint8_t narrow = 13;
};
NNH_SAVE_STRUCT(Cell, NNH_SAVE_FIELD(a, 1), NNH_SAVE_FIELD(f, 2), NNH_SAVE_FIELD(b, 3), NNH_SAVE_FIELD(s, 4),
                NNH_SAVE_FIELD(narrow, 7), NNH_SAVE_RETIRED(6));
struct Sheet {
    nnh::data::List<Cell, 4> cells;
};
NNH_SAVE_TYPE(Sheet, "sheet", NNH_SAVE_FIELD(cells, 1));

struct Wide17 { // more columns than a row plan holds
    std::uint8_t c1 = 0, c2 = 0, c3 = 0, c4 = 0, c5 = 0, c6 = 0, c7 = 0, c8 = 0, c9 = 0;
    std::uint8_t c10 = 0, c11 = 0, c12 = 0, c13 = 0, c14 = 0, c15 = 0, c16 = 0, c17 = 0;
};
NNH_SAVE_STRUCT(Wide17, NNH_SAVE_FIELD(c1, 1), NNH_SAVE_FIELD(c2, 2), NNH_SAVE_FIELD(c3, 3), NNH_SAVE_FIELD(c4, 4),
                NNH_SAVE_FIELD(c5, 5), NNH_SAVE_FIELD(c6, 6), NNH_SAVE_FIELD(c7, 7), NNH_SAVE_FIELD(c8, 8),
                NNH_SAVE_FIELD(c9, 9), NNH_SAVE_FIELD(c10, 10), NNH_SAVE_FIELD(c11, 11), NNH_SAVE_FIELD(c12, 12),
                NNH_SAVE_FIELD(c13, 13), NNH_SAVE_FIELD(c14, 14), NNH_SAVE_FIELD(c15, 15), NNH_SAVE_FIELD(c16, 16),
                NNH_SAVE_FIELD(c17, 17));
struct WideRows {
    nnh::data::List<Wide17, 3> rows;
};
NNH_SAVE_TYPE(WideRows, "wide_rows", NNH_SAVE_FIELD(rows, 1));

struct Named {
    nnh::data::String<40> name;
};
NNH_SAVE_STRUCT(Named, NNH_SAVE_FIELD(name, 1));
struct Roster {
    nnh::data::List<Named, 8> people;
    nnh::data::List<nnh::data::String<40>, 8> lines;
};
NNH_SAVE_TYPE(Roster, "roster", NNH_SAVE_FIELD(people, 1), NNH_SAVE_FIELD(lines, 2));

struct Five {
    std::uint8_t a = 0;
    std::uint32_t b = 0;
};
NNH_SAVE_STRUCT(Five, NNH_SAVE_FIELD(a, 1), NNH_SAVE_FIELD(b, 2));
struct Fives {
    nnh::data::List<Five, 30> rows;
};
NNH_SAVE_TYPE(Fives, "fives", NNH_SAVE_FIELD(rows, 1));

struct Eight { // 64 bytes a row
    std::uint64_t v0 = 0, v1 = 0, v2 = 0, v3 = 0, v4 = 0, v5 = 0, v6 = 0, v7 = 0;
};
NNH_SAVE_STRUCT(Eight, NNH_SAVE_FIELD(v0, 1), NNH_SAVE_FIELD(v1, 2), NNH_SAVE_FIELD(v2, 3), NNH_SAVE_FIELD(v3, 4),
                NNH_SAVE_FIELD(v4, 5), NNH_SAVE_FIELD(v5, 6), NNH_SAVE_FIELD(v6, 7), NNH_SAVE_FIELD(v7, 8));
struct Eights {
    nnh::data::List<Eight, 256> rows;
};
NNH_SAVE_TYPE(Eights, "eights", NNH_SAVE_FIELD(rows, 1));

struct Flags13 {
    bool f[13] = {};
};
NNH_SAVE_TYPE(Flags13, "flags13", NNH_SAVE_FIELD(f, 1));

struct Holder { // a row with a struct value and a bool
    saves_test::Pair pair;
    bool on = false;
};
NNH_SAVE_STRUCT(Holder, NNH_SAVE_FIELD(pair, 1), NNH_SAVE_FIELD(on, 2));
struct Holders {
    nnh::data::List<Holder, 3> items;
};
NNH_SAVE_TYPE(Holders, "holders", NNH_SAVE_FIELD(items, 1));

} // namespace rows

namespace {

/// Field 1: 3 rows of the template {1 u16, 2 f32, 3 bool, 4 string, 5 u32 (unknown to Cell), 6 u8 (retired), 7 u16}.
Bytes sheet_file() {
    return header("sheet", 1) + Bytes{
        0x53, 1, 3, 7, 0x04, 1, 0x0A, 2, 0x01, 3, 0x12, 4, 0x06, 5, 0x02, 6, 0x04, 7, 51,
        0x02, 0x01, 0, 0, 0xC0, 0x3F, 1, 2, 'a', 'b', 5, 0, 0, 0, 9, 50, 0,             // row 0: everything fits
        7, 0, 0, 0, 0xC0, 0x7F, 2, 4, 'a', 'b', 'c', 'd', 6, 0, 0, 0, 1, 0x2C, 0x01,     // row 1: NaN, 2, cut, 300
        9, 0, 0, 0, 0x80, 0x7F, 0, 0, 7, 0, 0, 0, 2, 0xFF, 0,                           // row 2: infinity
    };
}

bool refused(const Bytes& b, const char* message) {
    const auto r = nnh::detail::save_validate_fields(b.data(), b.size());
    if (r.ok()) return false;
    if (std::strcmp(r.error().message, message) == 0) return true;
    std::printf("    got \"%s\"\n", r.error().message);
    return false;
}

rows::Roster* g_bad_roster = nullptr;
void encode_bad_roster() {
    static std::uint8_t out[1024];
    (void)nnh::save::encode(*g_bad_roster, out, sizeof out);
}

} // namespace

NNH_TEST(save_rows_apply_each_column_by_its_field) {
    const Bytes b = sheet_file();
    rows::Sheet out;
    const auto r = nnh::save::decode(b.data(), b.size(), out);
    CHECK(r.ok() && out.cells.size() == 3);
    const rows::Cell& c0 = out.cells[0];
    CHECK(c0.a == 0x0102 && c0.f == 1.5f && c0.b && c0.s == "ab" && c0.narrow == 50);
    const rows::Cell& c1 = out.cells[1];
    CHECK(c1.a == 7 && c1.f == 12 && c1.b && c1.s == "abc" && c1.narrow == 13); // defaults where dropped
    const rows::Cell& c2 = out.cells[2];
    CHECK(c2.a == 9 && c2.f == 12 && !c2.b && c2.s == "" && c2.narrow == 255);
}

NNH_TEST(save_rows_report_counts_and_paths_per_row) {
    const Bytes b = sheet_file();
    rows::Sheet out;
    const auto r = nnh::save::decode(b.data(), b.size(), out);
    CHECK(r.ok() && r.value().unknown == 3 && r.value().dropped == 4 && r.value().adjusted == 1);
    if (r.ok() && nnh::build::kAssertsEnabled) {
        const nnh::save::Report& p = r.value();
        const auto at = [](int item, int id) { return nnh::save::path(1).item(item).field(id); };
        CHECK(p.unknown_at[0] == at(0, 5) && p.unknown_at[1] == at(1, 5) && p.unknown_at[2] == at(2, 5));
        CHECK(p.dropped_at[0] == at(1, 2) && p.dropped_at[1] == at(1, 3) && p.dropped_at[2] == at(1, 7));
        CHECK(p.adjusted_at[0] == at(1, 4) && p.adjusted_at[1] == nnh::save::FieldPath{});
    }
}

NNH_TEST(save_rows_visit_every_value_of_every_row) {
    const Bytes b = sheet_file();
    static rows::Sheet out;
    nnh::save::Report report;
    nnh::detail::SaveApply apply;
    apply.report = &report;
    const std::size_t header_size = 16 + 5;
    nnh::detail::apply_fields(apply, nnh::detail::save_info_of<rows::Sheet>(), reinterpret_cast<std::uint8_t*>(&out),
                              b.data() + header_size, b.data() + b.size());
    CHECK(apply.visited == 1 + 3 + 3 * 7); // the field, its items, each item's seven values
}

NNH_TEST(save_rows_of_more_columns_than_a_plan_holds_round_trip) {
    rows::WideRows in;
    for (int i = 0; i < 3; ++i) {
        rows::Wide17 w;
        std::uint8_t* bytes = reinterpret_cast<std::uint8_t*>(&w);
        for (int k = 0; k < 17; ++k) bytes[k] = static_cast<std::uint8_t>(10 * i + k);
        in.rows.push_back(w);
    }
    const Bytes b = encoded(in);
    rows::WideRows out;
    const auto r = nnh::save::decode(b.data(), b.size(), out);
    CHECK(r.ok() && out.rows.size() == 3 && r.value().dropped == 0);
    for (int i = 0; i < 3; ++i) CHECK(std::memcmp(&in.rows[i], &out.rows[i], sizeof(rows::Wide17)) == 0);
}

NNH_TEST(save_rows_validator_names_the_row_of_a_cut_or_bad_value) {
    // {1 u8, 2 u32} x 3 in 13 bytes: row 2's u32 has 2 bytes
    CHECK(refused({0x53, 4, 3, 2, 0x02, 1, 0x06, 2, 13, 1, 1, 2, 3, 4, 2, 1, 2, 3, 4, 3, 9, 9},
                  "field 4[2].2: value cut short"));
    // {1 u8, 2 string} x 2: row 1's string says 5 bytes, 1 left
    CHECK(refused({0x53, 4, 2, 2, 0x02, 1, 0x12, 2, 6, 1, 1, 'a', 2, 5, 'b'}, "field 4[1].2: string cut short"));
    CHECK(refused({0x53, 4, 2, 2, 0x02, 1, 0x12, 2, 7, 1, 1, 'a', 2, 2, 'b', 0}, "field 4[1].2: string holds a NUL"));
    CHECK(refused({0x53, 4, 2, 2, 0x02, 1, 0x12, 2, 7, 1, 1, 'a', 2, 2, 0xC3, 'b'},
                  "field 4[1].2: string is not UTF-8"));
}

NNH_TEST(save_rows_validator_counts_one_step_per_row_value) {
    // field 9: 13 rows of {1 u8, 4 string}: 1 field + 2 template fields + 26 values
    Bytes b = {0x53, 9, 13, 2, 0x02, 1, 0x12, 4, 13 * 3};
    for (int i = 0; i < 13; ++i) {
        b.push_back(static_cast<std::uint8_t>(i));
        b.push_back(1);
        b.push_back('a');
    }
    std::uint32_t steps = 0;
    CHECK(nnh::detail::save_validate_fields(b.data(), b.size(), &steps).ok() && steps == 29);
}

NNH_TEST(save_rows_encoder_names_the_row_of_a_bad_string) {
    static rows::Roster roster;
    nnh::data::String<40> s;
    s.assign("ok");
    for (int i = 0; i < 4; ++i) roster.people.push_back({s});
    const char bad[] = {'x', '\xFF', '\0'};
    roster.people[2].name.assign(bad);
    g_bad_roster = &roster;
    if (nnh::build::kAssertsEnabled) {
        CHECK(actions_test::crashes(encode_bad_roster)); // an assert in Debug and Development
    } else {
        std::uint8_t out[1024];
        const auto r = nnh::save::encode(roster, out, sizeof out);
        CHECK(!r.ok() && r.error().code == nnh::ErrorCode::InvalidArgument);
        CHECK(!r.ok() && std::strstr(r.error().message, "field 1[2].1:") != nullptr);
    }
}

NNH_TEST(save_rows_encoder_writes_every_bool_byte_as_zero_or_one) {
    rows::Flags13 in;
    const std::uint8_t bytes[13] = {0, 1, 2, 0x7F, 0x80, 0xFF, 1, 0, 0x10, 0x01, 0, 0xFE, 3};
    std::memcpy(in.f, bytes, sizeof bytes);
    const Bytes want = header("flags13", 1) + Bytes{0x21, 1, 13, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1};
    CHECK(encoded(in) == want);
}

NNH_TEST(save_rows_encoder_writes_struct_values_and_bools_of_rows) {
    rows::Holders h;
    h.items.push_back({{1, -2}, true});
    h.items.push_back({{3, 4}, false});
    const std::uint8_t two = 2;
    std::memcpy(&h.items[1].on, &two, 1); // any non-zero byte is true
    const Bytes want = header("holders", 1) + Bytes{
        0x53, 1, 2, 2, 0x13, 1, 0x01, 2, 18,           // 2 rows of {1 Pair, 2 bool}, 18 bytes
        7, 0x02, 1, 1, 0x05, 2, 0xFE, 0xFF, 1,          // row 0: pair {1 u8 1, 2 i16 -2}, on
        7, 0x02, 1, 3, 0x05, 2, 4, 0, 1,                // row 1: pair {3, 4}, on (2 -> 1)
    };
    CHECK(encoded(h) == want);
}

NNH_TEST(save_rows_encoder_refuses_every_short_buffer_and_writes_nothing_past_it) {
    rows::Roster roster;
    nnh::data::String<40> name;
    name.assign("abcdefghij");
    for (int i = 0; i < 8; ++i) roster.people.push_back({name});
    const std::size_t full = encoded(roster).size();
    bool right = full > 0;
    for (std::size_t capacity = 0; capacity < full; ++capacity) {
        static std::uint8_t out[512];
        std::memset(out, 0xEE, sizeof out);
        const auto r = nnh::save::encode(roster, out, capacity);
        right = right && !r.ok() && r.error().code == nnh::ErrorCode::LimitReached;
        for (std::size_t i = capacity; i < sizeof out; ++i) right = right && out[i] == 0xEE;
    }
    CHECK(right);
}

NNH_TEST(save_rows_decoder_drops_each_bad_bool_of_an_array) {
    const Bytes b = header("flags13", 1) + Bytes{0x21, 1, 13, 2, 1, 1, 0, 1, 0x80, 0, 1, 1, 1, 0, 0, 0xFF};
    rows::Flags13 out;
    out.f[0] = out.f[5] = out.f[12] = true;
    out.f[3] = true;
    const auto r = nnh::save::decode(b.data(), b.size(), out);
    CHECK(r.ok() && r.value().dropped == 3);
    CHECK(!out.f[0] && out.f[1] && !out.f[3] && out.f[4] && !out.f[5] && out.f[7] && !out.f[12]); // T{}: false
    if (r.ok() && nnh::build::kAssertsEnabled) {
        CHECK(r.value().dropped_at[0] == nnh::save::path(1).item(0));
        CHECK(r.value().dropped_at[1] == nnh::save::path(1).item(5));
        CHECK(r.value().dropped_at[2] == nnh::save::path(1).item(12));
    }
}

NNH_TEST(save_rows_lengths_stay_canonical_around_128_bytes) {
    rows::Roster roster;
    nnh::data::String<40> name;
    name.assign("0123456789012345678901234567890123456789"); // 40 bytes: 41 per row
    for (int i = 0; i < 3; ++i) roster.people.push_back({name});
    Bytes b = encoded(roster);
    const std::size_t at = 16 + 6 + 6; // field 1: wire, id, count, template size, wire, id, then the length
    CHECK(b.size() > at + 2 && b[at] == 123 && b[at + 1] == 40);
    roster.people.push_back({name}); // 164 bytes: two length bytes, the rows moved once
    roster.lines.push_back(name);
    roster.lines.push_back(name);
    roster.lines.push_back(name);
    roster.lines.push_back(name);
    b = encoded(roster);
    CHECK(b.size() > at + 3 && b[at] == 0xA4 && b[at + 1] == 0x01 && b[at + 2] == 40);
    const std::size_t lines = at + 2 + 164; // field 2: wire, id, count, then the length
    CHECK(b.size() == lines + 5 + 164 && b[lines] == 0x52 && b[lines + 3] == 0xA4 && b[lines + 4] == 0x01);
    rows::Roster out;
    const auto r = nnh::save::decode(b.data(), b.size(), out);
    CHECK(r.ok() && out.people.size() == 4 && out.people[3].name == name && out.lines.size() == 4 &&
          out.lines[3] == name);
}

NNH_TEST(save_rows_lengths_of_fixed_rows_are_reserved_at_their_size) {
    rows::Fives f;
    for (int i = 0; i < 25; ++i) f.rows.push_back({static_cast<std::uint8_t>(i), static_cast<std::uint32_t>(i)});
    Bytes b = encoded(f); // 125 bytes of rows
    const std::size_t at = 16 + 5 + 8;
    CHECK(b.size() == at + 1 + 125 && b[at] == 125 && b[at + 1] == 0 && b[at + 2] == 0);
    f.rows.push_back({25, 25}); // 130
    b = encoded(f);
    CHECK(b.size() == at + 2 + 130 && b[at] == 0x82 && b[at + 1] == 0x01 && b[at + 2] == 0);
    rows::Fives out;
    CHECK(nnh::save::decode(b.data(), b.size(), out).ok() && out.rows.size() == 26 && out.rows[25].b == 25);
}

NNH_TEST(save_rows_lengths_reach_three_bytes_at_16384) {
    static rows::Eights e;
    for (int i = 0; i < 255; ++i) e.rows.push_back({static_cast<std::uint64_t>(i) << 33});
    Bytes b = encoded(e); // 16320 bytes of rows
    const std::size_t at = 16 + 6 + 21; // wire, id, a two-byte count, template size, 8 x (wire, id)
    CHECK(b.size() == at + 2 + 16320 && b[at] == 0xC0 && b[at + 1] == 0x7F);
    e.rows.push_back({7}); // 16384
    b = encoded(e);
    CHECK(b.size() == at + 3 + 16384 && b[at] == 0x80 && b[at + 1] == 0x80 && b[at + 2] == 0x01);
    static rows::Eights out;
    const auto r = nnh::save::decode(b.data(), b.size(), out);
    CHECK(r.ok() && out.rows.size() == 256 && out.rows[255].v0 == 7 && out.rows[5].v0 == (5ull << 33));
}
