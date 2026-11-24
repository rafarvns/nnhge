// Must not compile: a char member (its sign differs between systems) (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>

#include "nnh/saves.hpp"

struct Hero {
    char initial = 'a';
};
NNH_SAVE_TYPE(Hero, "hero", NNH_SAVE_FIELD(initial, 1));
