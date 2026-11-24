// The schema-free validator of structured saves (spec 027 T1.2, FR-5, FR-14): hand-written payloads (the root's
// fields, after the structured header), every bound both ways, messages with the field's path, linear time.

#include <cstdint>
#include <cstring>
#include <vector>

#include "saves/save_validate.hpp"
#include "test.hpp"

namespace {

using Bytes = std::vector<std::uint8_t>;

bool accepted(const Bytes& b) { return nnh::detail::save_validate_fields(b.data(), b.size()).ok(); }

bool refused(const Bytes& b, const char* message) {
    const auto r = nnh::detail::save_validate_fields(b.data(), b.size());
    if (r.ok() || r.error().code != nnh::ErrorCode::Corrupted) return false;
    if (std::strcmp(r.error().message, message) == 0) return true;
    std::printf("    got \"%s\"\n", r.error().message);
    return false;
}

void append(Bytes& out, const Bytes& more) { out.insert(out.end(), more.begin(), more.end()); }

/// `levels` struct values nested inside each other (field id 1 each), the innermost holding one u8 field.
Bytes nested_structs(int levels) {
    Bytes inner = {0x02, 1, 7};
    for (int i = 0; i < levels; ++i) {
        Bytes outer = {0x13, 1, static_cast<std::uint8_t>(inner.size())};
        append(outer, inner);
        inner = outer;
    }
    return inner;
}

} // namespace

NNH_TEST(save_validator_accepts_no_fields) { CHECK(accepted({})); }

NNH_TEST(save_validator_accepts_every_one_value_kind) {
    const Bytes b = {
        0x01, 1,  0x01,                                  // 1 bool
        0x02, 2,  0xFF,                                  // 2 u8
        0x03, 3,  0x80,                                  // 3 i8
        0x04, 4,  0x34, 0x12,                            // 4 u16
        0x05, 5,  0xFF, 0xFF,                            // 5 i16
        0x06, 6,  1,    2,    3, 4,                      // 6 u32
        0x07, 7,  1,    2,    3, 4,                      // 7 i32
        0x08, 8,  1,    2,    3, 4, 5, 6, 7, 8,          // 8 u64
        0x09, 9,  1,    2,    3, 4, 5, 6, 7, 8,          // 9 i64
        0x0A, 10, 0,    0,    0xC0, 0x3F,                // 10 f32 1.5
        0x0B, 11, 0,    0,    0, 0, 0, 0, 0xF8, 0x3F,    // 11 f64 1.5
        0x0C, 12, 0,    0,    0x80, 0x3F, 0, 0, 0, 0x40, // 12 vec2 (1, 2)
        0x0D, 13, 1,    2,    3, 4,                      // 13 color
        0x0E, 14, 1,    2,    3, 4,                      // 14 data ref
        0x0F, 15, 1,    2,    3, 4,                      // 15 texture ref
        0x10, 16, 1,    2,    3, 4,                      // 16 sound ref
        0x11, 17, 1,    2,    3, 4,                      // 17 music ref
        0x12, 18, 3,    'a',  'b', 'c',                  // 18 string "abc"
        0x13, 19, 3,    0x02, 1, 7,                      // 19 struct {1 u8 7}
        0x02, 0x80, 0x01, 9,                             // 128 u8 (a two-byte id)
    };
    CHECK(accepted(b));
}

NNH_TEST(save_validator_accepts_every_sequence_shape) {
    const Bytes b = {
        0x24, 1, 3, 1, 0, 2, 0, 3, 0,                    // 1 u16[3]
        0x41, 2, 0,                                      // 2 empty list of bool
        0x52, 3, 2, 5, 1, 'a', 2, 'b', 'c',              // 3 list of 2 strings, 5 bytes
        0x53, 4, 2, 2, 0x02, 1, 0x12, 2, 6, 5, 1, 'x', 6, 1, 'y', // 4 list of 2 structs {1 u8, 2 string}
        0x53, 5, 0, 1, 0x01, 1, 0,                       // 5 empty list of structs: still a template
        0x33, 6, 1, 1, 0x44, 3, 5, 2, 1, 0, 2, 0,        // 6 struct[1] {3 list<u16>[2]}
    };
    CHECK(accepted(b));
}

NNH_TEST(save_validator_refuses_bad_wire_bytes) {
    CHECK(refused({0x14, 1, 0}, "field ?: bad wire byte 0x14"));
    CHECK(refused({0x00, 1, 0}, "field ?: bad wire byte 0x00"));
    CHECK(refused({0x62, 1, 0}, "field ?: bad wire byte 0x62"));
    CHECK(refused({0x82, 1, 0}, "field ?: bad wire byte 0x82"));
}

NNH_TEST(save_validator_refuses_bad_and_unordered_ids) {
    CHECK(refused({0x02, 0, 5}, "field ?: bad id"));
    CHECK(refused({0x02, 0x80, 0x80, 0x04, 5}, "field ?: bad id")); // 65536
    CHECK(refused({0x02, 0x80, 0x00, 5}, "field ?: bad id"));       // overlong
    CHECK(refused({0x02}, "field ?: bad id"));                       // cut
    CHECK(accepted({0x02, 0xFF, 0xFF, 0x03, 5}));                    // 65535
    CHECK(refused({0x02, 2, 0, 0x02, 1, 0}, "field 1: id not ascending"));
    CHECK(refused({0x02, 2, 0, 0x02, 2, 0}, "field 2: id not ascending"));
}

NNH_TEST(save_validator_refuses_values_cut_by_the_end) {
    CHECK(refused({0x06, 1, 0, 0}, "field 1: value cut short"));
    CHECK(refused({0x0B, 1, 0}, "field 1: value cut short"));
    CHECK(refused({0x12, 1, 5, 'a'}, "field 1: string cut short"));
    CHECK(refused({0x12, 1}, "field 1: string cut short"));
    CHECK(refused({0x13, 1, 9, 0x02, 1, 7}, "field 1: struct length exceeds the file"));
    CHECK(refused({0x13, 1}, "field 1: bad length"));
}

NNH_TEST(save_validator_refuses_bad_strings) {
    CHECK(refused({0x12, 1, 1, 0xFF}, "field 1: string is not UTF-8"));
    CHECK(refused({0x12, 1, 2, 0xC3, 'a'}, "field 1: string is not UTF-8"));
    CHECK(refused({0x12, 1, 2, 'a', 0}, "field 1: string holds a NUL"));
    CHECK(accepted({0x12, 1, 2, 0xC3, 0xA9})); // é
    CHECK(accepted({0x12, 1, 0}));            // empty
}

NNH_TEST(save_validator_checks_struct_contents_with_their_path) {
    CHECK(refused({0x13, 4, 3, 0x02, 0, 7}, "field 4.?: bad id"));
    CHECK(refused({0x13, 4, 2, 0x06, 2}, "field 4.2: value cut short"));
    CHECK(accepted({0x13, 4, 3, 0x02, 2, 7, 0x02, 5, 1})); // field 5 follows the struct: ascending at the root
}

NNH_TEST(save_validator_allows_eight_levels_and_refuses_nine) {
    CHECK(accepted(nested_structs(7))); // root + 7 nested structs = 8 levels
    CHECK(refused(nested_structs(8), "field 1.1.1.1.1.1.1.1: nesting deeper than 8 levels"));
}

NNH_TEST(save_validator_refuses_counts_beyond_the_file) {
    // field 7: u32 list of 900 items with 4 bytes left
    CHECK(refused({0x46, 7, 0x84, 0x07, 1, 2, 3, 4}, "field 7: list count 900 exceeds the file"));
    CHECK(refused({0x26, 7, 2, 1, 2, 3, 4}, "field 7: list count 2 exceeds the file"));
    CHECK(refused({0x26, 7}, "field 7: bad count"));
    CHECK(refused({0x26, 7, 0x80, 0x80, 0x04}, "field 7: bad count")); // 65536 items
    CHECK(refused({0x52, 3, 3, 2, 0, 0}, "field 3: list count 3 exceeds the file")); // 3 strings in 2 bytes
}

NNH_TEST(save_validator_refuses_sequence_bytes_that_do_not_match_their_items) {
    CHECK(refused({0x52, 3, 2, 6, 1, 'a', 2, 'b', 'c'}, "field 3: length 6 exceeds the file"));
    CHECK(refused({0x52, 3, 2, 4, 1, 'a', 2, 'b', 'c'}, "field 3[1]: string cut short"));
    CHECK(refused({0x52, 3, 1, 3, 1, 'a', 0x02, 4, 0}, "field 3: items don't fill their length"));
    CHECK(refused({0x53, 4, 1, 1, 0x02, 1, 2, 5, 0x02, 9, 0}, "field 4: items don't fill their length"));
}

NNH_TEST(save_validator_checks_templates) {
    CHECK(refused({0x53, 4, 0, 0, 0}, "field 4: template of 0 fields"));
    Bytes big = {0x53, 4, 0, 0x81, 0x01}; // 129 fields
    for (int i = 1; i <= 129; ++i) {
        big.push_back(0x01);
        big.push_back(static_cast<std::uint8_t>(i));
    }
    big.push_back(0);
    CHECK(refused(big, "field 4: template of 129 fields"));
    CHECK(refused({0x53, 4, 0, 2, 0x01, 2, 0x01, 1, 0}, "field 4: template ids not ascending"));
    CHECK(refused({0x53, 4, 0, 1, 0x7F, 1, 0}, "field 4: template: bad wire byte 0x7F"));
    CHECK(refused({0x53, 4, 0, 1, 0x01, 0, 0}, "field 4: template: bad id"));
}

NNH_TEST(save_validator_names_the_item_and_field_of_a_bad_row_value) {
    // field 9: list of 13 structs {1 u8, 4 string}; row 12's string is not UTF-8
    Bytes b = {0x53, 9, 13, 2, 0x02, 1, 0x12, 4, 13 * 3};
    for (int i = 0; i < 13; ++i) {
        b.push_back(static_cast<std::uint8_t>(i));
        b.push_back(1);
        b.push_back(i == 12 ? 0xFF : 'a');
    }
    CHECK(refused(b, "field 9[12].4: string is not UTF-8"));
}

NNH_TEST(save_validator_counts_struct_rows_as_a_level) {
    // 6 nested structs (7 levels), then a list of structs whose rows would be level 9: refused
    Bytes inner = {0x53, 1, 1, 1, 0x13, 1, 4, 3, 0x02, 1, 7}; // list of 1 struct {1 struct {1 u8}}
    for (int i = 0; i < 5; ++i) {
        Bytes outer = {0x13, 1, static_cast<std::uint8_t>(inner.size())};
        append(outer, inner);
        inner = outer;
    }
    CHECK(accepted(inner)); // root, 5 structs, the row, its struct: 8 levels
    Bytes deeper = {0x13, 1, static_cast<std::uint8_t>(inner.size())};
    append(deeper, inner);
    CHECK(refused(deeper, "field 1.1.1.1.1.1.1[0].1: nesting deeper than 8 levels"));
}

NNH_TEST(save_validator_runs_in_linear_time_on_an_adversarial_file) {
    // 256 KiB: a list of struct rows whose template has 128 bool fields, rows of 128 bytes each.
    const std::uint32_t rows = (256 * 1024 - 400) / 128;
    Bytes b = {0x53, 1, static_cast<std::uint8_t>(rows | 0x80), static_cast<std::uint8_t>(rows >> 7), 0x80, 0x01};
    for (int i = 1; i <= 128; ++i) {
        b.push_back(0x01);
        if (i < 128) {
            b.push_back(static_cast<std::uint8_t>(i));
        } else {
            b.push_back(0x80);
            b.push_back(0x01);
        }
    }
    const std::uint32_t bytes = rows * 128;
    b.push_back(static_cast<std::uint8_t>(bytes | 0x80));
    b.push_back(static_cast<std::uint8_t>((bytes >> 7) | 0x80));
    b.push_back(static_cast<std::uint8_t>(bytes >> 14));
    b.resize(b.size() + bytes, 0x01);
    std::uint32_t steps = 0;
    CHECK(nnh::detail::save_validate_fields(b.data(), b.size(), &steps).ok());
    CHECK(steps <= 2 * b.size());
}
