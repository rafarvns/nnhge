// Must not compile: a std::string member (not trivially copyable) (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>
#include <string>
#include "nnh/saves.hpp"

struct Hero {
    std::string name;
};
NNH_SAVE_TYPE(Hero, "hero", NNH_SAVE_FIELD(name, 1));
