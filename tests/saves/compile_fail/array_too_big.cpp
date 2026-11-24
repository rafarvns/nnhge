// Must not compile: an array of 65536 items (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>

#include "nnh/saves.hpp"

struct Hero {
    std::uint8_t a[65536] = {};
};
NNH_SAVE_TYPE(Hero, "hero", NNH_SAVE_FIELD(a, 1));
