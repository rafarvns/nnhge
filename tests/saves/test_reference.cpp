// The reference save (spec 027 T6.1, AC-2): examples/saves' Progress and its history; the encoded sizes of the three
// versions; a v1 value loaded by the v3 code equals the migration done by hand; the checksum reads values, not bytes.

#include <cstdio>

#include "saves/progress_sum.hpp"
#include "saves/reference.hpp"
#include "saves_fixture.hpp"

using saves_test::Bytes;
using saves_test::encoded;

namespace ex = saves_example;

NNH_TEST(save_reference_sizes_are_those_of_the_plan) {
    const std::size_t v1 = encoded(ex::reference_v1()).size();
    const std::size_t v2 = encoded(ex::reference_v2()).size();
    const std::size_t v3 = encoded(ex::reference_v3()).size();
    std::printf("    reference sizes: v1 %u, v2 %u, v3 %u, v3 max %u\n", static_cast<unsigned>(v1),
                static_cast<unsigned>(v2), static_cast<unsigned>(v3),
                static_cast<unsigned>(nnh::save::max_size<ex::Progress>()));
    CHECK(v1 > 1500 && v1 < 2500); // 1769 at stage 6 (plan: ~2.5 KB; the items' and quests' texts are short)
    CHECK(v2 > 3200 && v2 < 4200); // 3590
    CHECK(v3 > 3400 && v3 < 4400 && nnh::save::max_size<ex::Progress>() < 5200); // 3768
}

NNH_TEST(save_reference_v1_loads_into_v3_as_migrated_by_hand) {
    const Bytes b = encoded(ex::reference_v1());
    static ex::Progress loaded;
    const auto r = nnh::save::decode(b.data(), b.size(), loaded);
    CHECK(r.ok() && r.value().migrated == 2 && r.value().unknown == 0 && r.value().dropped == 0);
    CHECK(r.value().adjusted == 0 && r.value().version == 1);
    static const ex::Progress expected = ex::reference_v1_migrated();
    CHECK(ex::progress_sum(loaded) == ex::progress_sum(expected));
    CHECK(encoded(loaded) == encoded(expected));
    CHECK(loaded.inventory.size() == 33 && loaded.inventory[0].kind == 501 && loaded.inventory[0].equipped);
    CHECK(loaded.gems == 12345 && loaded.room == 0 && loaded.play_frames == 3600000);
}

NNH_TEST(save_reference_v2_loads_into_v3_with_one_step) {
    const Bytes b = encoded(ex::reference_v2());
    static ex::Progress loaded;
    const auto r = nnh::save::decode(b.data(), b.size(), loaded);
    CHECK(r.ok() && r.value().migrated == 1 && r.value().unknown == 0 && r.value().dropped == 0);
    CHECK(loaded.room == 12 && loaded.tile == 345 && loaded.gems == 12345 && loaded.hero.defense == 3);
    CHECK(loaded.play_frames == 5000000000ull && loaded.best_time == 123.456 && loaded.bestiary[511] != 0);
}

NNH_TEST(save_reference_v3_round_trips_and_keeps_its_subnormal) {
    static const ex::Progress in = ex::reference_v3();
    const Bytes b = encoded(in);
    static ex::Progress out;
    CHECK(nnh::save::decode(b.data(), b.size(), out).ok());
    CHECK(ex::progress_sum(out) == ex::progress_sum(in) && encoded(out) == b);
    std::uint32_t wear = 0;
    std::memcpy(&wear, &out.inventory[5].wear, 4);
    CHECK(wear == 0x00000123u);
}

NNH_TEST(save_reference_sum_reads_values_not_padding) {
    static ex::Progress a = ex::reference_v3();
    static ex::Progress b;
    std::memset(static_cast<void*>(&b), 0xFF, sizeof b); // padding bytes differ from a's
    b = a;
    CHECK(ex::progress_sum(a) == ex::progress_sum(b));
    b.inventory[63].label.assign("x");
    CHECK(ex::progress_sum(a) != ex::progress_sum(b));
}
