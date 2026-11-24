// Must not compile: field id 65536 (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>

#include "nnh/saves.hpp"

struct Hero {
    std::int32_t level = 1;
};
NNH_SAVE_TYPE(Hero, "hero", NNH_SAVE_FIELD(level, 65536));
