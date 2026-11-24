#pragma once
// Three versions of one small save type for the migration, view and summary tests (spec 027 T4.4-T4.6):
//   v1: 1 coins u32, 2 weapon u16
//   v2: 1 coins u32, 3 items List<u16, 4> (step 1: the weapon becomes the first item), 2 retired
//   v3: 4 gems u32 (step 2: coins / 10), 3 items, 5 equipped u16 (step 2: the first item), 1 and 2 retired
// The steps write a call log (migration_log) so the tests see their order and what they saw.

#include <cstdint>

#include "nnh/saves.hpp"

namespace mig {

/// Steps run since the last clear: "1" for the step from v1, "2" for the one from v2, with what they saw.
struct Log {
    char text[64] = {};
    int length = 0;
    void add(char c) {
        if (length < 63) text[length++] = c;
    }
    void clear() { *this = {}; }
};
inline Log g_log;

struct V1 {
    std::uint32_t coins = 0;
    std::uint16_t weapon = 0;
};
NNH_SAVE_TYPE(V1, "mig", NNH_SAVE_FIELD(coins, 1), NNH_SAVE_FIELD(weapon, 2));

struct V2 {
    std::uint32_t coins = 0;
    nnh::data::List<std::uint16_t, 4> items;
};
void v2_from_v1(V2& value, const nnh::save::OldFields& old);
NNH_SAVE_TYPE(V2, "mig", NNH_SAVE_VERSION(2), NNH_SAVE_STEP(1, v2_from_v1), NNH_SAVE_FIELD(coins, 1),
              NNH_SAVE_FIELD(items, 3), NNH_SAVE_RETIRED(2));

struct V3 {
    std::uint32_t gems = 0;
    nnh::data::List<std::uint16_t, 4> items;
    std::uint16_t equipped = 0;
};
void v3_from_v1(V3& value, const nnh::save::OldFields& old);
void v3_from_v2(V3& value, const nnh::save::OldFields& old);
NNH_SAVE_TYPE(V3, "mig", NNH_SAVE_VERSION(3), NNH_SAVE_STEP(2, v3_from_v2), NNH_SAVE_STEP(1, v3_from_v1),
              NNH_SAVE_FIELD(gems, 4), NNH_SAVE_FIELD(items, 3), NNH_SAVE_FIELD(equipped, 5), NNH_SAVE_RETIRED(1, 2));

/// A "Load game" summary of the same key: the gems only, reading files up to version 3.
struct Summary {
    std::uint32_t gems = 0;
};
NNH_SAVE_SUMMARY(Summary, "mig", 3, NNH_SAVE_FIELD(gems, 4));

} // namespace mig
