// Mini-fuzz of `.nnhsprite` sprite libraries (spec 016 T6.3): two code-built libraries written as files, with 1-4
// bytes changed and sometimes cut, 4000 mutants each. The target fixes the size and CRC itself, so mutants reach the
// content checks; the accepted ones are copied and played (counted here to prove the deep path runs). The real
// fuzzing (libFuzzer + ASan, fuzz_sprite) runs on Linux.

#include <cstdint>

#include "../animation/sprite_file.hpp"
#include "../animation/library_fixture.hpp"
#include "animation/sprite_validate.hpp"
#include "fuzz_targets.hpp"
#include "nnh/memory.hpp"
#include "test.hpp"

namespace {

std::uint32_t next(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state >> 8;
}

} // namespace

NNH_TEST(sprite_library_target_survives_mutated_inputs) {
    const std::vector<std::uint8_t> seeds[] = {anim_test::sprite_file(anim_test::rich_library()),
                                               anim_test::sprite_file(anim_test::strip(3))};
    std::uint32_t state = 1616;
    int accepted = 0;
    for (const std::vector<std::uint8_t>& seed : seeds) {
        CHECK(nnh::detail::validate_sprite_library(seed.data(), seed.size()).ok());
        nnh::fuzz::sprite_library_target(seed.data(), seed.size());
        for (int i = 0; i < 4000; ++i) {
            std::vector<std::uint8_t> buffer = seed;
            if (i % 8 == 0) buffer.resize(next(state) % (seed.size() + 1));
            const int flips = 1 + static_cast<int>(next(state) % 4);
            for (int k = 0; k < flips && !buffer.empty(); ++k) {
                buffer[next(state) % buffer.size()] ^= static_cast<std::uint8_t>(1 + next(state) % 255);
            }
            nnh::fuzz::sprite_library_target(buffer.data(), buffer.size());
            if (buffer.size() < sizeof(nnh::detail::spritefmt::Header)) continue;
            anim_test::reseal(buffer);
            accepted += nnh::detail::validate_sprite_library(buffer.data(), buffer.size()).ok() ? 1 : 0;
        }
    }
    CHECK(accepted > 300);
    CHECK(nnh::mem::tag_stats(nnh::detail::animation_memory_tag()).count == 0); // every library was freed
    nnh::fuzz::sprite_library_target(nullptr, 0);
}
