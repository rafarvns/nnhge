// Mini-fuzz of the .nnhfont validator (spec 029 T2.3): a hand-built Latin font with kerning and outlines mutated 20 000
// times with the size and CRC fixed, so mutations reach the deep checks; the seed itself must be accepted.

#include <cstdio>
#include <vector>

#include "../font/font_fixture.hpp"
#include "fuzz_targets.hpp"
#include "test.hpp"

NNH_TEST(font_validate_target_survives_mutated_fonts) {
    font_test::FontBuilder b = font_test::FontBuilder::latin();
    b.kern(U'A', U'V', -1).kern(U'T', U'o', -2);
    b.glyph(0x20AC, 0, 40, 8, 9, 0, 2, 9).outline(16, 40, 10, 11, -1, 1);
    b.fallbacks = {"fonts/b"};
    const std::vector<std::uint8_t> seed = b.build();
    CHECK(nnh::fuzz::font_fixed_accepted(seed.data(), seed.size()));
    std::uint32_t state = 2029;
    int accepted = 0;
    std::vector<std::uint8_t> buf;
    for (int i = 0; i < 20000; ++i) {
        buf = seed;
        const int flips = 1 + static_cast<int>((state = state * 1664525u + 1013904223u) >> 30);
        for (int f = 0; f < flips; ++f) {
            state = state * 1664525u + 1013904223u;
            buf[(state >> 8) % buf.size()] ^= static_cast<std::uint8_t>(1u << (state & 7u));
        }
        if ((state & 0x300u) == 0) buf.resize(buf.size() - (state >> 12) % 16);
        nnh::fuzz::font_validate_target(buf.data(), buf.size());
        accepted += nnh::fuzz::font_fixed_accepted(buf.data(), buf.size()) ? 1 : 0;
    }
    std::printf("    %d of 20000 mutated fonts accepted\n", accepted);
    CHECK(accepted > 0);
}
