// Must not compile: NNH_SAVE_VERSION given twice (spec 027 T2.3, found by the T8.1 mutation pass).

#include <cstdint>

#include "nnh/saves.hpp"

struct Hero {
    std::int32_t level = 1;
};
inline void from_v1(Hero&, const nnh::save::OldFields&) {}
NNH_SAVE_TYPE(Hero, "hero", NNH_SAVE_VERSION(2), NNH_SAVE_VERSION(2), NNH_SAVE_STEP(1, from_v1), NNH_SAVE_FIELD(level, 1));
