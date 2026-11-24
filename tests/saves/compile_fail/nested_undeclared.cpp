// Must not compile: a struct member without its own NNH_SAVE_STRUCT (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>

#include "nnh/saves.hpp"

struct Stats {
    std::uint8_t hp = 1;
};
struct Hero {
    Stats stats;
};
NNH_SAVE_TYPE(Hero, "hero", NNH_SAVE_FIELD(stats, 1));
