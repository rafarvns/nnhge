// Must not compile: a gap in the migration chain (version 3, a step from 1 only) (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>

#include "nnh/saves.hpp"

struct Hero {
    std::int32_t level = 1;
};
inline void from_v1(Hero&, const nnh::save::OldFields&) {}
inline void from_v2(Hero&, const nnh::save::OldFields&) {}
NNH_SAVE_TYPE(Hero, "hero", NNH_SAVE_VERSION(3), NNH_SAVE_STEP(1, from_v1), NNH_SAVE_FIELD(level, 1));
