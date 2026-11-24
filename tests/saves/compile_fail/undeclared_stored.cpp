// Must not compile: storing a struct without a save declaration (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>

#include "nnh/saves.hpp"

struct Hero {
    std::int32_t level = 1;
};
std::uint8_t g_buffer[64];
void save(const Hero& h) { (void)nnh::save::store("hero.sav", h, g_buffer, sizeof g_buffer); }
