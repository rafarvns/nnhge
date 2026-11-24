// Must not compile: two fields with one id (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>

#include "nnh/saves.hpp"

struct Hero {
    std::int32_t level = 1;
    std::int32_t gold = 0;
};
NNH_SAVE_TYPE(Hero, "hero", NNH_SAVE_FIELD(level, 2), NNH_SAVE_FIELD(gold, 2));
