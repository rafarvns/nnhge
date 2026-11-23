// A library's queries and stats (spec 021 T2.5, FR-11, FR-3): count, capacity, valid, name, kinds, tags, has, value,
// stats; invalid prefabs answer nothing.

#include "prefab_fixture.hpp"

#include <string_view>

using nnh::Prefab;
using nnh::PrefabLibrary;
using namespace prefab_test;

NNH_TEST(library_queries_answer_count_capacity_and_validity) {
    PrefabLibrary lib = make_library({.max_prefabs = 4, .max_entries = 8, .value_bytes = 128});
    PrefabLibrary other = make_library({.max_prefabs = 4, .max_entries = 8, .value_bytes = 128});
    CHECK(lib.count() == 0 && lib.capacity() == 4);
    const Prefab a = must_add(lib, "a", {}, Coin{1});
    const Prefab b = must_add(other, "b", {}, Coin{1});
    CHECK(lib.count() == 1 && lib.valid(a) && !lib.valid(Prefab{}) && !lib.valid(b));
    CHECK(!lib.valid(Prefab::from_bits(a.bits() + 1))); // index 1: not registered yet
    CHECK(!lib.valid(Prefab::from_bits(a.bits() ^ 0x10000u))); // another stamp
}

NNH_TEST(library_kinds_and_tags_fill_up_to_capacity_and_return_the_total) {
    PrefabLibrary lib = make_library();
    const Prefab p = must_add(lib, "p", {.tags = {kEnemy, kFlying, kFx}}, Body{}, Coin{2}, Bob{3});
    nnh::KindId kinds[2] = {};
    CHECK(lib.kinds(p, kinds, 2) == 3 && kinds[0] == nnh::kind_id<Body>() && kinds[1] == nnh::kind_id<Coin>());
    nnh::Tag tags[1] = {};
    CHECK(lib.tags(p, tags, 1) == 3 && tags[0] == kEnemy);
    CHECK(lib.kinds(Prefab{}, kinds, 2) == 0 && lib.tags(Prefab{}, tags, 1) == 0);
    CHECK(lib.kinds(p, nullptr, 0) == 3 && lib.tags(p, nullptr, 0) == 3);
}

NNH_TEST(library_has_and_value_read_the_stored_values) {
    PrefabLibrary lib = make_library();
    const Prefab p = must_add(lib, "p", {}, Body{{1, 2, 3, 4}, 5}, Coin{2});
    CHECK(lib.has<Body>(p) && lib.has<Coin>(p) && !lib.has<Bob>(p) && !lib.has<Coin>(Prefab{}));
    CHECK(lib.value<Coin>(p)->value == 2 && lib.value<Body>(p)->box.y == 2 && lib.value<Bob>(p) == nullptr);
}

NNH_TEST(library_stats_count_registrations_and_capacities) {
    PrefabLibrary lib = make_library({.max_prefabs = 4, .max_entries = 8, .value_bytes = 128});
    must_add(lib, "p", {}, Body{}, Coin{2});
    must_add(lib, "q", {}, Big{});
    const nnh::PrefabStats s = lib.stats();
    CHECK(s.prefabs == 2 && s.capacity == 4 && s.entries == 3 && s.entry_capacity == 8);
    CHECK(s.value_bytes == 20 + 4 + 64 && s.value_capacity == 128);
    CHECK(s.spawns == 0 && s.misses == 0 && s.map_skipped == 0 && s.map_unknown == 0);
}

NNH_TEST(an_invalid_prefab_has_no_name_and_no_values) {
    PrefabLibrary lib = make_library();
    must_add(lib, "p", {}, Coin{2});
    const Prefab bogus = Prefab::from_bits(0x00010005u);
    CHECK(std::string_view(lib.name(bogus)).empty() && lib.value<Coin>(bogus) == nullptr && !lib.has<Coin>(bogus));
}
