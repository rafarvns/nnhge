// Must not compile: a field names a member the struct doesn't have (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>

#include "nnh/saves.hpp"

struct Hero {
    std::uint8_t damage = 0;
};
NNH_SAVE_TYPE(Hero, "hero", NNH_SAVE_FIELD(damgae, 1));
