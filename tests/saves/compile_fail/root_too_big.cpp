// Must not compile: a save type over 64 KiB (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>

#include "nnh/saves.hpp"

struct Hero {
    std::uint8_t a[40000] = {};
    std::uint8_t b[40000] = {};
};
NNH_SAVE_TYPE(Hero, "hero", NNH_SAVE_FIELD(a, 1), NNH_SAVE_FIELD(b, 2));
