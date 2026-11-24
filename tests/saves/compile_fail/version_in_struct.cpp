// Must not compile: NNH_SAVE_VERSION in a save struct (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>

#include "nnh/saves.hpp"

struct Stats {
    std::uint8_t hp = 1;
};
NNH_SAVE_STRUCT(Stats, NNH_SAVE_VERSION(2), NNH_SAVE_FIELD(hp, 1));
