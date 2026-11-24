// Must not compile: storing a summary (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>

#include "nnh/saves.hpp"

struct Hero {
    std::int32_t level = 1;
    std::int32_t gold = 0;
};
NNH_SAVE_TYPE(Hero, "hero", NNH_SAVE_FIELD(level, 1), NNH_SAVE_FIELD(gold, 2));
struct HeroSummary {
    std::int32_t level = 1;
};
NNH_SAVE_SUMMARY(HeroSummary, "hero", 1, NNH_SAVE_FIELD(level, 1));
nnh::save::Buffer<Hero> g_buffer;
void save(const HeroSummary& s) { (void)nnh::save::store("hero.sav", s, g_buffer.bytes, sizeof g_buffer.bytes); }
