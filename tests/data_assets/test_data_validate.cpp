// The bounded validator of converted data assets (spec 014 FR-10, FR-11, FR-14; constitution X): after it
// accepts a file, every field index, value, string, enum index and list count in it is valid for the game's
// compiled type, so applying it stays in bounds and gives values the schema allows. Files are built with
// data_file::Writer against the tests' schema (test_types.hpp): Enemy's fields are 0 name, 1 stats {0 hp, 1
// attack}, 2 speed, 3 element, 4 sprite, 5 hurt, 6 drops [{0 item, 1 chance}], 7 tint, 8 spawn, 9 flags[3],
// 10 level, 11 theme.

#include <cmath>
#include <cstring>
#include <limits>

#include "data/data_format.hpp"
#include "data/data_validate.hpp"
#include "data/schema_fingerprint.hpp"
#include "data/schema_registry.hpp"
#include "data_file.hpp"
#include "test.hpp"
#include "test_types.hpp"

using data_file::Bytes;
using data_file::Writer;
using nnh::detail::data_registry;
using nnh::detail::DataCheck;
using nnh::detail::DataProblem;
using nnh::detail::TypeInfo;
namespace fmt = nnh::detail::datafmt;

namespace {

const TypeInfo& enemy() { return *find_type(data_registry(), nnh::detail::type_id<test_data::Enemy>()); }
std::uint32_t fingerprint() { return nnh::detail::schema_fingerprint(data_registry(), enemy()); }

DataCheck check(const Bytes& b) {
    return nnh::detail::validate_data(b.data(), b.size(), data_registry(), enemy(), fingerprint());
}
bool accepted(const Bytes& b) { return check(b).ok(); }
bool damaged(const Bytes& b, const char* reason) {
    const DataCheck c = check(b);
    return c.problem == DataProblem::Damaged && std::strcmp(c.reason, reason) == 0;
}

/// Every field of Enemy set once.
Bytes full() {
    Writer w(fingerprint());
    w.fields(12).field(0).str("Goblin");
    w.field(1).fields(2).field(0).u16(30).field(1).u8(0xFB);
    w.field(2).f32(1.25f).field(3).u8(1).field(4).u32(0x11111111u).field(5).u32(0x22222222u);
    w.field(6).u16(2);
    w.fields(2).field(0).u32(0x33333333u).field(1).f32(0.25f);
    w.fields(1).field(0).u32(0x44444444u);
    w.field(7).u8(10).u8(20).u8(30).u8(40).field(8).f32(3).f32(-4);
    w.field(9).u8(1).u8(0).u8(1).field(10).u8(42).field(11).u32(0x55555555u);
    return w.bytes();
}

/// Only the required name, then field `index` written by `value`.
template <class F> Bytes name_and(int index, F value) {
    Writer w(fingerprint());
    w.fields(2).field(0).str("Imp").field(index);
    value(w);
    return w.bytes();
}

Bytes minimal() { return Writer(fingerprint()).fields(1).field(0).str("Imp").bytes(); }

} // namespace

NNH_TEST(data_validate_accepts_every_field_set) { CHECK(accepted(full())); }

NNH_TEST(data_validate_accepts_only_the_required_fields) { CHECK(accepted(minimal())); }

NNH_TEST(data_validate_refuses_a_missing_required_field) {
    CHECK(damaged(Writer(fingerprint()).fields(0).bytes(), "a required field is missing"));
    // Required inside a nested struct that is written: stats without hp.
    CHECK(damaged(name_and(1, [](Writer& w) { w.fields(1).field(1).u8(3); }), "a required field is missing"));
    // ... and inside list items: a drop without its item.
    CHECK(damaged(name_and(6, [](Writer& w) { w.u16(1).fields(1).field(1).f32(0.5f); }),
                  "a required field is missing"));
}

NNH_TEST(data_validate_refuses_bad_headers) {
    CHECK(damaged(Bytes{}, "too short to be a data asset"));
    Bytes cut = minimal();
    cut.resize(fmt::kHeaderSize - 1);
    CHECK(damaged(cut, "too short to be a data asset"));
    Bytes magic = minimal();
    magic[0] = 'X';
    CHECK(damaged(magic, "not a converted data asset"));
    Bytes zero = minimal();
    data_file::put16(zero, fmt::kVersionAt, 0);
    CHECK(damaged(zero, "not a converted data asset"));
    Bytes size = minimal();
    data_file::put32(size, fmt::kSizeAt, static_cast<std::uint32_t>(size.size() + 1));
    CHECK(damaged(size, "size doesn't match the header"));
    Bytes crc = minimal();
    crc.back() ^= 1;
    CHECK(damaged(crc, "CRC mismatch"));
}

NNH_TEST(data_validate_reports_newer_versions_and_flags) {
    Bytes version = minimal();
    data_file::put16(version, fmt::kVersionAt, 2);
    CHECK(check(version).problem == DataProblem::NewerVersion);
    Bytes flags = minimal();
    data_file::put16(flags, fmt::kFlagsAt, 1);
    CHECK(check(flags).problem == DataProblem::NewerVersion);
}

NNH_TEST(data_validate_reports_a_file_converted_for_another_schema) {
    Bytes stale = minimal();
    data_file::put32(stale, fmt::kFingerprintAt, fingerprint() ^ 1);
    const DataCheck c = check(stale);
    CHECK(c.problem == DataProblem::Stale);
    CHECK(std::strcmp(c.reason, "converted for another version of the type: rebuild the assets") == 0);
}

NNH_TEST(data_validate_refuses_files_over_64_kib) {
    Writer w(fingerprint());
    w.fields(1).field(0).str("Imp");
    const std::uint8_t pad[1024] = {};
    for (int i = 0; i < 64; ++i) w.raw(pad, sizeof pad);
    const DataCheck c = check(w.bytes());
    CHECK(c.problem == DataProblem::OverLimit);
}

NNH_TEST(data_validate_refuses_bad_field_indices) {
    const char* reason = "field indices out of order or out of range";
    CHECK(damaged(name_and(12, [](Writer& w) { w.u8(0); }), reason));     // index = field count
    CHECK(damaged(name_and(0, [](Writer& w) { w.str("Imp"); }), reason)); // same index twice
    Writer back(fingerprint());
    back.fields(2).field(10).u8(5).field(0).str("Imp"); // decreasing
    CHECK(damaged(back.bytes(), reason));
    CHECK(damaged(Writer(fingerprint()).fields(13).bytes(), reason)); // more fields than the type has
}

NNH_TEST(data_validate_checks_bools) {
    CHECK(accepted(name_and(9, [](Writer& w) { w.u8(0).u8(1).u8(0); })));
    CHECK(damaged(name_and(9, [](Writer& w) { w.u8(0).u8(2).u8(0); }), "bool is neither 0 nor 1"));
}

NNH_TEST(data_validate_checks_integer_ranges) {
    CHECK(accepted(name_and(10, [](Writer& w) { w.u8(1); })));
    CHECK(accepted(name_and(10, [](Writer& w) { w.u8(99); })));
    CHECK(damaged(name_and(10, [](Writer& w) { w.u8(0); }), "integer out of range"));
    CHECK(damaged(name_and(10, [](Writer& w) { w.u8(100); }), "integer out of range"));
    // hp is a u16 in 1..999; attack an i8 with the full -128..127.
    CHECK(damaged(name_and(1, [](Writer& w) { w.fields(1).field(0).u16(1000); }), "integer out of range"));
    CHECK(accepted(name_and(1, [](Writer& w) { w.fields(2).field(0).u16(999).field(1).u8(0x80); })));
}

NNH_TEST(data_validate_checks_floats) {
    CHECK(accepted(name_and(2, [](Writer& w) { w.f32(0); })));
    CHECK(damaged(name_and(2, [](Writer& w) { w.f32(-0.5f); }), "float out of range"));
    CHECK(damaged(name_and(2, [](Writer& w) { w.f32(std::numeric_limits<float>::infinity()); }),
                  "float out of range"));
    CHECK(damaged(name_and(2, [](Writer& w) { w.f32(std::nanf("")); }), "float out of range"));
    // chance is 0..1; a vec2 takes any finite value.
    CHECK(damaged(name_and(6, [](Writer& w) { w.u16(1).fields(2).field(0).u32(1).field(1).f32(1.5f); }),
                  "float out of range"));
    CHECK(accepted(name_and(8, [](Writer& w) { w.f32(-1e30f).f32(1e30f); })));
    CHECK(damaged(name_and(8, [](Writer& w) { w.f32(0).f32(std::nanf("")); }), "float out of range"));
}

NNH_TEST(data_validate_checks_strings) {
    const char* full24 = "abcdefghijklmnopqrstuvwx";
    CHECK(accepted(Writer(fingerprint()).fields(1).field(0).str(full24).bytes()));
    CHECK(damaged(Writer(fingerprint()).fields(1).field(0).str("abcdefghijklmnopqrstuvwxy").bytes(),
                  "string longer than its capacity"));
    CHECK(damaged(Writer(fingerprint()).fields(1).field(0).u8(3).raw("a\0b", 3).bytes(),
                  "string with a NUL byte"));
    CHECK(damaged(Writer(fingerprint()).fields(1).field(0).u8(2).raw("\xC3(", 2).bytes(),
                  "string isn't UTF-8"));
}

NNH_TEST(data_validate_checks_enum_indices) {
    CHECK(accepted(name_and(3, [](Writer& w) { w.u8(2); })));
    CHECK(damaged(name_and(3, [](Writer& w) { w.u8(3); }), "enum index out of range"));
}

NNH_TEST(data_validate_checks_list_counts) {
    CHECK(accepted(name_and(6, [](Writer& w) { w.u16(0); })));
    CHECK(damaged(name_and(6, [](Writer& w) { w.u16(5); }), "list longer than its capacity"));
}

NNH_TEST(data_validate_refuses_every_truncation) {
    const Bytes good = full();
    for (std::size_t n = fmt::kHeaderSize; n < good.size(); ++n) {
        Bytes cut(good.begin(), good.begin() + static_cast<std::ptrdiff_t>(n));
        data_file::seal(cut);
        CHECK(damaged(cut, "truncated"));
    }
}

NNH_TEST(data_validate_refuses_a_type_whose_schema_is_incomplete) {
    // Enemy without Stats in the registry (a game whose schema check didn't run): a stats value can't be
    // read.
    nnh::detail::DataRegistry partial;
    nnh::detail::TypeNode node{&enemy(), nullptr};
    add_type(partial, node);
    const Bytes b = name_and(1, [](Writer& w) { w.fields(1).field(0).u16(5); });
    const DataCheck c = nnh::detail::validate_data(b.data(), b.size(), partial, enemy(), fingerprint());
    CHECK(c.problem == DataProblem::Damaged);
    CHECK(std::strcmp(c.reason, "the type's schema is incomplete") == 0);
}

NNH_TEST(data_validate_refuses_an_enum_missing_from_the_registry) {
    nnh::detail::DataRegistry partial;
    nnh::detail::TypeNode node{&enemy(), nullptr};
    add_type(partial, node);
    const Bytes b = name_and(3, [](Writer& w) { w.u8(0); });
    const DataCheck c = nnh::detail::validate_data(b.data(), b.size(), partial, enemy(), fingerprint());
    CHECK(c.problem == DataProblem::Damaged && std::strcmp(c.reason, "the type's schema is incomplete") == 0);
}

NNH_TEST(data_validate_refuses_an_integer_width_no_member_has) {
    // Tables read from a damaged description could say so; compiled ones never do.
    static nnh::detail::FieldInfo fields[1];
    fields[0].name = "odd";
    fields[0].kind = nnh::detail::FieldKind::Int;
    fields[0].width = 3;
    fields[0].min = 0;
    fields[0].max = 1e9;
    static TypeInfo odd;
    odd.name = "Odd";
    odd.fields = fields;
    odd.field_count = 1;
    nnh::detail::DataRegistry r;
    nnh::detail::TypeNode node{&odd, nullptr};
    add_type(r, node);
    const std::uint32_t fp = nnh::detail::schema_fingerprint(r, odd);
    const Bytes b = Writer(fp).fields(1).field(0).u8(1).u8(2).u8(3).bytes();
    const DataCheck c = nnh::detail::validate_data(b.data(), b.size(), r, odd, fp);
    CHECK(c.problem == DataProblem::Damaged && std::strcmp(c.reason, "the type's schema is incomplete") == 0);
}

NNH_TEST(data_validate_stops_at_four_nested_levels) {
    // A type nested in itself (only a damaged description can say so): 4 levels below the root read, the 5th
    // not.
    static const char loop_tag = 0;
    static nnh::detail::FieldInfo self[1];
    self[0].name = "self";
    self[0].kind = nnh::detail::FieldKind::Struct;
    self[0].target = &loop_tag;
    static TypeInfo loop;
    loop.tag = &loop_tag;
    loop.name = "Loop";
    loop.fields = self;
    loop.field_count = 1;
    nnh::detail::DataRegistry r;
    nnh::detail::TypeNode node{&loop, nullptr};
    add_type(r, node);
    const std::uint32_t fp = nnh::detail::schema_fingerprint(r, loop);
    const auto nested = [&](int levels) {
        Writer w(fp);
        for (int i = 0; i < levels; ++i) w.fields(1).field(0);
        return w.fields(0).bytes();
    };
    const Bytes four = nested(4);
    CHECK(nnh::detail::validate_data(four.data(), four.size(), r, loop, fp).ok());
    const Bytes five = nested(5);
    const DataCheck c = nnh::detail::validate_data(five.data(), five.size(), r, loop, fp);
    CHECK(c.problem == DataProblem::Damaged && std::strcmp(c.reason, "structs nested too deep") == 0);
}

NNH_TEST(data_validate_refuses_trailing_bytes) {
    Bytes extra = full();
    extra.push_back(0);
    data_file::seal(extra);
    CHECK(damaged(extra, "bytes after the end of the value"));
}
