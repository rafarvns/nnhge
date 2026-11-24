// Decoding (spec 027 T4.1, FR-8, FR-9): every kind and shape round-trips through encode and decode; the struct
// becomes T{} then gets the file's fields; a canary-filled struct is unchanged after each error; nothing allocates.

#include <cstring>

#include "alloc_counter.hpp"
#include "saves_fixture.hpp"

using saves_test::Bytes;
using saves_test::encoded;
using saves_test::header;
using saves_test::Kinds;
using saves_test::operator+;
using saves_test::Pair;
using saves_test::Seqs;

namespace {

struct Defaults {
    std::uint32_t a = 11;
    std::uint32_t b = 22;
    nnh::data::List<Pair, 3> pairs;
};
NNH_SAVE_TYPE(Defaults, "defaults", NNH_SAVE_FIELD(a, 1), NNH_SAVE_FIELD(b, 2), NNH_SAVE_FIELD(pairs, 3));

/// A struct full of 0xA5 bytes, to see that a failed decode wrote nothing.
template <class T> struct Canary {
    alignas(T) unsigned char bytes[sizeof(T)];
    Canary() { std::memset(bytes, 0xA5, sizeof bytes); }
    T& value() { return *reinterpret_cast<T*>(bytes); }
    bool untouched() const {
        for (unsigned char b : bytes) {
            if (b != 0xA5) return false;
        }
        return true;
    }
};

template <class T> bool refused_untouched(const Bytes& b, nnh::ErrorCode code) {
    Canary<T> canary;
    const auto r = nnh::save::decode(b.data(), b.size(), canary.value());
    return !r.ok() && r.error().code == code && canary.untouched();
}

} // namespace

NNH_TEST(save_round_trip_keeps_every_one_value_kind) {
    const Kinds in = saves_test::kinds_sample();
    const Bytes b = encoded(in);
    Kinds out;
    const auto r = nnh::save::decode(b.data(), b.size(), out);
    CHECK(r.ok());
    CHECK(encoded(out) == b);
    CHECK(out.u64 == in.u64 && out.i64 == in.i64 && out.f64 == in.f64 && out.name == "Ana");
    CHECK(out.pair.a == 9 && out.pair.b == -1 && out.big == saves_test::Big::High);
    CHECK(r.value().version == 1 && r.value().migrated == 0 && r.value().unknown == 0);
    CHECK(r.value().dropped == 0 && r.value().adjusted == 0 && !r.value().used_backup);
}

NNH_TEST(save_round_trip_keeps_every_sequence_shape) {
    const Seqs in = saves_test::seqs_sample();
    const Bytes b = encoded(in);
    Seqs out;
    CHECK(nnh::save::decode(b.data(), b.size(), out).ok());
    CHECK(encoded(out) == b);
    CHECK(out.flags.size() == 3 && out.flags[2] && out.words.size() == 3 && out.words[2] == "xyz");
    CHECK(out.pairs.size() == 2 && out.pairs[1].b == 4 && out.grid[1].a == 7 && out.numbers[2] == 0xFFFF);
}

NNH_TEST(save_decode_starts_from_the_struct_defaults) {
    // a file with only field 2: field 1 keeps T{}'s 11 whatever the struct held before
    const Bytes b = header("defaults", 1) + Bytes{0x06, 2, 5, 0, 0, 0};
    Defaults out;
    out.a = 99;
    out.pairs.push_back({3, 3});
    CHECK(nnh::save::decode(b.data(), b.size(), out).ok());
    CHECK(out.a == 11 && out.b == 5 && out.pairs.empty());
}

NNH_TEST(save_decode_gives_list_items_their_defaults) {
    Defaults in;
    in.pairs.push_back({7, 8});
    // field 3 written with a row template holding only field 2: each item's field 1 keeps Pair{}'s 1
    const Bytes b = header("defaults", 1) + Bytes{0x53, 3, 1, 1, 0x05, 2, 2, 8, 0};
    Defaults out;
    out.pairs.push_back({50, 50});
    out.pairs.push_back({60, 60});
    CHECK(nnh::save::decode(b.data(), b.size(), out).ok());
    CHECK(out.pairs.size() == 1 && out.pairs[0].a == 1 && out.pairs[0].b == 8);
    CHECK(out.pairs.begin()[1].a == 1 && out.pairs.begin()[1].b == -2); // the slot freed: Pair{} again
}

NNH_TEST(save_decode_leaves_the_struct_untouched_on_every_error) {
    const Bytes good = encoded(saves_test::kinds_sample());
    using nnh::ErrorCode;
    Bytes cut = good;
    cut.resize(cut.size() - 3);
    CHECK(refused_untouched<Kinds>(cut, ErrorCode::Corrupted));
    Bytes bad = good;
    bad[21] = 0x7F; // the first field's wire byte
    CHECK(refused_untouched<Kinds>(bad, ErrorCode::Corrupted));
    CHECK(refused_untouched<Kinds>(Bytes{1, 2, 3}, ErrorCode::Unsupported));          // no structured header
    CHECK(refused_untouched<Kinds>(header("seqs", 1), ErrorCode::InvalidArgument));   // another save type
    CHECK(refused_untouched<Kinds>(header("kinds", 2), ErrorCode::Unsupported));      // a newer version
    Bytes format2 = good;
    format2[4] = 2;
    CHECK(refused_untouched<Kinds>(format2, ErrorCode::Unsupported));
    Bytes short_header = good;
    short_header.resize(10);
    CHECK(refused_untouched<Kinds>(short_header, ErrorCode::Corrupted));
}

NNH_TEST(save_encode_and_decode_allocate_nothing) {
    const Kinds in = saves_test::kinds_sample();
    const Seqs seqs = saves_test::seqs_sample();
    static std::uint8_t buffer[1024];
    Kinds out;
    Seqs seqs_out;
    nnh::test::alloc_counter::reset();
    nnh::test::alloc_counter::enable(true);
    const auto n = nnh::save::encode(in, buffer, sizeof buffer);
    const bool ok = n.ok() && nnh::save::decode(buffer, n.value(), out).ok();
    const auto m = nnh::save::encode(seqs, buffer, sizeof buffer);
    const bool ok2 = m.ok() && nnh::save::decode(buffer, m.value(), seqs_out).ok();
    nnh::test::alloc_counter::enable(false);
    CHECK(ok && ok2 && nnh::test::alloc_counter::count() == 0);
}
