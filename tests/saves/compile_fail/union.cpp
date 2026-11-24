// Must not compile: a union member (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>

#include "nnh/saves.hpp"

union Either {
    std::int32_t i;
    float f;
};
struct Hero {
    Either either{};
};
NNH_SAVE_TYPE(Hero, "hero", NNH_SAVE_FIELD(either, 1));
