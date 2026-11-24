// Must not compile: a const member (the loader writes it) (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>

#include "nnh/saves.hpp"

struct Hero {
    const std::int32_t level = 1;
};
NNH_SAVE_TYPE(Hero, "hero", NNH_SAVE_FIELD(level, 1));
