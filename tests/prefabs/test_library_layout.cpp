// A prefab library's block layout (spec 021 T2.1, FR-1, II): aligned parts, totals by formula, the name table's size,
// the per-prefab and per-entry costs the plan states.

#include "prefabs/library_layout.hpp"

#include "prefab_fixture.hpp"

using nnh::detail::library_layout;
using nnh::detail::PrefabEntry;
using nnh::detail::PrefabRecord;

namespace {

std::size_t expected_total(int prefabs, int entries, int values, bool names) {
    const auto l = library_layout({.max_prefabs = prefabs, .max_entries = entries, .value_bytes = values}, names);
    std::size_t at = sizeof(nnh::detail::PrefabBlock) + 4u * prefabs + 2u * l.table_size;
    at = (at + alignof(PrefabRecord) - 1) / alignof(PrefabRecord) * alignof(PrefabRecord);
    at += sizeof(PrefabRecord) * prefabs + (names ? 32u * prefabs : 0u);
    at = (at + alignof(PrefabEntry) - 1) / alignof(PrefabEntry) * alignof(PrefabEntry);
    at += sizeof(PrefabEntry) * entries;
    at = (at + 15) / 16 * 16;
    return at + values;
}

} // namespace

NNH_TEST(prefab_records_and_entries_have_the_plans_sizes) {
    CHECK(sizeof(PrefabRecord) == (sizeof(void*) == 4 ? 72u : 80u));
    CHECK(sizeof(PrefabEntry) == 4 * sizeof(void*));
}

NNH_TEST(library_layout_aligns_each_part_and_sums_by_formula) {
    for (bool names : {false, true}) {
        const auto l = library_layout({.max_prefabs = 5, .max_entries = 7, .value_bytes = 33}, names);
        CHECK(l.hashes % 4 == 0 && l.table == l.hashes + 4 * 5);
        CHECK(l.records % alignof(PrefabRecord) == 0 && l.records >= l.table + 2 * 16);
        CHECK(l.entries % alignof(PrefabEntry) == 0 && l.values % 16 == 0);
        CHECK(l.entries >= l.names + (names ? 32u * 5 : 0u));
        CHECK(l.total == expected_total(5, 7, 33, names));
    }
}

NNH_TEST(library_layout_sizes_the_name_table_to_a_power_of_two_at_least_twice_the_prefabs) {
    CHECK(library_layout({.max_prefabs = 1}, false).table_size == 2);
    CHECK(library_layout({.max_prefabs = 5}, false).table_size == 16);
    CHECK(library_layout({.max_prefabs = 8}, false).table_size == 16);
    CHECK(library_layout({.max_prefabs = 1024}, false).table_size == 2048);
    CHECK(library_layout({.max_prefabs = 1024, .max_entries = 65535, .value_bytes = 1 << 20}, true).total ==
          expected_total(1024, 65535, 1 << 20, true));
}

NNH_TEST(library_layout_with_no_entries_or_values_ends_at_the_values) {
    const auto l = library_layout({.max_prefabs = 3}, false);
    CHECK(l.values >= l.entries && l.values - l.entries < 16 && l.total == l.values);
}

NNH_TEST(library_layout_of_the_plans_example_is_about_26_kb_on_the_3ds) {
    // 128 prefabs × 4 components of 16 bytes (Release): 128 × (72 + 4 + 4) + 512 × 16 + 8192 + the header on the 3DS.
    const auto l = library_layout({.max_prefabs = 128, .max_entries = 512, .value_bytes = 128 * 4 * 16}, false);
    CHECK(l.total == expected_total(128, 512, 8192, false));
    const std::size_t on_3ds = 128 * (72 + 4 + 4) + 512 * 16 + 8192;
    CHECK(on_3ds == 26624);
    if (sizeof(void*) == 4) CHECK(l.total > on_3ds && l.total < on_3ds + 256);
}
