// Must not compile: a pointer member (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>

#include "nnh/saves.hpp"

struct Hero {
    std::int32_t* target = nullptr;
};
NNH_SAVE_TYPE(Hero, "hero", NNH_SAVE_FIELD(target, 1));
