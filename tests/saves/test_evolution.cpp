// Automatic evolution (spec 027 T4.2, FR-2, FR-10, FR-18): a file written by an older declaration of the same save
// type loads into the new one without code: added fields keep their defaults, removed ones are skipped (counted
// unknown unless retired), renamed and moved members load by id, numbers widen and change signedness when they fit,
// integers become floats when exact, float widens to double on its bits, arrays and lists grow and convert into each
// other, strings grow, nested structs evolve by their own ids.

#include <cstring>

#include "saves_fixture.hpp"

using saves_test::encoded;

namespace evo {

struct PairV1 {
    std::uint8_t a = 1;
    std::uint8_t gone = 3;
};
NNH_SAVE_STRUCT(PairV1, NNH_SAVE_FIELD(a, 1), NNH_SAVE_FIELD(gone, 3));
struct PairV2 { // `a` renamed to `first`, field 3 removed, field 2 added
    std::int16_t second = 42;
    std::uint16_t first = 0;
};
NNH_SAVE_STRUCT(PairV2, NNH_SAVE_FIELD(first, 1), NNH_SAVE_FIELD(second, 2));

struct HeroV1 {
    std::uint8_t level = 0;
    std::uint32_t coins = 0;
    std::int16_t hp = 0;
    std::uint32_t big = 0;
    std::uint8_t unsigned_small = 0;
    std::int32_t negative = 0;
    std::uint32_t as_float = 0;
    std::int64_t as_double = 0;
    float tiny = 0;
    nnh::data::String<4> name;
    std::uint8_t bag[2] = {};
    nnh::data::List<std::uint16_t, 2> list;
    std::uint8_t to_list[3] = {};
    PairV1 pair;
    nnh::data::List<PairV1, 2> pairs;
    std::uint32_t old_retired = 0;
};
NNH_SAVE_TYPE(HeroV1, "hero", NNH_SAVE_FIELD(level, 1), NNH_SAVE_FIELD(coins, 2), NNH_SAVE_FIELD(hp, 3),
              NNH_SAVE_FIELD(big, 4), NNH_SAVE_FIELD(unsigned_small, 5), NNH_SAVE_FIELD(negative, 6),
              NNH_SAVE_FIELD(as_float, 7), NNH_SAVE_FIELD(as_double, 8), NNH_SAVE_FIELD(tiny, 9),
              NNH_SAVE_FIELD(name, 10), NNH_SAVE_FIELD(bag, 11), NNH_SAVE_FIELD(list, 12), NNH_SAVE_FIELD(to_list, 13),
              NNH_SAVE_FIELD(pair, 14), NNH_SAVE_FIELD(pairs, 15), NNH_SAVE_FIELD(old_retired, 16));

struct HeroV2 { // members reordered and renamed; 2 removed (unknown), 16 retired, 17 added
    PairV2 duo;
    nnh::data::List<PairV2, 4> duos;
    nnh::data::List<std::uint8_t, 4> from_array;
    std::uint32_t grown[4] = {7, 7, 7, 7};
    nnh::data::String<12> title;
    double tiny_wide = 0;
    double exact_double = 0;
    float exact_float = 0;
    std::int8_t small_signed = 0;
    std::int16_t signed_from_unsigned = 0;
    std::uint64_t big_wide = 0;
    std::int64_t hp_wide = 0;
    std::uint16_t level = 0;
    std::uint32_t gems = 7;
    nnh::data::List<std::uint32_t, 5> list_wide;
};
NNH_SAVE_TYPE(HeroV2, "hero", NNH_SAVE_FIELD(gems, 17), NNH_SAVE_FIELD(level, 1), NNH_SAVE_FIELD(hp_wide, 3),
              NNH_SAVE_FIELD(big_wide, 4), NNH_SAVE_FIELD(signed_from_unsigned, 5), NNH_SAVE_FIELD(small_signed, 6),
              NNH_SAVE_FIELD(exact_float, 7), NNH_SAVE_FIELD(exact_double, 8), NNH_SAVE_FIELD(tiny_wide, 9),
              NNH_SAVE_FIELD(title, 10), NNH_SAVE_FIELD(grown, 11), NNH_SAVE_FIELD(list_wide, 12),
              NNH_SAVE_FIELD(from_array, 13), NNH_SAVE_FIELD(duo, 14), NNH_SAVE_FIELD(duos, 15), NNH_SAVE_RETIRED(16));

HeroV1 old_hero() {
    HeroV1 h;
    h.level = 200;
    h.coins = 1234;
    h.hp = -300;
    h.big = 0xFFFFFFF0u;
    h.unsigned_small = 200;
    h.negative = -5;
    h.as_float = 1u << 24;
    h.as_double = -(1ll << 40) - 1;
    const std::uint32_t subnormal = 1; // 2^-149, the smallest float
    std::memcpy(&h.tiny, &subnormal, 4);
    h.name.assign("Bo\xC3\xA9");
    h.bag[0] = 1;
    h.bag[1] = 2;
    h.list.push_back(10);
    h.list.push_back(20);
    h.to_list[0] = 4;
    h.to_list[1] = 5;
    h.to_list[2] = 6;
    h.pair = {9, 8};
    h.pairs.push_back({1, 1});
    h.pairs.push_back({2, 2});
    h.old_retired = 77;
    return h;
}

} // namespace evo

using evo::HeroV2;

namespace {

HeroV2 load_v2(nnh::save::Report* report = nullptr) {
    const saves_test::Bytes b = encoded(evo::old_hero());
    HeroV2 h;
    const auto r = nnh::save::decode(b.data(), b.size(), h);
    CHECK(r.ok());
    if (report != nullptr && r.ok()) *report = r.value();
    return h;
}

} // namespace

NNH_TEST(save_evolution_keeps_the_default_of_an_added_field) { CHECK(load_v2().gems == 7); }

NNH_TEST(save_evolution_counts_a_removed_field_as_unknown_and_a_retired_one_not) {
    nnh::save::Report r;
    load_v2(&r);
    // field 2 (coins), the nested 14.3 (PairV1::gone) and 15[i].3 in each of the two rows; 16 is retired: silent
    CHECK(r.unknown == 4);
    CHECK(r.dropped == 0 && r.adjusted == 0);
    if (nnh::build::kAssertsEnabled) {
        CHECK(r.unknown_at[0] == nnh::save::path(2) && r.unknown_at[1] == nnh::save::path(14, 3));
        CHECK(r.unknown_at[2] == nnh::save::path(15).item(0).field(3));
    } else {
        CHECK(r.unknown_at[0].size() == 0); // Release keeps no paths
    }
}

NNH_TEST(save_evolution_loads_renamed_and_moved_members_by_id) {
    const HeroV2 h = load_v2();
    CHECK(h.level == 200 && h.duo.first == 9 && h.duo.second == 42); // 14.2 added in PairV2: its default
}

NNH_TEST(save_evolution_widens_integers_and_changes_their_sign_when_they_fit) {
    const HeroV2 h = load_v2();
    CHECK(h.hp_wide == -300 && h.big_wide == 0xFFFFFFF0u);    // i16 -> i64, u32 -> u64
    CHECK(h.signed_from_unsigned == 200 && h.small_signed == -5); // u8 200 -> i16, i32 -5 -> i8
}

NNH_TEST(save_evolution_turns_integers_into_floats_when_exact) {
    const HeroV2 h = load_v2();
    CHECK(h.exact_float == 16777216.0f);                   // u32 2^24 -> float
    CHECK(h.exact_double == -1099511627777.0);             // i64 -(2^40 + 1) -> double: 41 significant bits
}

NNH_TEST(save_evolution_widens_a_subnormal_float_to_double_on_its_bits) {
    const HeroV2 h = load_v2();
    std::uint64_t bits = 0;
    std::memcpy(&bits, &h.tiny_wide, 8);
    CHECK(bits == 0x36A0000000000000ull); // 2^-149 as a double: exponent -149 + 1023 = 0x36A
}

NNH_TEST(save_evolution_grows_strings_arrays_and_lists) {
    const HeroV2 h = load_v2();
    CHECK(h.title == "Bo\xC3\xA9");
    CHECK(h.grown[0] == 1 && h.grown[1] == 2 && h.grown[2] == 7 && h.grown[3] == 7); // the rest keep their defaults
    CHECK(h.list_wide.size() == 2 && h.list_wide[0] == 10 && h.list_wide[1] == 20); // u16 list -> u32 list
}

NNH_TEST(save_evolution_turns_arrays_into_lists_and_back) {
    const HeroV2 h = load_v2();
    CHECK(h.from_array.size() == 3 && h.from_array[0] == 4 && h.from_array[2] == 6); // u8[3] -> List<u8, 4>
    CHECK(h.duos.size() == 2 && h.duos[1].first == 2 && h.duos[1].second == 42);    // rows by their ids
}

NNH_TEST(save_evolution_is_the_same_for_a_file_loaded_back_into_the_old_type) {
    const saves_test::Bytes b = encoded(evo::old_hero());
    evo::HeroV1 again;
    CHECK(nnh::save::decode(b.data(), b.size(), again).ok());
    CHECK(encoded(again) == b);
}
