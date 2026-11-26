// Mini-fuzz of the localization validators (spec 029 T6): the four-language fixture's tables and manifest mutated
// 20 000 times each, re-sealed so mutations reach the deep checks; the seeds themselves must be accepted.

#include <cstdio>
#include <vector>

#include "../loc/loc_fixture.hpp"
#include "fuzz_targets.hpp"
#include "test.hpp"

namespace {

template <typename Target, typename Accepted>
int mutate(const std::vector<std::uint8_t>& seed, Target target, Accepted accepted_fn, std::uint32_t state) {
    int accepted = 0;
    std::vector<std::uint8_t> buf;
    for (int i = 0; i < 20000; ++i) {
        buf = seed;
        const int flips = 1 + static_cast<int>((state = state * 1664525u + 1013904223u) >> 30);
        for (int f = 0; f < flips; ++f) {
            state = state * 1664525u + 1013904223u;
            buf[(state >> 8) % buf.size()] ^= static_cast<std::uint8_t>(1u << (state & 7u));
        }
        if ((state & 0x300u) == 0) buf.resize(buf.size() - (state >> 12) % 16);
        target(buf.data(), buf.size());
        accepted += accepted_fn(buf.data(), buf.size()) ? 1 : 0;
    }
    return accepted;
}

} // namespace

NNH_TEST(loc_table_target_survives_mutated_tables) {
    const auto seed = loc_test::write_table(loc_test::four_languages().languages[0]);
    CHECK(nnh::fuzz::loc_table_fixed_accepted(seed.data(), seed.size()));
    const int accepted = mutate(seed, nnh::fuzz::loc_table_target, nnh::fuzz::loc_table_fixed_accepted, 29);
    std::printf("    %d of 20000 mutated tables accepted\n", accepted);
    CHECK(accepted > 0);
}

NNH_TEST(loc_manifest_target_survives_mutated_manifests) {
    loc_test::LocBuilder b = loc_test::four_languages();
    b.languages[0].font("body", "fonts/latin");
    const auto seed = b.manifest();
    CHECK(nnh::fuzz::loc_manifest_fixed_accepted(seed.data(), seed.size()));
    const int accepted = mutate(seed, nnh::fuzz::loc_manifest_target, nnh::fuzz::loc_manifest_fixed_accepted, 2950);
    std::printf("    %d of 20000 mutated manifests accepted\n", accepted);
    CHECK(accepted > 0);
}
