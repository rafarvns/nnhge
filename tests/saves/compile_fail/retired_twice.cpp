// Must not compile: a retired id listed twice (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>

#include "nnh/saves.hpp"

struct Hero {
    std::int32_t level = 1;
};
NNH_SAVE_TYPE(Hero, "hero", NNH_SAVE_FIELD(level, 1), NNH_SAVE_RETIRED(4, 5), NNH_SAVE_RETIRED(4));
